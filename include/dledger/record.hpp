#ifndef DLEDGER_INCLUDE_RECORD_H_
#define DLEDGER_INCLUDE_RECORD_H_

#include <set>
#include <vector>
#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace dledger {

class Record
{
public:
  Record(const std::shared_ptr<Data>& data);
  Record(Data data);

  std::string
  getPayload() const;

  std::string
  getRecordName() const;

public:
  std::shared_ptr<const Data> m_data;
  std::vector<std::string> m_precedingRecords;
  std::set<std::string> m_approvers;
};

std::vector<std::string>
getPrecedingRecords(const Record& record);

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RECORD_H_

