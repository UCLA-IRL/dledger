#include "dledger/config.hpp"

#include <cstdlib>
#include <ctime>
#include <ndn-cxx/util/io.hpp>

namespace dledger {

static const std::string DEFAULT_ANCHOR_CERT_PATH = "/dledger/dledger-anchor.cert";
static const std::string DEFAULT_MULTICAST_PREFIX = "/dledger-multicast";
static const std::string DEFAULT_PEER_PREFIX = "/dledger";

shared_ptr<Config>
Config::DefaultConfig()
{
  std::srand(std::time(nullptr));
  auto config = std::make_shared<Config>(DEFAULT_MULTICAST_PREFIX, DEFAULT_PEER_PREFIX + "/" + std::to_string(std::rand()));
  std::string homePath = std::getenv("HOME");
  config->trustAnchorCert = io::load<security::v2::Certificate>(homePath + DEFAULT_ANCHOR_CERT_PATH);
  if (config->trustAnchorCert == nullptr) {
    BOOST_THROW_EXCEPTION(std::runtime_error("Cannot load anchor certificate from the default path."));
  }
  config->databasePath = "/tmp/dledger-db/" + readString(config->peerPrefix.get(-1));
  return config;
}

shared_ptr<Config>
Config::CustomizedConfig(const std::string& multicastPrefix, const std::string& producerPrefix,
        const std::string& anchorCertPath, const std::string& databasePath)
{
  auto config = std::make_shared<Config>(multicastPrefix, producerPrefix);
  config->trustAnchorCert = io::load<security::v2::Certificate>(anchorCertPath);
  if (config->trustAnchorCert == nullptr) {
    std::cout << "couldn't load anchor certificate \n";
    BOOST_THROW_EXCEPTION(std::runtime_error("Cannot load anchor certificate from the designated path."));
  }
  std::cout << config->trustAnchorCert->getName().toUri() << std::endl;
  config->databasePath = databasePath;
  return config;
}

Config::Config(const std::string& multicastPrefix, const std::string& peerPrefix)
    : multicastPrefix(multicastPrefix),
      peerPrefix(peerPrefix)
{
    BOOST_ASSERT(appendWeight <= contributionWeight);
}

}  // namespace dledger