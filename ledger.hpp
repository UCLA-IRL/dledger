#include "ledger-record.hpp"
#include "backend.hpp"
#include <ndn-cxx/security/v2/certificate.hpp>
#include <ndn-cxx/security/key-chain.hpp>

namespace DLedger {

const static int PRECEDING_NUM = 2;
const static int CONFIRMATION_NUM = 10;

class Ledger
{
public:
  Ledger(const ndn::Name& routablePrefix, const ndn::security::v2::Certificate& cert,
         ndn::security::KeyChain& keychain);

  // record content:
  // [precedingRecord]:[precedingRecord]:...\n
  // ==start==\n
  // [content]\n
  // ==end==\n
  RecordState
  generateNewRecord(const std::string& payload);

  void
  detectIntrusion();

  // this function will not allocate new mem for record
  // instead, it will maintain a shared pointer to the record
  void
  onIncomingRecord(std::shared_ptr<const ndn::Data> data);

  std::vector<std::string>
  getTailingRecordList();

private:
  // A recursive function to iterate records endorsed by the new record
  // for the first time invocation, let @p recordId = ""
  void
  afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState);

private:
  // tailing records
  std::vector<std::string> m_tailingRecordList;
  // unconfirmed record ledger
  std::map<std::string, RecordState> m_unconfirmedRecords;

  // peer info
  ndn::Name m_routablePrefix;
  ndn::security::v2::Certificate m_peerCert;
  ndn::security::KeyChain& m_keychain;

  // backend database
  Backend m_backend;
};

} // namespace DLedger