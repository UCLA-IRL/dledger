#include "ledger-record.hpp"
#include <sstream>

namespace DLedger {

RecordState::RecordState(ndn::Data data)
{
  m_data = std::make_shared<ndn::Data>(std::move(data));
  m_id = m_data->getName().toUri();
  const auto& keyName = m_data->getSignature().getKeyLocator().getName();
  m_producer = keyName.getPrefix(-2).toUri();
  m_approvers.clear();
  m_precedingRecords.clear();
  m_precedingRecords = getPrecedingRecords(m_data);
}

LedgerRecord::LedgerRecord(ndn::Data data)
{
  m_data = std::make_shared<ndn::Data>(std::move(data));
  m_id = m_data->getName().toUri();
}

LedgerRecord::LedgerRecord(const RecordState& state)
{
  m_id = state.m_id;
  m_data = state.m_data;
}

std::vector<std::string>
getPrecedingRecords(std::shared_ptr<const ndn::Data> data)
{
  // record content:
  // [precedingRecord]:[precedingRecord]:...\n
  // ==start==\n
  // [content]\n
  // ==end==\n
  // producer-cert-record-id
  auto contentStr = ndn::encoding::readString(data->getContent());
  std::istringstream iss(contentStr);
  std::string line;
  std::getline(iss, line);

  std::vector<std::string> precedingRecords;
  std::string delimiter = ":";
  size_t last = 0;
  size_t next = 0;
  while ((next = line.find(delimiter, last)) != std::string::npos) {
    precedingRecords.push_back(line.substr(last, next-last));
    last = next + 1;
  }
  precedingRecords.push_back(line.substr(last));
  return precedingRecords;
}

} // namespace DLedger