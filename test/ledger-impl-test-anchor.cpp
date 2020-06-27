#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include "ledger-impl.hpp"
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>


#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>

using namespace dledger;

std::string peerList[] = {
        "/dledger/test-a",
        "/dledger/test-b",
        "/dledger/test-c",
        "/dledger/test-d",
        "/dledger/test-e",
};

std::string anchorName = "/dledger/test-anchor";

void addCertificateRecord(security::KeyChain& keychain, shared_ptr<Ledger> ledger) {
    CertificateRecord record(std::to_string(std::rand()));
    const auto& pib = keychain.getPib();

    for (const std::string& peer : peerList) {
        const auto& identity = pib.getIdentity(peer);
        const auto& key = identity.getDefaultKey();
        security::v2::Certificate newCert;

        Name certName = key.getName();
        certName.append("test-anchor").append(std::to_string(std::rand()));
        newCert.setName(certName);
        newCert.setContent(key.getDefaultCertificate().getContent());
        SignatureInfo signatureInfo;
        signatureInfo.setValidityPeriod(security::ValidityPeriod(time::system_clock::now(), time::system_clock::now() + time::days(3)));
        security::SigningInfo signingInfo(security::SigningInfo::SIGNER_TYPE_ID,
                                          anchorName, signatureInfo);
        newCert.setFreshnessPeriod(time::days(1));
        keychain.sign(newCert, signingInfo);

        record.addCertificateItem(newCert);
    }
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }
}

int
main(int argc, char** argv)
{
    boost::asio::io_service ioService;
    Face face(ioService);
    security::KeyChain keychain;
    std::shared_ptr<Config> config = nullptr;
    try {
        config = Config::CustomizedConfig("/dledger-multicast", anchorName,
                                          std::string("./dledger-anchor.cert"), std::string("/tmp/dledger-db/test-anchor"));
    }
    catch(const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face, "test-anchor"));

    if (!config->trustAnchorCert->isValid()) {
        std::cout << "Anchor certificate expired. " << std::endl;
    }

    addCertificateRecord(keychain, ledger);

    face.processEvents();
    return 0;
}