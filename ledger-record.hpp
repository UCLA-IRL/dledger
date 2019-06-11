#ifndef DLEDGER_RECORD_H
#define DLEDGER_RECORD_H

#include <iostream>
#include <set>
#include <ndn-cxx/data.hpp>

namespace DLedger {

// Ledger record class
// The main program needs to keep <id, RecordState> in mem
// This class is a reflection of what is kept in the database
class LedgerRecord
{
public:
  LedgerRecord();
  LedgerRecord(std::shared_ptr<const ndn::Data> data);

public:
  // key of the ledger
  std::string m_id;
  // record bytes
  ndn::Block m_data;
};

// The state of each unconfirmed record
struct RecordState
{
  std::string m_id = "";
  std::string m_producer = "";
  int endorse = 0;
  bool isConfirmed = false;
  std::set<std::string> approvers;
};

} // namespace DLedger

#endif // DLEDGER_RECORD_H
