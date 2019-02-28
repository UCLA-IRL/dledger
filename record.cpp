#include "record.hpp"

namespace ndn {
namespace dledger {

LedgerRecord::LedgerRecord(shared_ptr<const Data> contentObject,
                           int weight, int entropy, bool isArchived)
  : block(contentObject)
  , weight(weight)
  , entropy(entropy)
  , isArchived(isArchived)
{
}

} // namespace dledger
} // namespace ndn
