#include "ledger-record.hpp"
#include "backend.hpp"
#include <ndn-cxx/security/v2/certificate.hpp>
#include <ndn-cxx/security/key-chain.hpp>

using namespace ndn;
namespace DLedger {

class Ledger
{
public:
  Ledger(const Name& routablePrefix, security::KeyChain& keychain,
         int approvalNum, int contributeWeight, int confirmWeight);

  // record content:
  // [precedingRecord]:[precedingRecord]:...\n
  // ==start==\n
  // [content]\n
  // ==end==\n
  RecordState
  generateNewRecord(const std::string& payload);

  void
  initGenesisRecord(const Name& mcPrefix, int genesisRecordNum);

  void
  detectIntrusion();

  // this function will not allocate new mem for record
  // instead, it will maintain a shared pointer to the record
  void
  onIncomingRecord(std::shared_ptr<const Data> data);

  // A recursive function to iterate records endorsed by the new record
  // for the first time invocation, let @p recordId = ""
  void
  afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState);

public:
  // tailing records
  std::vector<std::string> m_tailingRecordList;
  // unconfirmed record ledger
  std::map<std::string, RecordState> m_unconfirmedRecords;

  // peer info
  Name m_routablePrefix;
  security::v2::Certificate m_peerCert;
  security::KeyChain& m_keyChain;

  int m_approvalNum; // the number of referred blocks
  int m_contributeWeight; // contribution entropy of a block which no new tips can refer
  int m_confirmWeight; // the number of peers to approve

  // backend database
  Backend m_backend;
};

} // namespace DLedger