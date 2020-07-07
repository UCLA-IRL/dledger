//
// Created by Tyler on 6/26/20.
//

#ifndef DLEDGER_CERT_LIST_H
#define DLEDGER_CERT_LIST_H


#include <map>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/security/v2/certificate.hpp>
#include <list>
#include <unordered_set>
#include <dledger/config.hpp>

using namespace ndn;
namespace dledger {
    class CertList {
    public:
        CertList(const Config& config);
        void insert(const security::v2::Certificate& certificate);
        bool verifySignature(const Data& data) const;
        bool verifySignature(const Interest& interest) const;
        void revoke(const Name& certificateName);
        bool authorizedToGenerate() const;

        void setLastCertRecord(const Name& certName);
        const Name& getLastCertRecord() const;

        Name getCertificateNameIdentity(const Name& certificateName) const;

    private:
        const Config& m_config;
        std::map<Name, std::list<security::v2::Certificate>> m_peerCertificates; // first: name of the peer, second: certificate
        std::unordered_set<Name> m_revokedCertificates;
        Name m_lastCertRecord;
    };
}


#endif //DLEDGER_CERT_LIST_H
