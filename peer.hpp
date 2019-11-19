#include "ledger.hpp"
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>

using namespace ndn;
namespace DLedger {

static int RECORD_GEN_FREQUENCY = 1; // 1 per second
static int SYNC_FREQUENCY = 0.2; // 1 per 5 second
static int CONTRIBUTE_WEIGHT = 3;
static int CONFIRM_WEIGHT = 5;

// @mcPrefix multicast prefix is like /DLedger
// @routablePrefix routable prefix is like /DLedger/producer1
class Peer
{
public:
  Peer(const Name& mcPrefix, const Name& routablePrefix,
           int genesisNum, int approvalNum,
           double recordGenFreq = RECORD_GEN_FREQUENCY,
           double syncFreq = SYNC_FREQUENCY,
           int contributeWeight = CONTRIBUTE_WEIGHT,
           int confirmWeight = CONFIRM_WEIGHT);

  void
  run();

private:
  void
  OnData(const Interest& interest, const Data& data);

  void
  OnInterest(const Interest& interest);

  // void
  // OnNack(const Interest& interest, const lp::Nack& nack);

  // void
  // OnTimeout(const Interest& interest);

  // Generates new record and sends notif interest
  void
  GenerateRecord();

  // Triggers sync interest
  void
  GenerateSync();

  // Fetches record using the given prefix
  void
  FetchRecord(Name recordName);

public:
  boost::asio::io_service m_ioService;
  Face m_face;
  Scheduler m_scheduler;
  KeyChain m_keyChain;

  Ledger m_ledger;

  // Record Fetching State
  // std::list<LedgerRecord> m_recordStack; // records stacked until their ancestors arrive
  // std::set<std::string> m_missingRecords;
  // int m_reqCounter; // request counter that talies record fetching interests sent with data received back

  // Configuration
  Name m_mcPrefix; // Multicast prefix
  Name m_routablePrefix; // Node's prefix

  int m_recordGenFreq; // Frequency of record gen: time period (s) between two adjacent records
  int m_syncFreq; // Frequency of sync: time period (s) between two adjacent sync
};

} // namespace DLedger
