#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include "ledger-impl.hpp"
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>


#include <ndn-cxx/util/io.hpp>

using namespace dledger;

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

bool
testGenData(std::string signerId)
{
 //Make config with Config::Config(const std::string& multicastPrefix, const std::string& peerPrefix)
 //a keychain 
 //a Face
 //this makes a ledgerImpl, then have it produce data, send out sync requests periodically
 //that's really it


  // auto ledger = Ledger::initLedger(*config, keychain, face, signerId);
  // //construct a record
  // std::cout << "initialization of ledger worked \n";
  // std::cout << "processing events \n";
  // face.processEvents();
  // return true;
}

int
main(int argc, char** argv)
{
  std::string idName = argv[1];
  Face face;
  security::KeyChain keychain;
  std::shared_ptr<Config> config = nullptr;
  try {
    config = Config::DefaultConfig();
  }
  catch(const std::exception& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }
  std::cout << "aaaa" << std::endl;

  auto ledger = Ledger::initLedger(*config, keychain, face, idName);
  face.processEvents();

  Record record(RecordType::GenericRecord, "record-1");
  ledger->addRecord(record);

  // auto success = testGenData(idName);
  // if (!success) {
  //   std::cout << "ledgerimp generate data failed" << std::endl;
  // }
  // else {
  //   std::cout << "ledgerimp ex with no errors" << std::endl;
  // }
  // success = testSyncInterest();
  // if (!success) {
  //   std::cout << "ledgerimp generate syncinterest failed" << std::endl;
  // }
  // else {
  //   std::cout << "syncint with no errors" << std::endl;
  // }
  return 0;
}