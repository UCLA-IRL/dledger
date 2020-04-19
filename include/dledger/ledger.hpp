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
  static std::unique_ptr<Ledger> initLedger(const Config& config, security::KeyChain& keychain, Face& face);

  Ledger() = default;
  virtual ~Ledger() = default;

  virtual ReturnCode
  addRecord(Record& record, const Name& signerIdentity) = 0;

  virtual optional<Record>
  getRecord(const std::string& recordName) = 0;

  virtual bool
  hasRecord(const std::string& recordName) = 0;

  virtual void
  setOnRecordAppLogic(const OnRecordAppLogic& onRecordAppLogic) {
    m_onRecordAppLogic = onRecordAppLogic;
  }

private:
  OnRecordAppLogic m_onRecordAppLogic;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_LEDGER_H_