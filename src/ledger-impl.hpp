#ifndef DLEDGER_SRC_LEDGER_IMPLE_H_
#define DLEDGER_SRC_LEDGER_IMPLE_H_

#include "dledger/ledger.hpp"
#include "dledger/record.hpp"
#include "dledger/config.hpp"
#include "backend.hpp"
#include "cert-list.h"
#include <ndn-cxx/security/v2/certificate.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <stack>


using namespace ndn;
namespace dledger {

class LedgerImpl : public Ledger
{
public:
  LedgerImpl(const Config& config, security::KeyChain& keychain, Face& network, std::string id);

  ~LedgerImpl() override;

  ReturnCode
  createRecord(Record& record) override;

  optional<Record>
  getRecord(const std::string& recordName) override;

  bool
  hasRecord(const std::string& recordName) override;

private:
  void
  onNack(const Interest&, const lp::Nack& nack);

  void
  onTimeout(const Interest& interest);

  // the function to generate a sync Interest and send it out
  // should be invoked periodically or on solicit request
  void
  sendPeriodicSyncInterest();

  bool
  checkSyntaxValidityOfRecord(const Data& data);
  bool
  checkReferenceValidityOfRecord(const Data& data);

  // Interest format: each <> is only one name component
  // /<multicast_prefix>/NOTIF/<Full Name of Record>
  // Signature of the producer
  void
  onNewRecordNotification(const Interest& interest);

  // Interest format:
  // /<multicast_prefix>/SYNC
  // Parameters: A list of tailing record names
  void
  onLedgerSyncRequest(const Interest& interest);

  // Interest format:
  // record full name
  void
  onRecordRequest(const Interest& interest);

  // Zhiyi's temp function
  void
  fetchRecord(const Name& dataName);
  void
  onFetchedRecord(const Interest& interest, const Data& data);

  /**
   * Adds the record to backend and the tailing record map
   * @param record
   */
  void
  addToTailingRecord(const Record& record, bool verified);

  //Siqi's temp function
  struct TailingRecordState{
      bool referenceVerified;
      std::set<std::string> refSet;
      bool recordPolicyVerified;
  };
  void sendSyncInterest();
  void dumpList(const std::map<Name, TailingRecordState>& weight);

  /**
   * Check if the ancestor of the record is OK
   * @param record the record to be checked
   * @return true if the record is resolved; it is added or set as bad Record
   */
  bool checkRecordAncestor(const Record &record);

  /**
   * handles the information when a record is accepted.
   */
  void onRecordAccepted(const Record &record);
private:
  Config m_config;
  Face& m_network;
  std::string m_id;
  Scheduler m_scheduler;

  Backend m_backend;
  std::map<Name, TailingRecordState> m_tailRecords;
  CertList m_certList;

  std::map<std::string, time::system_clock::TimePoint> m_rateCheck;
  security::KeyChain& m_keychain;

  // Zhiyi's temp member variable
  std::list<std::pair<Record, time::system_clock::TimePoint>> m_syncStack;

  // Siqi's temp member variable
  std::set<Name> m_badRecords;
};

// class Ledger
// {
// public:
//   Ledger(const Name& multicastPrefix,
//         const std::string& producerId, security::KeyChain& keychain,
//         const std::string& myCertRecordId,
//         security::v2::Certificate trustAnchorCert, Face& face,
//         int approvalNum, int contributeWeight, int confirmWeight);

//   void
//   onRequestData(const Interest& interest, const Data& data);

//   // record content:
//   // [precedingRecord]:[precedingRecord]:...\n
//   // ==start==\n
//   // [content]\n
//   // ==end==\n
//   // producer-cert-record-name
//   RecordState
//   generateNewRecord(const std::string& payload);

//   void
//   initGenesisRecord(const Name& mcPrefix, int genesisRecordNum);

//   void
//   detectIntrusion();

//   // this function will transfer the resource from @p data to RecordState
//   // data will become undefined afterwards
//   void
//   onIncomingRecord(Data data);

//   // @TODO: add two functions to check whether a record has been received:
//   // 1. whether is in unconfirmed record list
//   // 2. whether is in confirmed record list
//   bool
//   hasReceivedAsUnconfirmedRecord(const Data& data) const;
//   bool
//   hasReceivedAsConfirmedRecord(const Data& data) const;

//   // @TODO: add one function to verify incoming record Data's signature
//   // 1. read the last line of data content and get the record from local confirmed ledger.
//   //    If cannot find, return false
//   // 2. use the certificate from the record found to verify the data.
//   //    If fails, return false
//   // 3. check whether the last name componet is the hash of the content
//   //    If fails, return false
//   bool
//   isValidRecord(const Data& data) const;

//   // @TODO: I ported this function directly from NDNSIM. Please double check its correctness.
//   // The expected logic:
//   // 1. check if preceding records are received, if not, fetch them from the network
//   // 2. the new record increases its preceding record's weight and then grandparents and ...
//   //    this should stop at confirmed records
//   // 3. in the back walk, if any record becomes confirmed, move the record to database (which means confirmed)
//   // 4. (Difficult) when preceding records are missing, it's possible to fetch a large number of records,
//   //    in this case, we should ensure all the newly fetched record are valid,
//   //    once identifying a bad record, all its child records, grandchild records, ..., are invalid and should be dropped.
//   // NOTICE: Zhiyi: I don't know whether one function is enough. More functions can be created if needed.
//   //
//   // A recursive function to iterate records endorsed by the new record
//   // for the first time invocation, let @p recordId = ""
//   void
//   afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState);

// public:
//   // tailing records
//   // self-generated records will never be added to tailing record list
//   std::vector<std::string> m_tailingRecordList;
//   // unconfirmed record ledger
//   std::map<std::string, RecordState> m_unconfirmedRecords;

//   // peer info
//   Name m_mcPrefix;
//   std::string m_producerId;
//   security::v2::Certificate m_peerCert;
//   security::KeyChain& m_keyChain;
//   security::v2::Certificate m_trustAnchorCert;
//   Face& m_face;

//   int m_approvalNum; // the number of referred blocks
//   int m_contributeWeight; // contribution entropy of a block which no new tips can refer
//   int m_confirmWeight; // the number of peers to approve
//   std::string m_certRecord; // self certificate record in the ledger

//   // backend database
//   Backend m_backend;
// };

} // namespace DLedger

#endif // DLEDGER_SRC_LEDGER_IMPLE_H_