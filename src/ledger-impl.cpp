#include "ledger-impl.hpp"

#include <time.h> /* time */

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>  // Include boost::for is_any_of
#include <boost/algorithm/string/split.hpp>           // Include for boost::split
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>
#include <random>

using namespace ndn;
namespace dledger {

LedgerImpl::LedgerImpl(const Config& config,
                       security::KeyChain& keychain,
                       Face& network)
    : Ledger(),
      m_config(config),
      m_keychain(keychain),
      m_network(network)
//consider adding a producer identity member?
/*
   m_face.setInterestFilter("/example/testApp",
                             bind(&Producer::onInterest, this, _1, _2),
                             nullptr, // RegisterPrefixSuccessCallback is optional
                             bind(&Producer::onRegisterFailed, this, _1, _2));
  */
{
  std::string dbName = LedgerImpl::random_string(2);
  std::cout << "db name: " << dbName << "\n";
  m_backend.initDatabase(dbName);
  std::cout << "in constructor \n";
  Name syncName = m_config.multicastPrefix;
  syncName.append("SYNC");
  Name notifName = m_config.multicastPrefix;
  notifName.append("NOTIF");

  m_network.registerPrefix(m_config.multicastPrefix, nullptr, nullptr);
  std::cout << "prefix registered \n";

  m_network.setInterestFilter(m_config.multicastPrefix, bind(&LedgerImpl::onRecordRequest, this, _2), nullptr, nullptr);
  m_network.setInterestFilter(syncName, bind(&LedgerImpl::onLedgerSyncRequest, this, _2));
  m_network.setInterestFilter(notifName, bind(&LedgerImpl::onNewRecordNotification, this, _2));
  std::cout << "interest filters set \n";
  sendPerodicSyncInterest();
}

LedgerImpl::~LedgerImpl()
{
}

ReturnCode
LedgerImpl::addRecord(Record& record, const Name& signerIdentity)
{
  std::cout << "adding record \n";
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
    if (m_config.peerPrefix.isPrefixOf(recordId)) {
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
  std::cout << "shuffled tailing records \n";
  // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>
  // each <> represent only one component
  Name dataName = m_config.peerPrefix;
  dataName.append(std::to_string(record.m_type)).append(record.m_uniqueIdentifier).appendTimestamp();
  auto data = make_shared<Data>(dataName);
  auto contentBlock = makeEmptyBlock(tlv::Content);
  record.wireEncode(contentBlock);
  data->setContent(contentBlock);

  // sign the packet with peer's key
  try {
    m_keychain.sign(*data, security::signingByIdentity(signerIdentity));
  }
  catch (const std::exception& e) {
    return ReturnCode::signingError(e.what());
  }
  record.m_data = data;

  std::cout << "about to putRecord \n";

  // add new record into the ledger
  m_backend.putRecord(data);

  // send out notification: /multicastPrefix/NOTIF/record-name/<digest>
  Name intName(m_config.multicastPrefix);
  intName.append("NOTIF").append(data->getFullName().wireEncode());
  std::cout << data->getFullName().toUri();
  Interest interest(intName);
  try {
    m_keychain.sign(interest, security::signingByIdentity(signerIdentity));
  }
  catch (const std::exception& e) {
    return ReturnCode::signingError(e.what());
  }
  m_network.expressInterest(interest, nullptr, nullptr, nullptr);

  return ReturnCode::noError();
}

optional<Record>
LedgerImpl::getRecord(const std::string& recordName)
{
  std::cout << "getRecord Called \n";
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
  std::cout << "hasRecord Called \n";
  auto dataPtr = m_backend.getRecord(Name(recordName));
  if (dataPtr != nullptr) {
    return true;
  }
  return false;
}

void
LedgerImpl::onNewRecordNotification(const Interest& interest)
{
  std::cout << "newRecordNotification Called \n";
  //Temporary stand-in.
  security::v2::Certificate certInLedger;
  //put back in later
  /*if(!security::verifySignature(interest, certInLedger)){
    std::cout << "Signature wasn't verified in onNewRecord";
  } else {
    std::cout << "but not that hard ";
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
  */
  // verify the signature
  // if (security::verifySignature(interest, TODO: certificate of the peer))
  // extract the Record Data name from the interest

  // /multicastPrefix/NOTIF/record-name/<digest>
  auto nameBlock = interest.getName().get(m_config.multicastPrefix.size() + 1).toUri();
  std::mt19937_64 eng{std::random_device{}()};
  std::uniform_int_distribution<> dist{10, 100};
  sleep(dist(eng) / 100);
  //chop off NOTIF bit
  ndn::Name interestForRecordName(nameBlock);
  std::cout << "did i get interest name? \n";
  std::cout << interestForRecordName.toUri();
  std::cout << "\n";
  Interest interestForRecord(interestForRecordName);

  std::cout << "Sending Interest " << interestForRecord << std::endl;
  m_network.expressInterest(interestForRecord,
                            bind(&LedgerImpl::onRequestedData, this, _1, _2),
                            bind(&LedgerImpl::onNack, this, _1, _2),
                            bind(&LedgerImpl::onTimeout, this, _1));

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
  std::cout << "sendPeriodicInterest Called \n";
  Name syncInterestName = m_config.multicastPrefix;
  syncInterestName.append("SYNC");
  Interest syncInterest(syncInterestName);
  std::string appParameters = "";
  for (const auto& item : m_tailingRecords) {
    appParameters += item.toUri();
    appParameters += "\n";
  }
  syncInterest.setApplicationParameters((uint8_t*)appParameters.c_str(), appParameters.size());
  m_keychain.sign(syncInterest, signingByIdentity(m_config.peerPrefix));
  // nullptrs for callbacks because a sync Interest is not expecting a Data back
  m_network.expressInterest(syncInterest, nullptr, nullptr, nullptr);
  std::cout << "reached end of sendPeriodic\n";
  std::cout << syncInterest.getName().toUri() << "\n";
  // @todo
  // scheduler.schedule();
}

bool
LedgerImpl::checkValidityOfRecord(const Data& data)
{
  std::cout << "checkValidity Called";
  try {
    dledger::Record dataRecord = dledger::Record(data);
  }
  catch (const std::exception& e) {
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
  long timeDiff = static_cast<long int>((present - m_rateCheck[producedBy]));
  if (!(timeDiff < day)) {
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
  std::cout << "onRequestedData Called";
  // Context: this peer sent a Interest to ask for a Data
  // this function is to handle the replied Data.
  if (!checkValidityOfRecord(data)) {
    std::cout << "Requested data malformed";
    return;
  }
  else {
    //TODO: Additional functionality here where we, before adding it into the ledger, first check out its references
    //And also go back and ask for more, potentially not adding it at all
    auto producedBy = data.getName().get(-3).toUri();
    std::time_t present = std::time(0);
    m_rateCheck[producedBy] = present;
    m_backend.putRecord((make_shared<Data>(data)));
  }
  // maybe a static function outside this fun but in the same cpp file
  // checkValidityOfRecord
}

//
std::string
LedgerImpl::random_string(size_t length)
{
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  std::cout << "onLedgerSync Called \n";
  // Context: you received a Interest packet which contains a list of tailing records
  const auto& tailFromParam = encoding::readString(interest.getApplicationParameters());
  std::cout << "encoding worked \n";
  //Okay, so we can turn it into a string -- now we need to take that string, separate it into a bunch of strings (demarcated by \n))
  //then for each one of those, check if we have it in our tailing records, if we do, good. if not, go search for it and all further records
  //How to get: Use record.getPointersFromHeader to get back a list of names.
  //how to make it fully recursive, though? and need to ensure that all preceeding records valid, otherwise you get rid of it...
  std::vector<std::string> receivedTail;
  boost::split(receivedTail, tailFromParam, boost::is_any_of("\n"), boost::token_compress_on);
  std::vector<ndn::Name> namedReceivedTail;
  //First, make them NDN names
  for (int i = 0; i < receivedTail.size(); i++) {
    std::cout << receivedTail[i] << "\n";
    namedReceivedTail.push_back(ndn::Name(receivedTail[i]));
  }
  //Check if they're in our tailing records. If they aren't, check if they're in the database. If they aren't, put them in an unverified list.
  for (int i = 0; i < receivedTail.size(); i++) {
    if (std::find(m_tailingRecords.begin(), m_tailingRecords.end(), receivedTail[i]) != m_tailingRecords.end()) {
      std::cout << "This record is already in our ledger";
    }
    else if (m_backend.getRecord(receivedTail[i])) {
      sendPerodicSyncInterest();
    }
    else {
      ndn::Name interestForRecordName;
      interestForRecordName = (interest.getName().getPrefix(-1));
      interestForRecordName.append(Name(receivedTail[i]));
      Interest interestForRecord(interestForRecordName);

      std::cout << "Sending Interest " << interestForRecord.getName().toUri() << std::endl;
      m_network.expressInterest(interestForRecord,
                                bind(&LedgerImpl::onRequestedData, this, _1, _2),
                                bind(&LedgerImpl::onNack, this, _1, _2),
                                bind(&LedgerImpl::onTimeout, this, _1));
    }
  }
  //really what we want to do is make a new vector of names, and add any names the two don't have in common.
  //if they're the same, nothing to do
  //else send all tailing records? what if there are additional records that aren't tailing?

  // you should compare your own tailing records with this one
  // if not same:
  // 1. you see a new record that is not in your, go fetch it and all the further records until all of them are in your ledger
  // 2. you see a record but it's no longer a tailing record in your ledger, then you send your SyncRequest

  // in 1. whenever you get a new record, do record check
}

void
LedgerImpl::onRecordRequest(const Interest& interest)
{
  std::cout << "onRecordRequest Called \n";
  // Context: you received an Interest asking for a record
  ndn::Name recordName;
  recordName.wireDecode(interest.getName().get(-1).blockFromValue());
  auto dataPtr = m_backend.getRecord(recordName);
  if (dataPtr) {
    m_network.put(*dataPtr);
  }
  else {
    //do nothing
  }
  // check whether you have it, if yes, reply
  // if not, drop it
}

std::unique_ptr<Ledger>
Ledger::initLedger(const Config& config, security::KeyChain& keychain, Face& face)
{
  std::cout << "ledger init \n";
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

}  // namespace dledger