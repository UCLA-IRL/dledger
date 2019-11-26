#ifndef DLEDGER_RECORD_H
#define DLEDGER_RECORD_H

#include <iostream>
#include <set>
#include <vector>
#include <ndn-cxx/data.hpp>

namespace DLedger {

// Unconfirmed record
// The ledger needs to keep <id, RecordState> in memory
// If a record get confirmed, the record should be kept in the database
class RecordState
{
public:
  RecordState() = default;
  RecordState(ndn::Data data);

public:
  // record id
  std::string m_id = "";
  // record producer
  std::string m_producer = "";
  // pointer to the record
  std::shared_ptr<ndn::Data> m_data;
  // number of endorsement
  int m_endorse = 0;
  // preceding records
  std::vector<std::string> m_precedingRecords;
  // peers who endorsed this record
  std::set<std::string> m_approvers;
};

// Confirmed Record
// The ledger keeps confirmed record into the database
// This class is used when a record is fetched from the database
class LedgerRecord
{
public:
  LedgerRecord() = default;
  LedgerRecord(ndn::Data data);
  LedgerRecord(const RecordState& state);

public:
  // key of the ledger
  std::string m_id;
  // record bytes
  std::shared_ptr<ndn::Data> m_data;
};

static std::vector<std::string>
getPrecedingRecords(std::shared_ptr<const ndn::Data> data);

} // namespace DLedger

#endif // DLEDGER_RECORD_H
