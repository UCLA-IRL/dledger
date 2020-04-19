#include "ledger-impl.hpp"
#include <algorithm>
#include <random>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>
#include <time.h>       /* time */


using namespace ndn;
namespace dledger {

LedgerImpl::LedgerImpl(const Config& config,
                       security::KeyChain& keychain,
                       Face& network)
  : Ledger()
  , m_config(config)
  , m_keychain(keychain)
  , m_network(network)
  //consider adding a producer identity member?
{}

LedgerImpl::~LedgerImpl()
{}

ReturnCode
LedgerImpl::addRecord(Record& record, const Name& signerIdentity)
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
  int counter = 0, iterator = 0;
  for (; counter < m_config.preceidingRecordNum && iterator < m_tailingRecords.size(); counter++) {
    const auto& recordId = m_tailingRecords[iterator];
    if (m_config.producerPrefix.isPrefixOf(recordId)) {
      counter--;
      iterator++;
      continue;
    }
    record.addPointer(recordId);
    m_tailingRecords.erase(m_tailingRecords.begin() + iterator);
  }
  if (counter < m_config.preceidingRecordNum) {
    return ReturnCode::notEnoughTailingRecord();
  }

  // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>
  // each <> represent only one component
  Name dataName = m_config.producerPrefix;
  dataName.append(std::to_string(record.m_type)).append(record.m_uniqueIdentifier).appendTimestamp();
  auto data = make_shared<Data>(dataName);
  auto contentBlock = makeEmptyBlock(tlv::Content);
  record.wireEncode(contentBlock);
  data->setContent(contentBlock);

  // sign the packet with peer's key
  try {
    m_keychain.sign(*data, security::signingByIdentity(signerIdentity));
  }
  catch(const std::exception& e) {
    return ReturnCode::signingError(e.what());
  }
  record.m_data = data;

  // add new record into the ledger
  m_backend.putRecord(data);

  // send out notification
  Name intName(m_config.multicastPrefix);
  intName.append("NOTIF").append(data->getFullName().wireEncode());
  Interest interest(intName);
  try {
    m_keychain.sign(interest, security::signingByIdentity(signerIdentity));
  }
  catch(const std::exception& e) {
    return ReturnCode::signingError(e.what());
  }
  m_network.expressInterest(interest, nullptr, nullptr, nullptr);

  return ReturnCode::noError();
}

optional<Record>
LedgerImpl::getRecord(const std::string& recordName)
{
  auto dataPtr = m_backend.getRecord(Name(recordName));
  if (dataPtr != nullptr) {
    return Record(dataPtr);
  }
  else {
    return nullopt;
  }
}

bool
LedgerImpl::hasRecord(const std::string& recordName)
{
  auto dataPtr = m_backend.getRecord(Name(recordName));
  if (dataPtr != nullptr) {
    return true;
  }
  return false;
}

void
LedgerImpl::onNewRecordNotification(const Interest& interest)
{
  //Temporary stand-in.
  security::v2::Certificate certInLedger;
  if(!security::verifySignature(interest, certInLedger)){
    std::cout << "Signature wasn't verified in onNewRecord";
  } else {
    // verify the signature
    // if (security::verifySignature(interest, TODO: certificate of the peer))
    // extract the Record Data name from the interest
    auto nameBlock = interest.getName().get(-1).toUri();
    std::mt19937_64 eng{std::random_device{}()};
    std::uniform_int_distribution<> dist{10, 100};
    sleep(dist(eng)/100);
    //chop off NOTIF bit
    ndn::Name interestForRecordName;
    interestForRecordName.wireDecode(interest.getName().get(-1).blockFromValue());
    Interest interestForRecord(interestForRecordName);

    std::cout << "Sending Interest " << interestForRecord << std::endl;
    m_network.expressInterest(interestForRecord,
                           bind(&LedgerImpl::onRequestedData, this,  _1, _2),
                           bind(&LedgerImpl::onNack, this, _1, _2),
                           bind(&LedgerImpl::onTimeout, this, _1));



  }

  // a random timer and then fetch it back
}

void
LedgerImpl::onNack(const Interest&, const lp::Nack& nack)
{
  std::cout << "Received Nack with reason " << nack.getReason() << std::endl;
}

void
LedgerImpl::onTimeout(const Interest& interest)
{
  std::cout << "Timeout for " << interest << std::endl;
}

