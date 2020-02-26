#include "ledger-impl.hpp"
#include <algorithm>
#include <random>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>

using namespace ndn;
namespace dledger {

LedgerImpl::LedgerImpl(const Config& config,
         const Name& multicastPrefix,
         const Name& producerPrefix,
         const security::v2::Certificate& trustAnchorCert,
         security::KeyChain& keychain,
         Face& network)
  : m_config(config)
  , m_multicastPrefix(multicastPrefix)
  , m_producerPrefix(producerPrefix)
  , m_trustAnchorCert(trustAnchorCert)
  , m_keychain(keychain)
  , m_network(network)
{}

LedgerImpl::~LedgerImpl()
{}

ReturnCode
LedgerImpl::addRecord(const std::string& recordName, const std::string& payload)
{
  if (m_tailingRecords.size() <= 0) {
    return ReturnCode::noTailingRecord();
  }
  // randomly shuffle the tailing record list
  std::set<std::string> precedingRecords;
  std::random_device rd;
  std::default_random_engine engine{rd()};
  std::shuffle(std::begin(m_tailingRecords), std::end(m_tailingRecords), engine);
  // fulfill the record content with preceding record IDs
  // and remove the selected preceding records from tailing record list
  std::string contentStr = "";
  for (int i = 0; i < m_config.preceidingRecordNum; i++) {
    const auto& recordId = m_tailingRecords[i];
    contentStr += recordId;
    m_tailingRecords.pop_back();
    if (i < m_config.preceidingRecordNum - 1) {
      contentStr += ":";
    }
    else {
      contentStr += "\n";
    }
  }
  contentStr += "==start==\n";
  contentStr += payload;
  contentStr += "\n==end==\n";
  // // calculate digest
  // std::istringstream sha256Is(contentStr);
  // util::Sha256 sha(sha256Is);
  // std::string contentDigest = sha.toString();
  // sha.reset();

  Name dataName = m_producerPrefix;
  dataName.append(recordName);
  Data data(dataName);
  data.setContent(encoding::makeStringBlock(tlv::Content, contentStr));
  // sign the packet with peer's key
  m_keychain.sign(data, security::signingByIdentity(m_producerPrefix));
}

  ReturnCode
LedgerImpl::getRecord(const std::string& recordName, Record& record)
{
  return ReturnCode::noError();
}

  bool
LedgerImpl::checkRecord(const std::string& recordName)
{
  return true;
}

  void
LedgerImpl::setOnRecordAppLogic(const OnNewRecord& onNewRecord)
{
}

  void
LedgerImpl::onNewRecordNotification(const Interest& interest)
{}

  void
LedgerImpl::onRequestedData(const Interest& interest, const Data& data)
{}

  void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{}

