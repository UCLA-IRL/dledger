#ifndef DLEDGER_INCLUDE_CONFIG_H_
#define DLEDGER_INCLUDE_CONFIG_H_

#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

using namespace ndn;
namespace dledger {

class Config
{
public:
  static shared_ptr<Config> DefaultConfig();
  static shared_ptr<Config> CustomizedConfig(const std::string& multicastPrefix, const std::string& producerPrefix, const std::string anchorCertPath);

  Config(const std::string& multicastPrefix, const std::string& producerPrefix);

public:
  int preceidingRecordNum = 2;
  Name multicastPrefix;
  Name producerPrefix;
  std::shared_ptr<security::v2::Certificate> trustAnchorCert;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_CONFIG_H_