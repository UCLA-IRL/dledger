#include "ledger-record.hpp"
#include <sstream>

namespace DLedger {

LedgerRecord::LedgerRecord(std::shared_ptr<const ndn::Data> data)
{
  m_id = data->getName().toUri();
  m_data = data->wireEncode();
}

LedgerRecord::LedgerRecord(const RecordState& state)
{
  m_id = state.m_id;
  m_data = state.m_data->wireEncode();
}

std::vector<std::string>
LedgerRecord::getPrecedingRecords(std::shared_ptr<const ndn::Data> data)
{
  // record content:
  // [precedingRecord]:[precedingRecord]:...\n
  // ==start==\n
  // [content]\n
  // ==end==\n
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

RecordState::RecordState(std::shared_ptr<const ndn::Data> data)
{
  m_id = data->getName().toUri();
  m_data = data;
  const auto& keyName = data->getSignature().getKeyLocator().getName();
  m_producer = keyName.getPrefix(-2).toUri();
  m_approvers.clear();
  m_precedingRecords.clear();
  m_precedingRecords = LedgerRecord::getPrecedingRecords(data);
}

} // namespace DLedger