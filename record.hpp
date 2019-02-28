#ifndef DLEDGER_RECORD_H
#define DLEDGER_RECORD_H

#include <iostream>
#include <set>
#include <ndn-cxx/data.hpp>

namespace ndn {
namespace dledger{

class LedgerRecord
{
public:
  LedgerRecord(std::shared_ptr<const Data> contentObject,
               int weight = 1, int entropy = 0, bool isArchived = false);
public:
  std::shared_ptr<const Data> block;
  int weight = 1;
  int entropy = 0;
  std::set<std::string> approverNames;
  bool isArchived = false;
};

} // namespace dledger
} // namespace ndn

#endif // DLEDGER_RECORD_H
