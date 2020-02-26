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

  virtual ReturnCode
  addRecord(const std::string& recordName, const std::string& payload);

  virtual ReturnCode
  getRecord(const Name& recordName, Record& record);

  virtual bool
  checkRecord(const Name& recordName);

  virtual void
  setOnRecordAppLogic(const OnNewRecord& onNewRecord);
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_LEDGER_H_