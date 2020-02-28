#ifndef DLEDGER_INCLUDE_LEDGER_H_
#define DLEDGER_INCLUDE_LEDGER_H_

#include <iostream>
#include <ndn-cxx/name.hpp>
#include "dledger/record.hpp"
#include "dledger/return-code.hpp"

using namespace ndn;
namespace dledger {

typedef function<bool(const Data&)> OnNewRecord;

class Ledger {
public:
  Ledger() = default;
  virtual ~Ledger() = default;

  virtual ReturnCode
  addRecord(const std::string& recordIdentifier, Record& record, const Name& signerIdentity) = 0;

  virtual ReturnCode
  getRecord(const std::string& recordName, Record& record) = 0;

  virtual bool
  checkRecord(const std::string& recordName) = 0;

  virtual void
  setOnRecordAppLogic(const OnNewRecord& onNewRecord) = 0;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_LEDGER_H_