void
LedgerImpl::sendPerodicSyncInterest()
{
  Name syncInterestName = m_config.multicastPrefix;
  syncInterestName.append("SYNC");
  Interest syncInterest(syncInterestName);
  std::string appParameters = "";
  for (const auto& item : m_tailingRecords) {
    appParameters += item.toUri();
    appParameters += "\n";
  }
  syncInterest.setApplicationParameters((uint8_t*)appParameters.c_str(), appParameters.size());
  m_keychain.sign(syncInterest, signingByIdentity(m_config.producerPrefix));
   // nullptrs for callbacks because a sync Interest is not expecting a Data back
  m_network.expressInterest(syncInterest, nullptr, nullptr, nullptr);
}

bool
LedgerImpl::checkValidityOfRecord(const Data& data)
{
  try {
  dledger::Record dataRecord = dledger::Record(data);
  } catch(const std::exception& e) {
    std::cout << "Record wasn't proper";
    return false;
  }
  //Kknow this isn't right, but not sure how to use the keyLocator
  KeyLocator keyl = KeyLocator(data.getSignature().getKeyLocator());
  //Add state to ledger class which is another dictionary? Key is keylocator [keyname], value is the certificate. If have key, verify, else then trash
  //ok, now do the rate check
  std::string producedBy = data.getName().get(-3).toUri();
  std::time_t present = std::time(0);
  //magic number is seconds in a day
  long day = 86400;
  long timeDiff = static_cast<long int> ((present - m_rateCheck[producedBy] ));
  if(!(timeDiff < day)){
    return false;
  }
  // 1. check format: name, payload: all the TLVs are there
  // to do this: decode it into our record class
  // 2. check signature
  //  --- look into the key-locator field Data: so that you know which key be used to verify
  //  --- key-locator: name of the key
  //  *--- add new field to the content: record-id: certificate record
  // it must have a sig signed by a certificate in the ledger
  // 3. check rate limit -- state in memory, dict: peer-id, timestamp of latest record
  return true;
}

void
LedgerImpl::onRequestedData(const Interest& interest, const Data& data)
{
  // Context: this peer sent a Interest to ask for a Data
  // this function is to handle the replied Data.
  if(!checkValidityOfRecord(data)){
    std::cout << "Requested data malformed";
    return;
  } else {
    auto producedBy = data.getName().get(-3).toUri();
    std::time_t present = std::time(0);
    m_rateCheck[producedBy] = present;
    m_backend.putRecord((make_shared<Data>(data)));
  }
  // maybe a static function outside this fun but in the same cpp file
  // checkValidityOfRecord
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  // Context: you received a Interest packet which contains a list of tailing records
  //Assumes that the tailing list is the last part of the name
  ndn::name::Component lastComponent = interest.getName().get(-1);
  lastComponent.wireDecode(lastComponent.blockFromValue());
  std::vector<ndn::Name> receivedTail;
  std::vector<ndn::Name> diff;
  std::set_difference(m_tailingRecords.begin(), m_tailingRecords.end(), receivedTail.begin(), receivedTail.end(),
                      std::inserter(diff, diff.begin()));
  //really what we want to do is make a new vector of names, and add any names the two don't have incommon.
  //if they're the same, nothing to do
  //else send all tailing records? what if there are additional records that aren't tailing?
  if (true){
    //return success?
  } else {

  }
  // you should compare your own tailing records with this one
  // if not same:
  // 1. you see a new record that is not in your, go fetch it and all the further records until all of them are in your ledger
  // 2. you see a record but it's no longer a tailing record in your ledger, then you send your SyncRequest

  // in 1. whenever you get a new record, do record check
}

void
LedgerImpl::onRecordRequest(const Interest& interest)
{
  // Context: you received an Interest asking for a record
  ndn::Name recordName;
  recordName.wireDecode(interest.getName().get(-1).blockFromValue());
  auto dataPtr = m_backend.getRecord(recordName);
  if(dataPtr){
    m_network.put(*dataPtr);
  } else {
    //do nothing
  }
  // check whether you have it, if yes, reply
  // if not, drop it
}

std::unique_ptr<Ledger>
Ledger::initLedger(const Config& config, security::KeyChain& keychain, Face& face)
{
  return std::make_unique<LedgerImpl>(config, keychain, face);
}



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