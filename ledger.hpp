#include "ledger-record.hpp"
#include "backend.hpp"
#include <ndn-cxx/security/v2/certificate.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>

using namespace ndn;
namespace DLedger {

class Ledger
{
public:
  Ledger(const Name& multicastPrefix,
        const std::string& producerId, security::KeyChain& keychain,
        const std::string& myCertRecordId,
        security::v2::Certificate trustAnchorCert, Face& face,
        int approvalNum, int contributeWeight, int confirmWeight);

  void
  onRequestData(const Interest& interest, const Data& data);

  // record content:
  // [precedingRecord]:[precedingRecord]:...\n
  // ==start==\n
  // [content]\n
  // ==end==\n
  // producer-cert-record-name
  RecordState
  generateNewRecord(const std::string& payload);

  void
  initGenesisRecord(const Name& mcPrefix, int genesisRecordNum);

  void
  detectIntrusion();

  // this function will transfer the resource from @p data to RecordState
  // data will become undefined afterwards
  void
  onIncomingRecord(Data data);

  // @TODO: add two functions to check whether a record has been received:
  // 1. whether is in unconfirmed record list
  // 2. whether is in confirmed record list
  bool
  hasReceivedAsUnconfirmedRecord(const Data& data) const;
  bool
  hasReceivedAsConfirmedRecord(const Data& data) const;

  // @TODO: add one function to verify incoming record Data's signature
  // 1. read the last line of data content and get the record from local confirmed ledger.
  //    If cannot find, return false
  // 2. use the certificate from the record found to verify the data.
  //    If fails, return false
  // 3. check whether the last name componet is the hash of the content
  //    If fails, return false
  bool
  isValidRecord(const Data& data) const;

  // @TODO: I ported this function directly from NDNSIM. Please double check its correctness.
  // The expected logic:
  // 1. check if preceding records are received, if not, fetch them from the network
  // 2. the new record increases its preceding record's weight and then grandparents and ...
  //    this should stop at confirmed records
  // 3. in the back walk, if any record becomes confirmed, move the record to database (which means confirmed)
  // 4. (Difficult) when preceding records are missing, it's possible to fetch a large number of records,
  //    in this case, we should ensure all the newly fetched record are valid,
  //    once identifying a bad record, all its child records, grandchild records, ..., are invalid and should be dropped.
  // NOTICE: Zhiyi: I don't know whether one function is enough. More functions can be created if needed.
  //
  // A recursive function to iterate records endorsed by the new record
  // for the first time invocation, let @p recordId = ""
  void
  afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState);

public:
  // tailing records
  // self-generated records will never be added to tailing record list
  std::vector<std::string> m_tailingRecordList;
  // unconfirmed record ledger
  std::map<std::string, RecordState> m_unconfirmedRecords;

  // peer info
  Name m_mcPrefix;
  std::string m_producerId;
  security::v2::Certificate m_peerCert;
  security::KeyChain& m_keyChain;
  security::v2::Certificate m_trustAnchorCert;
  Face& m_face;

  int m_approvalNum; // the number of referred blocks
  int m_contributeWeight; // contribution entropy of a block which no new tips can refer
  int m_confirmWeight; // the number of peers to approve
  std::string m_certRecord; // self certificate record in the ledger

  // backend database
  Backend m_backend;
};

} // namespace DLedger