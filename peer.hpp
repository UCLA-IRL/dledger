#include "ledger.hpp"
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>

namespace DLedger {

// static int RECORD_GEN_FREQUENCY = 1;
// static int SYNC_FREQUENCY = 5;
static int CONTRIBUTE_ENTROPY = 3;
static int CONFIRM_ENTROPY = 5;

class Peer
{
public:
  Peer(const Name& mcPrefix, const Name& routablePrefix,
       double recordGenFreq, double syncFreq);

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

  // Record Fetching State
  std::list<LedgerRecord> m_recordStack; // records stacked until their ancestors arrive
  std::set<std::string> m_missingRecords;
  int m_reqCounter; // request counter that talies record fetching interests sent with data received back

  // Configuration
  Name m_mcPrefix; // Multicast prefix
  Name m_routablePrefix; // Node's prefix

  int m_genesisNum; // the number of genesis blocks
  int m_approvalNum; // the number of referred blocks

  int m_recordGenFreq; // Frequency of record gen: time period (s) between two adjacent records
  int m_syncFreq; // Frequency of sync: time period (s) between two adjacent sync

  int m_contributeEntropy; // contribution entropy of a block which no new tips can refer
  int m_confirmEntropy; // the number of peers to approve
};

} // namespace DLedger
