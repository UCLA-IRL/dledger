#ifndef DLEDGER_SRC_LEDGER_IMPLE_H_
#define DLEDGER_SRC_LEDGER_IMPLE_H_

#include "dledger/ledger.hpp"
#include "dledger/record.hpp"
#include "dledger/config.hpp"
#include "backend.hpp"
#include <ndn-cxx/security/certificate.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <stack>
#include <random>


using namespace ndn;
namespace dledger {

class LedgerImpl : public Ledger
{
public:
  LedgerImpl(const Config& config, security::KeyChain& keychain, Face& network);

  ~LedgerImpl() override;

  ReturnCode
  createRecord(Record& record) override;

  optional<Record>
  getRecord(const std::string& recordName) const override;

  bool
  hasRecord(const std::string& recordName) const override;

  std::list<Name>
  listRecord(const std::string& prefix) const override;

private:
  optional<Record>
  getRecord(const Name& recordName) const;

  bool
  seenRecord(const Name& recordName) const;

  void
  onNack(const Interest&, const lp::Nack& nack);

  void
  onTimeout(const Interest& interest);

  // the function to generate a sync Interest and send it out
  // should be invoked periodically or on solicit request
  ReturnCode
  sendSyncInterest();

  bool
  checkSyntaxValidityOfRecord(const Data& data);
  bool
  checkEndorseValidityOfRecord(const Data& data);

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
  addToTailingRecord(const Record& record, bool endorseVerified);

  //Siqi's temp function
  struct TailingRecordState{
      bool referenceVerified;
      std::set<Name> refSet;
      bool endorseVerified;
      Record record;
      time::system_clock::TimePoint addedTime;
  };
  static void dumpList(const std::map<Name, TailingRecordState>& weight);

  /**
   * Check if the ancestor of the record is OK
   * @param record the record to be checked
   * @return true if the record is resolved; it is added or set as bad Record
   */
  bool checkRecordAncestor(const Record &record);

  /**
   * handles the information when a record is accepted.
   */
  void onRecordConfirmed(const Record &record);

  /**
   * handles removal of timeout records
   */
  void removeTimeoutRecords();

private:
  Config m_config;
  Face& m_network;
  Scheduler m_scheduler;
  Backend m_backend;
  security::KeyChain& m_keychain;

  std::map<Name, TailingRecordState> m_tailRecords;

  // Zhiyi's temp member variable
  std::list<std::pair<Record, time::system_clock::TimePoint>> m_syncStack;

  // Siqi's temp member variable
  std::set<Name> m_badRecords;
  scheduler::EventId m_syncEventID;
  scheduler::EventId m_replySyncEventID;
  std::mt19937_64 m_randomEngine{std::random_device{}()};
  std::list<Name> m_lastCertRecords; // for certificate chains
};

} // namespace DLedger

#endif // DLEDGER_SRC_LEDGER_IMPLE_H_