#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>


#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <array>

using namespace dledger;

std::list<std::string> startingPeerPath({
                                                "./test-1b.cert",
                                                "./test-1a.cert",
                                                "./test-1c.cert",
                                                "./test-1d.cert",
                                                "./test-1e.cert"
                                        });

std::array<std::string, 5> peerList2 = {
        "/dledger/test-2b",
        "/dledger/test-2a",
        "/dledger/test-2c",
        "/dledger/test-2d",
        "/dledger/test-2e"
};

std::array<std::string, 5> peerList3 = {
        "/dledger/test-2a",
        "/dledger/test-1b",
        "/dledger/test-2a",
        "/dledger/test-1b",
        "/dledger/test-1a",
};

std::string anchorName = "/dledger/test-anchor";

std::string addCertificateRecord(security::KeyChain& keychain, shared_ptr<Ledger> ledger, const std::array<std::string, 5>& peerList) {
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
    return result.what();
}

std::string addRevokeRecord(security::KeyChain& keychain, shared_ptr<Ledger> ledger, std::string certRecordName, std::list<std::string> startPeerPaths) {
    std::cout << "Making revoke record" << std::endl;
    RevocationRecord record(std::to_string(std::rand()));
    auto fetchResult = ledger->getRecord(certRecordName);
    assert(fetchResult.has_value());
    auto certRecord = CertificateRecord(*fetchResult);
    record.addCertificateNameItem(certRecord.getCertificates().begin()->getFullName());
    std::cout << "Making revoke record 1/2" << std::endl;
    auto startingPeerCert = io::load<security::v2::Certificate>(*startPeerPaths.begin());
    record.addCertificateNameItem(startingPeerCert->getFullName());
    std::cout << "Making revoke record 2/2" << std::endl;

    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    return result.what();
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
                                          std::string("./dledger-anchor.cert"), std::string("/tmp/dledger-db/test-anchor"),
                                          startingPeerPath);
        mkdir("/tmp/dledger-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    catch(const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face));

    auto recordName = addCertificateRecord(keychain, ledger, peerList2);
    Scheduler scheduler(ioService);
    scheduler.schedule(time::seconds(45),
            [ledger, &keychain, recordName]{addRevokeRecord(keychain, ledger, recordName, startingPeerPath);});
    scheduler.schedule(time::seconds(130),
                       [ledger, &keychain]{addCertificateRecord(keychain, ledger, peerList3);});

    face.processEvents(time::seconds(180));
    return 0;
}