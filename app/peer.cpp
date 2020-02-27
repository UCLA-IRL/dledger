#include "peer.hpp"
#include <chrono>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio.hpp>
#include <ndn-cxx/util/sha256.hpp>

using namespace ndn;

namespace DLedger {

NDN_LOG_INIT(DLedger);

Peer::Peer(const Name& mcPrefix, const Name& routablePrefix,
           int genesisNum, int approvalNum,
           double recordGenFreq, double syncFreq,
           int contributeWeight, int confirmWeight)
  : m_face(m_ioService)
  , m_scheduler(m_ioService)
  , m_mcPrefix(mcPrefix)
  , m_routablePrefix(routablePrefix)
  , m_ledger(m_routablePrefix, m_keyChain, approvalNum, contributeWeight, confirmWeight)
  , m_recordGenFreq(recordGenFreq)
  , m_syncFreq(syncFreq)
{
  m_ledger.initGenesisRecord(m_mcPrefix, genesisNum);
}

void
Peer::run()
{
  // prefix registration
  m_face.setInterestFilter(InterestFilter(m_mcPrefix),
                           bind(&Peer::OnInterest, this, _2), nullptr);
  m_face.setInterestFilter(InterestFilter(m_routablePrefix),
                           bind(&Peer::OnInterest, this, _2), nullptr);

  // schedule events
  // m_scheduler.schedule(time::seconds(m_recordGenFreq),
  //                     std::bind(&Peer::GenerateRecord, this));
  m_scheduler.schedule(time::seconds(m_syncFreq),
                       std::bind(&Peer::GenerateSync, this));

  // run
  m_face.processEvents();
}

void
Peer::GenerateSync()
{
  Name syncName(m_mcPrefix);
  syncName.append("SYNC");
  for (size_t i = 0; i != m_ledger.m_tailingRecordList.size(); i++) {
    syncName.append(m_ledger.m_tailingRecordList[i]);
  }

  auto syncInterest = std::make_shared<Interest>(syncName);
  syncInterest->setCanBePrefix(false);
  syncInterest->setInterestLifetime(time::milliseconds(500));
  NDN_LOG_INFO("> SYNC Interest " << syncInterest->getName().toUri());
  m_face.expressInterest(*syncInterest,
                         bind(&Peer::OnData, this, _1, _2),
                         nullptr, nullptr);

  m_scheduler.schedule(time::seconds(m_syncFreq), [this]{GenerateSync();});
}

std::vector<std::string>
Peer::GetApprovedBlocks(const Data& data)
{
  std::vector<std::string> approvedBlocks;
  auto content = ::ndn::encoding::readString(data.getContent());
  int nSlash = 0;
  const char *st, *ed;
  for (st = ed = content.c_str(); *ed && *ed != '*'; ed ++){
    if (*ed == ':') {
      if (nSlash >= 2) {
        approvedBlocks.push_back(std::string(st, ed));
      }
      nSlash = 0;
      st = ed + 1;
    }
    else if(*ed == '/'){
      nSlash ++;
    }
  }
  if (nSlash >= 2) {
    approvedBlocks.push_back(std::string(st, ed));
  }
  return approvedBlocks;
}

void
Peer::OnInterest(const Interest& interest)
{
  NDN_LOG_INFO("< Interest " << interest.getName().toUri());
  auto interestName = interest.getName();
  auto interestNameUri = interestName.toUri();

  // if it is notification interest (/mc-prefix/NOTIF/creator-pref/name)
  if (interestNameUri.find("NOTIF") != std::string::npos) {
    Name recordName(m_mcPrefix);
    recordName.append(interestName.getSubName(2).toUri());
    auto search = m_ledger.find(recordName.toUri());
    if (search == m_ledger.end()) {
      FetchRecord(recordName);
    }
  }
  // else if it is sync interest (/mc-prefix/SYNC/tip1/tip2 ...)
  // note that here tip1 will be /mc-prefix/creator-pref/name)
  else if (interestNameUri.find("SYNC") != std::string::npos) {
    auto tipDigest = interestName.getSubName(2);
    int iStartComponent = 0;
    auto tipName = tipDigest.getSubName(iStartComponent, 3);
    auto tipNameStr = tipName.toUri();
    while (tipNameStr != "/") {
      auto it = m_ledger.find(tipNameStr);
      if (it == m_ledger.end()) {
        FetchRecord(tipName);
      }
      else {
        // if weight is greater than 1,
        // this node has more recent tips
        // trigger sync
        if (it->second.weight > 1) {
          std::string syncNameStr(m_mcPrefix.toUri());
          //Name syncName(m_mcPrefix);
          //syncName.append("SYNC");
          syncNameStr += "/SYNC";
          for (size_t i = 0; i != m_ledger.m_tailingRecordList.size(); i++) {
            //syncName.append(m_ledger.m_tailingRecordList[i]);
            if(syncNameStr[syncNameStr.size() - 1] != '/' && m_ledger.m_tailingRecordList[i][0] != '/')
              syncNameStr += "/";
            syncNameStr += m_ledger.m_tailingRecordList[i];
          }

          auto syncInterest = std::make_shared<Interest>(syncNameStr);
          syncInterest->setCanBePrefix(false);
          m_face.expressInterest(*syncInterest, bind(&Peer::OnData, this, _1, _2),
                                 nullptr, nullptr);
        }
      }
      iStartComponent += 3;
      tipName = tipDigest.getSubName(iStartComponent, 3);
      tipNameStr = tipName.toUri();
    }
  }
  // else it is record fetching interest
  else {
    auto it = m_ledger.find(interestName.toUri());
    if (it != m_ledger.end()) {
      m_face.put(*it->second.block);
    }
    else {
      // This node doesn't have as well so it tries to fetch
      FetchRecord(interestName);
    }
  }
}

void
Peer::OnData(const Interest& interest, const Data& data)
{
  std::cout << "OnData(): DATA= " << data.getName().toUri() << std::endl;

  auto dataName = data.getName();
  auto dataNameUri = dataName.toUri();

  bool approvedBlocksInLedger = true;
  bool isTailingRecord = false;

  // Application-level semantics
  auto it = m_ledger.find(dataNameUri);
  if (it != m_ledger.end()){
    return;
  }

  auto it2 = m_missingRecords.find(dataNameUri);
  if (it2 == m_missingRecords.end()) {
    NDN_LOG_INFO("Is a Tailing Record");
    isTailingRecord = true;
  }
  else {
    m_missingRecords.erase(it2);
  }

  std::vector<std::string> approvedBlocks = GetApprovedBlocks(data);
  m_recordStack.push_back(LedgerRecord(std::make_shared<Data>(data)));
  for (size_t i = 0; i != approvedBlocks.size(); i++) {
    auto approvedBlockName = Name(approvedBlocks[i]);
    if (approvedBlockName.size() < 2) { // ignoring empty strings when splitting (:tip1:tip2)
      NDN_LOG_INFO("IGNORED " << approvedBlockName);
      continue;
    }
    if (approvedBlockName.get(1) == dataName.get(1)) { // recordname format: /dledger/node/hash
      m_recordStack.pop_back();
      NDN_LOG_INFO("INTERLOCK VIOLATION " << approvedBlockName);
      return;
    }
    it = m_ledger.find(approvedBlocks[i]);
    if (it == m_ledger.end()) {
      approvedBlocksInLedger = false;
      it2 = m_missingRecords.find(approvedBlocks[i]);
      if (it2 == m_missingRecords.end()) {
        m_missingRecords.insert(approvedBlocks[i]);
        FetchRecord(approvedBlockName);
        NDN_LOG_INFO("GO TO FETCH " << approvedBlockName);
      }
    }
    else {
      NDN_LOG_INFO("EXISTS APPROVAL " << approvedBlockName);
      if (isTailingRecord && it->second.entropy > m_confirmWeight) {
        NDN_LOG_INFO("Break Contribution Policy!!");
        return;
      }
    }
  }

  if (approvedBlocksInLedger) {
    for(auto it = m_recordStack.rbegin(); it != m_recordStack.rend(); ){
      NDN_LOG_INFO("STACK SIZE " << m_recordStack.size());

      const auto& record = *it;
      auto recordName = record.block->getName().toUri();
      approvedBlocks = GetApprovedBlocks(*record.block);
      bool ready = true;
      for(const auto& approveeName : approvedBlocks){
        if(m_ledger.find(approveeName) == m_ledger.end()){
          ready = false;
          break;
        }
      }
      if(!ready){
        it ++;
        continue;
      }

      m_ledger.m_tailingRecordList.push_back(recordName);
      m_ledger.insert(std::pair<std::string, LedgerRecord>(record.block->getName().toUri(), record));
      for (size_t i = 0; i != approvedBlocks.size(); i++) {
        m_ledger.m_tailingRecordList.erase(std::remove(m_ledger.m_tailingRecordList.begin(),
                                    m_ledger.m_tailingRecordList.end(), approvedBlocks[i]), m_ledger.m_tailingRecordList.end());

      }
      std::set<std::string> visited;
      UpdateWeightAndEntropy(record.block, visited, record.block->getName().getSubName(0, 2).toUri());
      std::cout << "ReceiveRecord: visited records size: " << visited.size()
                << " unconfirmed depth: " << log2(visited.size() + 1) << std::endl;

      it = decltype(it)(m_recordStack.erase(std::next(it).base()));
    }
  }
}

void
Peer::FetchRecord(Name recordName)
{
  auto recordInterest = std::make_shared<Interest>(recordName);
  recordInterest->setCanBePrefix(false);

  std::cout << "> RECORD Interest " << recordInterest->getName().toUri() << std::endl;

  m_face.expressInterest(*recordInterest, bind(&Peer::OnData, this, _1, _2),
                         nullptr, nullptr);
}

void
Peer::UpdateWeightAndEntropy(shared_ptr<const Data> tail,
                             std::set<std::string>& visited, std::string nodeName)
{
  auto tailName = tail->getName().toUri();
  // std::cout << tail->getName().getSubName(0, 2).toUri() << std::endl;
  visited.insert(tailName);
  // std::cout << "visited set size: " << visited.size() << std::endl;

  std::vector<std::string> approvedBlocks = GetApprovedBlocks(*tail);
  std::set<std::string> processed;

  for (size_t i = 0; i != approvedBlocks.size(); i++) {
    auto approvedBlock = approvedBlocks[i];

    // this approvedblock shouldnt be previously processed to avoid increasing weight multiple times
    // (this condition is when multiple references point to same block)
    auto search = processed.find(approvedBlock);
    if (search == processed.end()) {

      // do not increase weight if block has been previously visited
      // (this condition is useful when different chains merge)
      auto search2 = visited.find(approvedBlock);
      if (search2 == visited.end()) {

        auto it = m_ledger.find(approvedBlock);
        if (it != m_ledger.end()) { // this should always return true
          it->second.weight += 1;
          it->second.approverNames.insert(nodeName);
          it->second.entropy = it->second.approverNames.size();
          if (it->second.entropy >= m_confirmWeight) {
            it->second.isArchived = true;
            continue;
          }
          processed.insert(approvedBlock);
          UpdateWeightAndEntropy(it->second.block, visited, nodeName);
        }
        else {
          // NS_LOG_ERROR("it == m_ledger.end(): " << approvedBlock);
          throw 0;
        }
      }
    }
  }
}

// Generate a new record and send out notif and sync interest
void
Peer::GenerateRecord()
{
  if (m_missingRecords.size() > 0) {
    m_scheduler.schedule(time::seconds(m_recordGenFreq),
                         [this]{GenerateRecord();});
    return;
  }
  std::set<std::string> selectedBlocks;
  int tryTimes = 0;
  for (int i = 0; i < m_approvalNum; i++) {
    auto referenceIndex = rand() % (m_ledger.m_tailingRecordList.size() - 1);
    auto reference = m_ledger.m_tailingRecordList.at(referenceIndex);
    bool isArchived = m_ledger.find(reference)->second.isArchived;

    // cannot select a block generated by myself
    // cannot select a confirmed block
    while (m_routablePrefix.isPrefixOf(reference) || isArchived) {
      referenceIndex = rand() % (m_ledger.m_tailingRecordList.size() - 1);
      reference = m_ledger.m_tailingRecordList.at(referenceIndex);
      isArchived = m_ledger.find(reference)->second.isArchived;
    }
    selectedBlocks.insert(reference);
    if (i == m_approvalNum - 1 && selectedBlocks.size() < 2) {
      i--;
      tryTimes++;
      if (tryTimes > 10) {
        m_scheduler.schedule(time::seconds(m_recordGenFreq),
                                  [this]{GenerateRecord();});
        return;
      }
    }
  }

  std::string recordContent = "";
  for (const auto& item : selectedBlocks) {
    recordContent += ":";
    recordContent += item;
    m_ledger.m_tailingRecordList.erase(std::remove(m_ledger.m_tailingRecordList.begin(),
                                m_ledger.m_tailingRecordList.end(), item), m_ledger.m_tailingRecordList.end());
  }
  // to avoid the same digest made by multiple peers, add peer specific info
  recordContent += "***";
  recordContent += m_routablePrefix.toUri();

  // generate digest as a name component
  std::istringstream sha256Is(recordContent);
  ::ndn::util::Sha256 sha(sha256Is);
  std::string recordDigest = sha.toString();
  sha.reset();

  // generate a new record
  // Naming: /dledger/nodeX/digest
  Name recordName(m_routablePrefix);
  recordName.append(recordDigest);
  auto record = std::make_shared<Data>(recordName);
  record->setContent(::ndn::encoding::makeStringBlock(::ndn::tlv::Content, recordContent));
  m_keyChain.sign(*record);

  // attach to local ledger
  m_ledger.insert(std::pair<std::string, LedgerRecord>(recordName.toUri(), LedgerRecord(record)));
  // add to tip list
  m_ledger.m_tailingRecordList.push_back(recordName.toUri());

  // update weights of directly or indirectly approved blocks
  std::set<std::string> visited;
  UpdateWeightAndEntropy(record, visited, recordName.getSubName(0, 2).toUri());
  //NS_LOG_INFO("NewRecord: visited records size: " << visited.size()
  //            << " unconfirmed depth: " << log2(visited.size() + 1));

  Name notifName(m_mcPrefix);
  notifName.append("NOTIF").append(m_routablePrefix.getSubName(1).toUri()).append(recordDigest);
  Interest notif(notifName);
  notif.setCanBePrefix(false);

  std::cout << "> NOTIF Interest " << notif.getName().toUri() << std::endl;

  m_face.expressInterest(notif, bind(&Peer::OnData, this, _1, _2),
                         nullptr, nullptr);

  m_scheduler.schedule(time::seconds(m_recordGenFreq),
                            [this]{GenerateRecord();});
}

} // namespace DLedger
