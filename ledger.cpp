#include "ledger.hpp"
#include <algorithm>
#include <random>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>

namespace DLedger {

Ledger::Ledger(const ndn::Name& routablePrefix,
               const ndn::security::v2::Certificate& cert, ndn::security::KeyChain& keychain)
  : m_routablePrefix(routablePrefix)
  , m_peerCert(cert)
  , m_keychain(keychain)
{
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
  for (int i = 0; i < PRECEDING_NUM; i++) {
    const auto& recordId = m_tailingRecordList[m_tailingRecordList.size() - 1];
    if (m_unconfirmedRecords[recordId].m_producer != ndn::security::v2::extractIdentityFromCertName(m_peerCert.getName())) {
      contentStr += m_tailingRecordList[m_tailingRecordList.size() - 1];
      m_tailingRecordList.pop_back();
      if (i < PRECEDING_NUM - 1) {
        contentStr += ":";
      }
      else {
        contentStr += "\n";
      }
    }
    else {
      // if the last element was generated my one's own, swap it with the second last element and retry
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
  ndn::util::Sha256 sha(sha256Is);
  std::string contentDigest = sha.toString();
  sha.reset();
  // generate NDN data packet
  ndn::Name dataName = m_routablePrefix;
  dataName.append("DLedger").append(contentDigest);
  auto data = std::make_shared<ndn::Data>(dataName);
  data->setContent(ndn::encoding::makeStringBlock(ndn::tlv::Content, contentStr));
  // sign the packet with peer's key
  m_keychain.sign(*data, ndn::security::signingByCertificate(m_peerCert));
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
Ledger::onIncomingRecord(std::shared_ptr<const ndn::Data> data)
{
  RecordState state(data);
  auto it = m_unconfirmedRecords.find(state.m_id);
  // check whether the record is already obtained
  if (it != m_unconfirmedRecords.end()) {
    // if not, append the record into the ledger
    m_unconfirmedRecords[state.m_id] = state;
    afterAddingNewRecord("", state);
  }
  else {
    // otherwise, return
    return;
  }
}

std::vector<std::string>
Ledger::getTailingRecordList()
{
  return m_tailingRecordList;
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
    if (it->second.m_endorse >= CONFIRMATION_NUM) {
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