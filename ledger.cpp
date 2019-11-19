#include "ledger.hpp"
#include <algorithm>
#include <random>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>

using namespace ndn;
namespace DLedger {

Ledger::Ledger(const Name& routablePrefix, security::KeyChain& keychain,
               int approvalNum, int contributeWeight, int confirmWeight)
  : m_routablePrefix(routablePrefix)
  , m_keyChain(keychain)
  , m_approvalNum(approvalNum)
  , m_contributeWeight(contributeWeight)
  , m_confirmWeight(confirmWeight)
{
  auto identity = m_keyChain.createIdentity(routablePrefix);
  m_peerCert = identity.getDefaultKey().getDefaultCertificate();
}

void
Ledger::initGenesisRecord(const Name& mcPrefix, int genesisRecordNum)
{
  // adding genesis records
  for (int i = 0; i < genesisRecordNum; i++) {
    Name genesisName(mcPrefix);
    genesisName.append("genesis");
    genesisName.append("genesis" + std::to_string(i));
    auto genesis = std::make_shared<Data>(genesisName);
    const auto& genesisNameStr = genesisName.toUri();
    m_tailingRecordList.push_back(genesisNameStr);
    m_backend.putRecord(LedgerRecord(genesis));
  }
}

RecordState
Ledger::generateNewRecord(const std::string& payload)
{
  // randomly shuffle the tailing record list
  std::set<std::string> precedingRecords;
  std::random_device rd;
  std::default_random_engine engine{rd()};
  std::shuffle(std::begin(m_tailingRecordList), std::end(m_tailingRecordList), engine);

  // fulfill the record content and remove the selected preceding records from tailing record list
  std::string contentStr = "";
  for (int i = 0; i < m_approvalNum; i++) {
    const auto& recordId = m_tailingRecordList[m_tailingRecordList.size() - 1];
    if (m_unconfirmedRecords[recordId].m_producer != m_routablePrefix) {
      contentStr += recordId;
      m_tailingRecordList.pop_back();
      if (i < m_approvalNum - 1) {
        contentStr += ":";
      }
      else {
        contentStr += "\n";
      }
    }
    else {
      // if the last element was generated my one's own, swap it with the second last element and retry
      // THIS IS TO FOLLOW DLEDGER'S INTERLOCK POLICY
      if (m_tailingRecordList.size() - 2 >= 0) {
        std::swap(m_tailingRecordList[m_tailingRecordList.size() - 1], m_tailingRecordList[m_tailingRecordList.size() - 2]);
        i--;
      }
      else {
        return RecordState();
      }
    }
  }
  contentStr += "==start==\n";
  contentStr += payload;
  contentStr += "\n==end==\n";
  // calculate digest
  std::istringstream sha256Is(contentStr);
  util::Sha256 sha(sha256Is);
  std::string contentDigest = sha.toString();
  sha.reset();
  // generate NDN data packet
  Name dataName = m_routablePrefix;
  dataName.append("DLedger").append(contentDigest);
  auto data = std::make_shared<Data>(dataName);
  data->setContent(encoding::makeStringBlock(tlv::Content, contentStr));
  // sign the packet with peer's key
  m_keyChain.sign(*data, security::signingByCertificate(m_peerCert));
  // append newly generated record into the Ledger
  RecordState state(data);
  m_unconfirmedRecords[state.m_id] = state;
  afterAddingNewRecord("", state);
}

void
Ledger::detectIntrusion()
{
  // TODO
}

void
Ledger::onIncomingRecord(std::shared_ptr<const Data> data)
{
  RecordState state(data);
  auto it = m_unconfirmedRecords.find(state.m_id);
  // check whether the record is already obtained
  if (it == m_unconfirmedRecords.end()) {
    // if not, check record against INTERLOCK POLICY
    std::vector<std::string>::iterator it = std::find(state.m_precedingRecords.begin(),
                                                      state.m_precedingRecords.end(), state.m_producer);
    if (it != state.m_precedingRecords.end()) {
      // this record approves its producer's own records, drop it
      return;
    }
    // append the record into the ledger
    m_unconfirmedRecords[state.m_id] = state;
    afterAddingNewRecord("", state);
  }
}

void
Ledger::afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState)
{
  // first time invocation?
  if (recordId == "") {
    auto precedingRecords = LedgerRecord::getPrecedingRecords(newRecordState.m_data);
    for (const auto& item : precedingRecords) {
      afterAddingNewRecord(item, newRecordState);
    }
    return;
  }

  // otherwise, get preceding record
  auto it = m_unconfirmedRecords.find(recordId);
  // check if the record is already confirmed
  if (it == m_unconfirmedRecords.end()) {
    return;
  }
  else {
    // if not, update the record state
    it->second.m_approvers.insert(newRecordState.m_producer);
    it->second.m_endorse = it->second.m_approvers.size();
    if (it->second.m_endorse >= m_confirmWeight) {
      // insert the record into the backend database
      m_backend.putRecord(LedgerRecord(it->second));
      // remove it from the unconfirmed record map
      m_unconfirmedRecords.erase(it);
      return;
    }
    // otherwise go record's preceding records
    for (const auto& item : it->second.m_precedingRecords) {
      afterAddingNewRecord(item, newRecordState);
    }
  }
}

} // namespace DLedger