  void
LedgerImpl::onRecordRequest(const Interest& interest)
{}


//===============================================================================

// // This solely cannot ensure interlock policy.
// // Signature verification of the current record and preceding records are also needed.
// static bool
// hasBreakInterlock(const std::string& recordId, const std::string& producerID)
// {
//   Name name(recordId);
//   if (name.get(-2).toUri() == producerID) {
//     return true;
//   }
//   return false;
// }

// Ledger::Ledger(const Name& multicastPrefix,
//                const std::string& producerId, security::KeyChain& keychain,
//                const std::string& myCertRecordId,
//                security::v2::Certificate trustAnchorCert, Face& face,
//                int approvalNum, int contributeWeight, int confirmWeight)
//   : m_mcPrefix(multicastPrefix)
//   , m_producerId(producerId)
//   , m_keyChain(keychain)
//   , m_trustAnchorCert(trustAnchorCert)
//   , m_face(face)
//   , m_approvalNum(approvalNum)
//   , m_contributeWeight(contributeWeight)
//   , m_confirmWeight(confirmWeight)
// {
//   auto identity = m_keyChain.createIdentity(Name(m_producerId));
//   m_peerCert = identity.getDefaultKey().getDefaultCertificate();

//   // earn whether the ledger has obtained a cert from the identity manager
//   if (myCertRecordId == "") {
//     // if not, apply for a certificate from identity manager
//     Interest request("/identity-manager/request");
//     request.setApplicationParameters(m_peerCert.wireEncode());
//     m_face.expressInterest(request, std::bind(&Ledger::onRequestData, this, _1, _2), nullptr, nullptr);
//   }
//   else {
//     m_certRecord = myCertRecordId;
//   }
// }

// void
// Ledger::onRequestData(const Interest& interest, const Data& data)
// {
//   m_certRecord = readString(data.getContent());
// }

// void
// Ledger::initGenesisRecord(const Name& mcPrefix, int genesisRecordNum)
// {
//   // adding genesis records
//   for (int i = 0; i < genesisRecordNum; i++) {
//     Name genesisName(mcPrefix);
//     genesisName.append("genesis");
//     genesisName.append("genesis" + std::to_string(i));
//     ndn::Data genesis(genesisName);
//     const auto& genesisNameStr = genesisName.toUri();
//     m_tailingRecordList.push_back(genesisNameStr);
//     m_backend.putRecord(LedgerRecord(genesis));
//   }
// }

// RecordState
// Ledger::generateNewRecord(const std::string& payload)
// {
//   // randomly shuffle the tailing record list
//   std::set<std::string> precedingRecords;
//   std::random_device rd;
//   std::default_random_engine engine{rd()};
//   std::shuffle(std::begin(m_tailingRecordList), std::end(m_tailingRecordList), engine);
//   // fulfill the record content with preceding record IDs
//   // and remove the selected preceding records from tailing record list
//   std::string contentStr = "";
//   for (int i = 0; i < m_approvalNum; i++) {
//     const auto& recordId = m_tailingRecordList[i];
//     contentStr += recordId;
//     m_tailingRecordList.pop_back();
//     if (i < m_approvalNum - 1) {
//       contentStr += ":";
//     }
//     else {
//       contentStr += "\n";
//     }
//   }
//   contentStr += "==start==\n";
//   contentStr += payload;
//   contentStr += "\n==end==\n";
//   contentStr += m_certRecord;
//   // calculate digest
//   std::istringstream sha256Is(contentStr);
//   util::Sha256 sha(sha256Is);
//   std::string contentDigest = sha.toString();
//   sha.reset();
//   // generate NDN data packet
//   // @TODO need discussion here: which prefix should be used
//   // for now, simply use: /multicast/producer-id/record-hash, we assume producer-id is one component
//   Name dataName = m_mcPrefix;
//   dataName.append(m_producerId).append(contentDigest);
//   ndn::Data data(dataName);
//   data.setContent(encoding::makeStringBlock(tlv::Content, contentStr));
//   // sign the packet with peer's key
//   m_keyChain.sign(data, security::signingByCertificate(m_peerCert));
//   // append newly generated record into the Ledger
//   RecordState state(data);
//   m_unconfirmedRecords[state.m_id] = state;
//   afterAddingNewRecord("", state);
// }

// void
// Ledger::detectIntrusion()
// {
//   // leave this empty for now
//   // @TODO need to discuss with Randy King
// }

// bool
// Ledger::hasReceivedAsUnconfirmedRecord(const Data& data) const
// {
//   return true;
// }

// bool
// Ledger::hasReceivedAsConfirmedRecord(const Data& data) const
// {
//   return true;
// }

// bool
// Ledger::isValidRecord(const Data& data) const
// {
//   return true;
// }

// void
// Ledger::onIncomingRecord(Data data)
// {
//   if (hasReceivedAsUnconfirmedRecord(data) || hasReceivedAsConfirmedRecord(data)) {
//     return;
//   }
//   if (!isValidRecord(data)) {
//     return;
//   }
//   RecordState state(data);
//   // @TODO: the logic to check whether breaks INTERLOCK policy should go to isValidRecord()
//   // check record against INTERLOCK POLICY
//   std::vector<std::string>::iterator it = std::find_if(state.m_precedingRecords.begin(), state.m_precedingRecords.end(),
//                                                        std::bind(hasBreakInterlock, _1, state.m_producer));
//   if (it != state.m_precedingRecords.end()) {
//     // this record approves its producer's own records, drop it
//     return;
//   }
//   // now the new record looks fine, add it
//   m_unconfirmedRecords[state.m_id] = state;
//   afterAddingNewRecord("", state);
// }

// void
// Ledger::afterAddingNewRecord(const std::string& recordId, const RecordState& newRecordState)
// {
//   // first time invocation?
//   if (recordId == "") {
//     auto precedingRecords = getPrecedingRecords(newRecordState.m_data);
//     for (const auto& item : precedingRecords) {
//       afterAddingNewRecord(item, newRecordState);
//     }
//     return;
//   }

//   // otherwise, get preceding record
//   auto it = m_unconfirmedRecords.find(recordId);
//   // check if the record is already confirmed
//   if (it == m_unconfirmedRecords.end()) {
//     return;
//   }
//   else {
//     // if not, update the record state
//     it->second.m_approvers.insert(newRecordState.m_producer);
//     it->second.m_endorse = it->second.m_approvers.size();
//     if (it->second.m_endorse >= m_confirmWeight) {
//       // insert the record into the backend database
//       m_backend.putRecord(LedgerRecord(it->second));
//       // remove it from the unconfirmed record map
//       m_unconfirmedRecords.erase(it);
//       return;
//     }
//     // otherwise go record's preceding records
//     for (const auto& item : it->second.m_precedingRecords) {
//       afterAddingNewRecord(item, newRecordState);
//     }
//   }
// }

} // namespace DLedger