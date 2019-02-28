#include <ndn-cxx/name.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>

namespace ndn {
namespace dledger{

class LedgerRecord
{
public:
  LedgerRecord(shared_ptr<const Data> contentObject,
               int weight = 1, int entropy = 0, bool isArchived = false);
public:
  shared_ptr<const Data> block;
  int weight = 1;
  int entropy = 0;
  std::set<std::string> approverNames;
  bool isArchived = false;

//public:
//  bool isASample = false;
//  Time creationTime;
};

class Peer
{
public:
  Peer();

  virtual ~Peer(){};

  void
  registerPrefix(const Name& prefix);

  void
  run();

private:
  void
  OnData(const Interest& interest, const Data& data);

  void
  OnInterest(const Interest& interest);

  void
  OnNack(const Interest& interest, const lp::Nack& nack);

  void
  OnTimeout(const Interest& interest);

protected:
  void
  StartApplication();

  void
  StopApplication();

  void
  ScheduleNextGeneration();

  // schedule next sync
  void
  ScheduleNextSync();

  void
  SetRandomize(const std::string& value, double frequency);

  void
  SetGenerationRandomize(const std::string& value);

  void
  SetSyncRandomize(const std::string& value);

  std::string
  GetGenerationRandomize() const;

  std::string
  GetSyncRandomize() const;

public:
  // Get approved blocks from record content
  std::vector<std::string>
  GetApprovedBlocks(shared_ptr<const Data> data);
private:
  // Generates new record and sends notif interest
  void
  GenerateRecord();

  // Triggers sync interest
  void
  GenerateSync();

  // Fetches record using the given prefix
  void
  FetchRecord(Name prefix);

  // Update weight of records
  void
  UpdateWeightAndEntropy(shared_ptr<const Data> tail, std::set<std::string>& visited, std::string nodeName);

protected:
  boost::asio::io_service m_ioService;
  Face m_face;
  Scheduler m_scheduler;
  bool m_firstTime;
  bool m_syncFirstTime;
  //Ptr<RandomVariableStream> m_random;
  //Ptr<RandomVariableStream> m_syncRandom;
  //std::string m_randomTypeGeneration;
  //std::string m_randomTypeSync;
  EventId m_sendEvent; ///< @brief EventId of pending "send packet" event
  //EventId m_syncSendEvent;

  std::vector<std::string> m_tipList; // Tip list
  std::map<std::string, LedgerRecord> m_ledger;

  std::list<LedgerRecord> m_recordStack; // records stacked until their ancestors arrive
  std::set<std::string> m_missingRecords;
  int m_reqCounter; // request counter that talies record fetching interests sent with data received back

  // the var to tune
  signed int m_frequency; // Frequency of record generation (a record every m_frequency seconds)
  double m_syncFrequency; // Frequency of sync interest multicast
  int m_weightThreshold; // weight to be considered as archived block
  int m_conEntropy; // max entropy of a block which no new tips can refer
  int m_entropyThreshold; // the number of peers to approve
  int m_genesisNum; // the number of genesis blocks
  int m_referredNum; // the number of referred blocks

private:
  Name m_routablePrefix; // Node's prefix
  Name m_mcPrefix; // Multicast prefix

  KeyChain m_keyChain;

public:
  std::map<std::string, LedgerRecord> & GetLedger() {
    return m_ledger;
  }
};
}
}
