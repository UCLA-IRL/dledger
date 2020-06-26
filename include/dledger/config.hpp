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
  static shared_ptr<Config> CustomizedConfig(const std::string& multicastPrefix, const std::string& peerPrefix,
          const std::string& anchorCertPath, const std::string& databasePath);

  /**
   * Construct a Config instance used for DLedger initialization.
   * @p multicastPrefix, input, the distributed ledger system's multicast prefix.
   * @p peerPrefix, input, the unique prefix of the peer.
   */
  Config(const std::string& multicastPrefix, const std::string& peerPrefix);

public:
  /**
   * The number of preceding records that referenced by a later record.
   */
  int precedingRecordNum = 2;
  /**
   * The maximum weight of record that can be referenced.
   */
  int appendWeight = 1;
  /**
   * The maximum weight of record that can be allowed.
   */
  int contributionWeight = 2;
  /**
   * The weight of record that can be confirmed and be appended without contribution policy.
   */
  int confirmWeight = 2;
  /**
   * The multicast prefix, under which an Interest can reach to all the peers in the same multicast group.
   */
  Name multicastPrefix;
  /**
   * Producer's unique name prefix, under which an Interest can reach to the producer.
   */
  Name peerPrefix;
  /**
   * The trust anchor certificate of the whole distributed ledger system.
   * The identity should be another peer.
   */
  std::shared_ptr<security::v2::Certificate> trustAnchorCert;
  /**
   * The path to the Database;
   */
   std::string databasePath;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_CONFIG_H_