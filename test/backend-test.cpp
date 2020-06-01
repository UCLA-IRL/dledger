#include "backend.hpp"
#include <ndn-cxx/name.hpp>
#include <iostream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>

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
testBackEnd()
{
  Backend backend;
  auto data = makeData("/dledger/12345", "content is 12345");
  auto fullName = data->getFullName();

  backend.putRecord(data);
  auto anotherRecord = backend.getRecord(fullName);
  if (data == nullptr || anotherRecord == nullptr) {
      return false;
  }
  return data->wireEncode() == anotherRecord->wireEncode();
}

bool
testNameGet()
{
  std::string name1 = "name1";
  ndn::Name name2("/dledger/name1/123");
  if (name2.get(-2).toUri() == name1) {
    return true;
  }
  return false;
}

int
main(int argc, char** argv)
{
  auto success = testBackEnd();
  if (!success) {
    std::cout << "testBackEnd failed" << std::endl;
  }
  else {
    std::cout << "testBackEnd with no errors" << std::endl;
  }
  success = testNameGet();
  if (!success) {
    std::cout << "testNameGet failed" << std::endl;
  }
  else {
    std::cout << "testNameGet with no errors" << std::endl;
  }
  return 0;
}