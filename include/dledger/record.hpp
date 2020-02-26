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
  Record(Data data);

private:
  std::shared_ptr<Data> m_data;
  std::vector<std::string> m_precedingRecords;
  std::set<std::string> m_approvers;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RECORD_H_

