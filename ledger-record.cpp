#include "ledger-record.hpp"

namespace DLedger {

LedgerRecord::LedgerRecord()
{

}

LedgerRecord::LedgerRecord(std::shared_ptr<const ndn::Data> data)
{
  m_id = data->getName().toUri();
  m_data = data->wireEncode();
}

} // namespace DLedger