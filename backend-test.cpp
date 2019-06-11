#include "backend.hpp"
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>

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

int
main(int argc, char** argv)
{
  DLedger::Backend backend;
  auto data = makeData("/dledger/12345", "content is 12345");
  DLedger::LedgerRecord record(data);

  backend.putRecord(record);
  auto anotherRecord = backend.getRecord("/dledger/12345");
  if (record.m_data != anotherRecord.m_data) {
    std::cout << "something goes wrong!!" << std::endl;
  }
  else {
    ndn::Data data2(record.m_data);
    std::cout << "fetched record id: " << data2.getName().toUri() << std::endl;
    std::cout << "fetched record content: " << ndn::readString(data2.getContent()) << std::endl;
    std::cout << "no failures" << std::endl;
  }
  return 0;
}