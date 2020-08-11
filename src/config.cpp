#include "dledger/config.hpp"
#include "default-cert-manager.h"

#include <cstdlib>
#include <ctime>
#include <ndn-cxx/util/io.hpp>
#include <utility>

namespace dledger {

static const std::string DEFAULT_ANCHOR_CERT_PATH = "/dledger/dledger-anchor.cert";
static const std::string DEFAULT_MULTICAST_PREFIX = "/dledger-multicast";
static const std::string DEFAULT_PEER_PREFIX = "/dledger";

shared_ptr<Config>
Config::DefaultConfig()
{
  std::srand(std::time(nullptr));
  auto peerPrefix = DEFAULT_PEER_PREFIX + "/" + std::to_string(std::rand());
  std::string homePath = std::getenv("HOME");
  auto trustAnchorCert = io::load<security::v2::Certificate>(homePath + DEFAULT_ANCHOR_CERT_PATH);
  if (trustAnchorCert == nullptr) {
    BOOST_THROW_EXCEPTION(std::runtime_error("Cannot load anchor certificate from the default path."));
  }
  auto config = std::make_shared<Config>(DEFAULT_MULTICAST_PREFIX, peerPrefix,
                                         make_shared<DefaultCertificateManager>(peerPrefix, trustAnchorCert, std::list<security::v2::Certificate>()));
  config->databasePath = "/tmp/dledger-db/" + readString(config->peerPrefix.get(-1));
  return config;
}

shared_ptr<Config>
Config::CustomizedConfig(const std::string& multicastPrefix, const std::string& producerPrefix,
        const std::string& anchorCertPath, const std::string& databasePath, const std::list<std::string> &startingPeerPaths)
{
  auto trustAnchorCert = io::load<security::v2::Certificate>(anchorCertPath);
  if (trustAnchorCert == nullptr) {
    BOOST_THROW_EXCEPTION(std::runtime_error("Cannot load anchor certificate from the designated path."));
  }
  std::cout << "Trust Anchor: " << trustAnchorCert->getName().toUri() << std::endl;

  //starting peers
  std::list<security::v2::Certificate> startingPeerCerts;
  for (const auto& path : startingPeerPaths) {
      auto cert = io::load<security::v2::Certificate>(path);
      if (cert == nullptr) {
          BOOST_THROW_EXCEPTION(std::runtime_error("Cannot load starting certificate from the designated path."));
      }
      std::cout << "Starting Peer: " << cert->getName().toUri() << std::endl;
      startingPeerCerts.push_back(*cert);
  }
  auto config = std::make_shared<Config>(multicastPrefix, producerPrefix,
          make_shared<DefaultCertificateManager>(producerPrefix, trustAnchorCert, startingPeerCerts));
  config->databasePath = databasePath;
  return config;
}

Config::Config(const std::string& multicastPrefix, const std::string& peerPrefix, shared_ptr<CertificateManager> certificateManager_)
    : multicastPrefix(multicastPrefix),
      peerPrefix(peerPrefix),
      certificateManager(std::move(certificateManager_))
{}

}  // namespace dledger