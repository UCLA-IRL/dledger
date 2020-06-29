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

typedef function<bool(const Data&)> OnRecordAppCheck;
typedef function<void(const Record&)> OnRecordAppAccepted;

class Ledger {
public:
  /**
   * Initialize a DLedger instance from the config.
   * @p config, input, the configuration of multicast prefix, peer prefix, and settings of Dledger behavior
   * @p keychain, input, the local NDN keychain instance
   * @p face, input, the localhost NDN face to send/receive NDN packets.
   */
  static std::unique_ptr<Ledger>
  initLedger(const Config& config, security::KeyChain& keychain, Face& face);

  Ledger() = default;
  virtual ~Ledger() = default;

  /**
   * Create a new record to the Dledger.
   * @p record, input, a record instance which contains the record payload
   */
  virtual ReturnCode
  createRecord(Record& record) = 0;

  /**
   * Get an existing record from the Dledger.
   * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
   */
  virtual optional<Record>
  getRecord(const std::string& recordName) const = 0;

  /**
   * Check whether the record exists in the Dledger.
   * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
   */
  virtual bool
  hasRecord(const std::string& recordName) const = 0;

  /**
    * list the record exists in the Dledger.
    * @p recordName, input, the name of the record, which is an NDN name prefix.
    */
  virtual std::list<Name>
  listRecord(const std::string& prefix) const = 0;

  /**
   * Set additional checking rules when receiving a new record.
   * @p onRecordAppCheck, input, a callback function invoked whenever there is a new record received from the Internet.
   */
  virtual void
  setOnRecordAppCheck(const OnRecordAppCheck& onRecordAppCheck) {
      m_onRecordAppCheck = onRecordAppCheck;
  }

  /**
    * Set additional action when a new record is accepted.
    * @p onRecordAppCheck, input, a callback function invoked whenever there is a new record is accepted.
    */
  virtual void
  setOnRecordAppAccepted(const OnRecordAppAccepted& onRecordAppAccepted) {
      m_onRecordAppAccepted = onRecordAppAccepted;
  }

protected:
  OnRecordAppCheck m_onRecordAppCheck;
  OnRecordAppAccepted m_onRecordAppAccepted;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_LEDGER_H_