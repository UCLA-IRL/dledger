//
// Created by Tyler on 8/8/20.
//

#include <iostream>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include "default-cert-manager.h"

dledger::DefaultCertificateManager::DefaultCertificateManager(const Name &peerPrefix,
                                                              shared_ptr<security::v2::Certificate> anchorCert,
                                                              const std::list<security::v2::Certificate> &startingPeers)
        :
        m_peerPrefix(peerPrefix), m_anchorCert(std::move(anchorCert)) {
    if (m_peerPrefix.size() != m_anchorCert->getIdentity().size()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("trust Anchor should follow the peer prefix format"));
    }
    if (!m_anchorCert->isValid()) {
        BOOST_THROW_EXCEPTION(std::runtime_error("trust Anchor Expired"));
    }
    m_peerCertificates[m_anchorCert->getIdentity()].push_back(*m_anchorCert);
    for (const auto &certificate: startingPeers) {
        m_peerCertificates[certificate.getIdentity()].push_back(certificate);
    }
}

bool dledger::DefaultCertificateManager::verifySignature(const Data &data) const {
    auto identity = data.getName().getPrefix(m_peerPrefix.size());
    auto iterator = m_peerCertificates.find(identity);
    if (iterator == m_peerCertificates.cend()) return false;
    for (const auto &cert : iterator->second) {
        if (security::verifySignature(data, cert)) {
            return true;
        }
    }
    return false;
}

bool dledger::DefaultCertificateManager::verifyRecordFormat(const dledger::Record &record) const {

    if (record.getType() == RecordType::CERTIFICATE_RECORD) {
        if (!m_anchorCert->getIdentity().isPrefixOf(record.getRecordName())) {
            std::cout << "-- Certificate Record from bad person." << std::endl;
            return false;
        }
        try {
            auto certRecord = CertificateRecord(record);
            for (const auto &cert: certRecord.getCertificates()) {
                if (!security::verifySignature(cert, *m_anchorCert)) {
                    std::cout << "-- invalid certificate: " << cert.getName() << std::endl;
                    return false;
                }
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad certificate record format. " << std::endl;
            return false;
        }
    } else if (record.getType() == RecordType::REVOCATION_RECORD) {
        try {
            auto revokeRecord = RevocationRecord(record);
            bool isAnchor = readString(m_anchorCert->getIdentity().get(-1)) == revokeRecord.getProducerID();
            for (const auto &certName: revokeRecord.getRevokedCertificates()) {
                if (!certName.get(-1).isImplicitSha256Digest() ||
                    !security::v2::Certificate::isValidName(certName.getPrefix(-1))) {
                    std::cout << "-- invalid revoked certificate: " << certName << std::endl;
                    return false;
                }
                if (!isAnchor &&
                    readString(getCertificateNameIdentity(certName).get(-1)) != revokeRecord.getProducerID()) {
                    std::cout << "-- invalid revoked of other's certificate: " << certName << std::endl;
                    return false;
                }
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad revocation record format. " << std::endl;
            return false;
        }
    } else {
        std::cout << "-- Not a certificate/revocation record" << std::endl;
    }

    return true;
}

bool dledger::DefaultCertificateManager::endorseSignature(const Data &data) const {
    auto identity = data.getName().getPrefix(m_peerPrefix.size());
    auto iterator = m_peerCertificates.find(identity);
    if (iterator == m_peerCertificates.cend()) return false;
    for (const auto &cert : iterator->second) {
        if (m_revokedCertificates.count(cert.getFullName())) continue;
        if (security::verifySignature(data, cert)) {
            return true;
        }
    }
    return false;
}

bool dledger::DefaultCertificateManager::verifySignature(const Interest &interest) const {
    SignatureInfo info(interest.getName().get(-2).blockFromValue());
    auto identity = info.getKeyLocator().getName().getPrefix(-2);
    auto iterator = m_peerCertificates.find(identity);
    if (iterator == m_peerCertificates.cend()) return false;
    for (const auto &cert : iterator->second) {
        if (m_revokedCertificates.count(cert.getFullName())) continue;
        if (security::verifySignature(interest, cert)) {
            return true;
        }
    }
    return false;
}

void dledger::DefaultCertificateManager::acceptRecord(const dledger::Record &record) {
    if (record.getType() == RecordType::CERTIFICATE_RECORD) {
        try {
            auto certRecord = CertificateRecord(record);
            for (const auto &cert: certRecord.getCertificates()) {
                if (m_revokedCertificates.count(cert.getFullName()))
                    continue;
                std::cout << "Insert certificate " << cert.getName() << std::endl;
                m_peerCertificates[cert.getIdentity()].push_back(cert);
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad certificate record format. " << std::endl;
            return;
        }
    } else if (record.getType() == RecordType::REVOCATION_RECORD) {
        try {
            auto revokeRecord = RevocationRecord(record);
            for (const auto &certName: revokeRecord.getRevokedCertificates()) {
                std::cout << "Revoke certificate " << certName << std::endl;
                m_revokedCertificates.insert(certName);
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad revocation record format. " << std::endl;
            return;
        }
    }
}

Name dledger::DefaultCertificateManager::getCertificateNameIdentity(const Name &certificateName) const {
    return certificateName.getPrefix(m_peerPrefix.size());
}

bool dledger::DefaultCertificateManager::authorizedToGenerate() const {
    auto iterator = m_peerCertificates.find(m_peerPrefix);
    if (iterator == m_peerCertificates.cend()) return false;
    return !iterator->second.empty();
}
