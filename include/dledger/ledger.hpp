#ifndef DLEDGER_INCLUDE_LEDGER_H_
#define DLEDGER_INCLUDE_LEDGER_H_

#include <iostream>
#include <optional>
#include <ndn-cxx/name.hpp>
#include "dledger/record.hpp"
#include "dledger/config.hpp"
#include "dledger/return-code.hpp"

using namespace ndn;
namespace dledger {

typedef function<bool(const Data&)> OnRecordAppLogic;

class Ledger {
public:
  /**
   * Initialize a DLedger instance from the config.
   * @p config, input, the configuration of multicast prefix, peer prefix, and settings of Dledger behavior
   * @p keychain, input, the local NDN keychain instance
   * @p face, input, the localhost NDN face to send/receive NDN packets.
   */
  static std::unique_ptr<Ledger>
  initLedger(const Config& config, security::KeyChain& keychain, Face& face, std::string id);

  Ledger() = default;
  virtual ~Ledger() = default;

  /**
   * Create a new record to the Dledger.
   * @p record, input, the record instance which contains the record payload
   */
  virtual ReturnCode
  addRecord(Record& record) = 0;

  /**
   * Get an existing record from the Dledger.
   * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
   */
  virtual optional<Record>
  getRecord(const std::string& recordName) = 0;

  /**
   * Check whether the record exists in the Dledger.
   * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
   */
  virtual bool
  hasRecord(const std::string& recordName) = 0;

  /**
   * Set additional checking rules when receiving a new record.
   * @p onRecordAppLogic, input, a callback function invoked whenever there is a new record received from the Internet.
   */
  virtual void
  setOnRecordAppLogic(const OnRecordAppLogic& onRecordAppLogic) {
    m_onRecordAppLogic = onRecordAppLogic;
  }

private:
  OnRecordAppLogic m_onRecordAppLogic;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_LEDGER_H_