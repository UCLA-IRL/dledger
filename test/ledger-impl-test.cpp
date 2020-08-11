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

std::shared_ptr<ndn::Data>
makeData(const std::string& name, const std::string& content)
{
  using namespace ndn;
  using namespace std;
  auto data = make_shared<Data>(ndn::Name(name));
  data->setContent((const uint8_t*)content.c_str(), content.size());
  ndn::SignatureSha256WithRsa fakeSignature;
  fakeSignature.setValue(ndn::encoding::makeEmptyBlock(tlv::SignatureValue));
  data->setSignature(fakeSignature);
  data->wireEncode();
  return data;
}

void periodicAddRecord(shared_ptr<Ledger> ledger, Scheduler& scheduler) {
    Record record(RecordType::GENERIC_RECORD, std::to_string(std::rand()));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    // schedule for the next record generation
    scheduler.schedule(time::seconds(5), [ledger, &scheduler] { periodicAddRecord(ledger, scheduler); });
}

std::string addRevokeRecord(security::KeyChain& keychain, shared_ptr<Ledger> ledger, const std::string& id) {
    RevocationRecord record(std::to_string(std::rand()));
    auto recordList = ledger->listRecord("/dledger/test-anchor/Cert");
    std::list<Name> certNames;
    for (const auto& certRecordName: recordList) {
        std::cout << "FOUND CERT RECORD " << certRecordName << std::endl;
        auto fetchResult = ledger->getRecord(certRecordName.toUri());
        assert(fetchResult.has_value());
        CertificateRecord certRecord(*fetchResult);
        for (const auto& cert: certRecord.getCertificates()) {
            std::cout << "- FOUND CERTIFICATE " << cert.getFullName() << std::endl;
            if (cert.getKeyName().toUri().find(id) != std::string::npos) {
                record.addCertificateNameItem(cert.getFullName());
                std::cout << "- SELECTED" << std::endl;
            }
        }
    }

    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    return result.what();
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
    config = Config::CustomizedConfig("/dledger-multicast", "/dledger/" + idName,
            std::string("./dledger-anchor.cert"), std::string("/tmp/dledger-db/" + idName),
                                      startingPeerPath);
    mkdir("/tmp/dledger-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  }
  catch(const std::exception& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face));

  Scheduler scheduler(ioService);
  periodicAddRecord(ledger, scheduler);
  if (idName == "test-2a") {
      scheduler.schedule(time::seconds(45),
                         [ledger, &keychain, idName] { addRevokeRecord(keychain, ledger, idName); });
      scheduler.schedule(time::seconds(45) + time::milliseconds(800),
                         [ledger, &keychain, idName] { addRevokeRecord(keychain, ledger, idName); });
  }

  face.processEvents(time::seconds(180));
  return 0;
}