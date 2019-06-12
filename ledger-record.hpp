#ifndef DLEDGER_RECORD_H
#define DLEDGER_RECORD_H

#include <iostream>
#include <set>
#include <vector>
#include <ndn-cxx/data.hpp>

namespace DLedger {

// Ledger record class
// The main program needs to keep <id, RecordState> in mem
// This class is a reflection of what is kept in the database
class LedgerRecord
{
public:
  LedgerRecord() = default;
  LedgerRecord(std::shared_ptr<const ndn::Data> data);
  LedgerRecord(const RecordState& state);

  static std::vector<std::string>
  getPrecedingRecords(std::shared_ptr<const ndn::Data> data);

public:
  // key of the ledger
  std::string m_id;
  // record bytes
  ndn::Block m_data;
};

// The state of each unconfirmed record
class RecordState
{
public:
  RecordState() = default;
  RecordState(std::shared_ptr<const ndn::Data> data);

public:
  // record id
  std::string m_id = "";
  // record producer
  std::string m_producer = "";
  // pointer to the record
  std::shared_ptr<const ndn::Data> m_data;
  // number of endorsement
  int m_endorse = 0;
  // preceding records
  std::vector<std::string> m_precedingRecords;
  // peers who endorsed this record
  std::set<std::string> m_approvers;
};

} // namespace DLedger

#endif // DLEDGER_RECORD_H
