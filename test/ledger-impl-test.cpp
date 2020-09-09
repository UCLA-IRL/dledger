#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>


#include <ndn-cxx/util/io.hpp>

using namespace dledger;

std::list<std::string> startingPeerPath({
                                                "./test-certs/test-a.cert",
                                                "./test-certs/test-b.cert",
                                                "./test-certs/test-c.cert",
                                                "./test-certs/test-d.cert"
                                        });

void periodicAddRecord(shared_ptr<Ledger> ledger, Scheduler& scheduler) {
    Record record(RecordType::GENERIC_RECORD, std::string("Test_") + std::to_string(std::rand()));
    security::KeyChain keychain;
    const auto& pib = keychain.getPib();
    const auto& identity = pib.getIdentity("/example");
    const auto& key = identity.getDefaultKey();
    security::v2::Certificate newCert;

    Name certName = key.getName();
    certName.append("test-dledger").append(std::to_string(std::rand()));
    newCert.setName(certName);
    newCert.setContent(key.getDefaultCertificate().getContent());
    SignatureInfo signatureInfo;
    signatureInfo.setValidityPeriod(security::ValidityPeriod(time::system_clock::now(), time::system_clock::now() + time::days(1)));
    security::SigningInfo signingInfo(security::SigningInfo::SIGNER_TYPE_ID,
                                      "/example", signatureInfo);
    newCert.setFreshnessPeriod(time::days(1));
    keychain.sign(newCert, signingInfo);
    record.addRecordItem(newCert.wireEncode());
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    // schedule for the next record generation
    scheduler.schedule(time::seconds(5), [ledger, &scheduler] { periodicAddRecord(ledger, scheduler); });
}

int
main(int argc, char** argv)
{
  if (argc < 2) {
      fprintf(stderr, "Usage: %s id_name\n", argv[0]);
      return 1;
  }
  std::string idName = argv[1];
  boost::asio::io_service ioService;
  Face face(ioService);
  security::KeyChain keychain;
  std::shared_ptr<Config> config = nullptr;
  try {
    config = Config::CustomizedConfig("/ndn/multicast/CA-dledger", "/ndn/dledger/" + idName,
            std::string("./dledger-anchor.cert"), std::string("/tmp/dledger-db/" + idName),
                                      startingPeerPath);
    mkdir("/tmp/dledger-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  catch(const std::exception& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face));
  ledger->setOnRecordAppCheck([&](const Data& r) {
      Record record = r;
      if (record.getType() == GENERIC_RECORD) {
          bool hasName = false;
          bool hasCert = false;
          for (const auto &item : record.getRecordItems()) {
              if (item.type() == tlv::Name) {
                  if (hasName) return false;
                  if (security::v2::Certificate::isValidName(Name(item))) {
                      hasName = true;
                  } else {
                      return false;
                  }
              } else if (item.type() == tlv::Data) {
                  if (hasCert) return false;
                  try {
                      auto c = security::v2::Certificate(item);
                      hasCert = true;
                  } catch (std::exception &e) {
                      return false;
                  }
              } else {
                  return false;
              }
          }
          if (hasCert && hasName) return false;
          if (!hasCert && !hasName) return false;
      }
      return true;
  });

  Scheduler scheduler(ioService);
  periodicAddRecord(ledger, scheduler);

  face.processEvents();
  return 0;
}