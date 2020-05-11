#include "ledger-impl.hpp"

#include <algorithm>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>
#include <random>
#include <sstream>

using namespace ndn;
namespace dledger {

const static size_t DEFAULT_GENESIS_BLOCKS = 3;

LedgerImpl::LedgerImpl(const Config& config,
                       security::KeyChain& keychain,
                       Face& network, std::string id)
    : Ledger(),
      m_config(config),
      m_keychain(keychain),
      m_network(network),
      m_id(id),
      m_scheduler(network.getIoService())
{
  std::cout << "\nDLedger Initialization Start" << std::endl;
  //****STEP 1****
  // Initialize Database
  std::string dbDir = "/tmp/dledger-db/" + readString(m_config.peerPrefix.get(-1));
  m_backend.initDatabase(dbDir);
  std::cout << "STEP 1" << std::endl
            << "- LevelDB at " << dbDir << " has been initialized." << std::endl;

  //****STEP 2****
  // Register the prefix to local NFD
  Name syncName = m_config.multicastPrefix;
  syncName.append("SYNC");
  Name notifName = m_config.multicastPrefix;
  notifName.append("NOTIF");
  m_network.setInterestFilter(m_config.peerPrefix, bind(&LedgerImpl::onRecordRequest, this, _2), nullptr, nullptr);
  m_network.setInterestFilter(syncName, bind(&LedgerImpl::onLedgerSyncRequest, this, _2), nullptr, nullptr);
  m_network.setInterestFilter(notifName, bind(&LedgerImpl::onNewRecordNotification, this, _2), nullptr, nullptr);
  std::cout << "STEP 2" << std::endl
            << "- Prefixes " << m_config.peerPrefix.toUri() << ","
            << syncName.toUri() << ","
            << notifName.toUri()
            << " have been registered." << std::endl;

  //****STEP 3****
  // Make the genesis data
  for (int i = 0; i < DEFAULT_GENESIS_BLOCKS; i++) {
    Name recordName("/genesis/" + std::to_string(i));
    auto data = make_shared<Data>(recordName);
    auto contentBlock = makeEmptyBlock(tlv::Content);
    data->setContent(contentBlock);
    m_keychain.sign(*data, security::signingWithSha256());
    m_tailingRecords.push_back(data->getFullName());
    m_backend.putRecord(data);
  }
  std::cout << "STEP 3" << std::endl
            << "- " << DEFAULT_GENESIS_BLOCKS << " genesis records have been added to the DLedger" << std::endl
            << "DLedger Initialization Succeed\n\n";

  this->sendPerodicSyncInterest();
  this->startPerodicAddRecord();
}

LedgerImpl::~LedgerImpl()
{
}

ReturnCode
LedgerImpl::addRecord(Record& record)
{
  std::cout << "[LedgerImpl::addRecord] Add new record" << std::endl;
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
  auto tailingRecordsCopy = m_tailingRecords;
  for (; counter < m_config.preceidingRecordNum && iterator < m_tailingRecords.size(); counter++) {
    const auto& recordId = m_tailingRecords[iterator];
    if (m_config.peerPrefix.isPrefixOf(recordId)) {
      counter--;
      iterator++;
      continue;
    }
    record.addPointer(recordId);
    //m_tailingRecords.erase(m_tailingRecords.begin() + iterator);
  }
  if (counter < m_config.preceidingRecordNum) {
    m_tailingRecords = tailingRecordsCopy;
    return ReturnCode::notEnoughTailingRecord();
  }
  // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>
  // each <> represent only one component
  Name dataName = m_config.peerPrefix;
  dataName.append(recordTypeToString(record.m_type)).append(record.m_uniqueIdentifier).appendTimestamp();
  auto data = make_shared<Data>(dataName);
  auto contentBlock = makeEmptyBlock(tlv::Content);
  record.wireEncode(contentBlock);
  data->setContent(contentBlock);
  data->setFreshnessPeriod(time::minutes(5));

  // sign the packet with peer's key
  try {
    m_keychain.sign(*data, security::signingByIdentity(m_config.peerPrefix));
  }
  catch (const std::exception& e) {
    m_tailingRecords = tailingRecordsCopy;
    return ReturnCode::signingError(e.what());
  }
  record.m_data = data;

  // send out notification: /multicastPrefix/NOTIF/record-name/<digest>
  Name intName(m_config.multicastPrefix);
  intName.append("NOTIF").append(data->getFullName().wireEncode());
  Interest interest(intName);
  interest.setCanBePrefix(false);
  interest.setMustBeFresh(true);
  try {
    m_keychain.sign(interest, security::signingByIdentity(m_config.peerPrefix));
  }
  catch (const std::exception& e) {
    m_tailingRecords = tailingRecordsCopy;
    return ReturnCode::signingError(e.what());
  }
  m_network.expressInterest(interest, nullptr, bind(&LedgerImpl::onNack, this, _1, _2), nullptr);

  // add new record into the ledger
  m_backend.putRecord(data);
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
  auto dataPtr = m_backend.getRecord(Name(recordName));
  if (dataPtr != nullptr) {
    return true;
  }
  return false;
}

void
LedgerImpl::onNewRecordNotification(const Interest& interest)
{
  std::cout << "[LedgerImpl::onNewRecordNotification] Receive Notification of a New Record" << std::endl;

  // TODO: verify the signature
  // if (security::verifySignature(interest, TODO: certificate of the peer))

  // /multicastPrefix/NOTIF/record-name/<digest>
  auto nameBlock = interest.getName().get(m_config.multicastPrefix.size() + 1);
  nameBlock.parse();
  Name recordName(nameBlock.get(tlv::Name));
  if (hasRecord(recordName.toUri())) {
    std::cout << "- The record is already in the local DLedger. Ignore" << std::endl;
    return;
  }
  std::cout << "- The record is not seen before. Fetch it back" << std::endl
            << "-- " << recordName.toUri() << std::endl;
  std::mt19937_64 eng{std::random_device{}()};
  std::uniform_int_distribution<> dist{10, 100};
  m_scheduler.schedule(time::milliseconds(dist(eng)), [&, recordName] {
    Interest recordInterest(recordName);
    recordInterest.setCanBePrefix(false);
    recordInterest.setMustBeFresh(true);
    m_network.expressInterest(recordInterest,
                              bind(&LedgerImpl::onRequestedData, this, _1, _2),
                              bind(&LedgerImpl::onNack, this, _1, _2),
                              bind(&LedgerImpl::onTimeout, this, _1));
  });
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
  std::cout << "[LedgerImpl::sendPerodicSyncInterest] Send SYNC Interest.\n";

  // construct SYNC Interest
  Name syncInterestName = m_config.multicastPrefix;
  syncInterestName.append("SYNC");
  Interest syncInterest(syncInterestName);
  Block appParam = makeEmptyBlock(tlv::ApplicationParameters);
  for (const auto& item : m_tailingRecords) {
    appParam.push_back(item.wireEncode());
  }
  appParam.parse();
  syncInterest.setApplicationParameters(appParam);
  syncInterest.setCanBePrefix(false);
  syncInterest.setMustBeFresh(true);
  m_keychain.sign(syncInterest, signingByIdentity(m_config.peerPrefix));
  // nullptrs for data and timeout callbacks because a sync Interest is not expecting a Data back
  m_network.expressInterest(syncInterest.setMustBeFresh(1), nullptr,
                            bind(&LedgerImpl::onNack, this, _1, _2), nullptr);

  // schedule for the next SyncInterest Sending
  m_scheduler.schedule(time::seconds(5), [this] { sendPerodicSyncInterest(); });
}

void
LedgerImpl::startPerodicAddRecord()
{
  Record record(RecordType::GenericRecord, std::to_string(std::rand()));
  record.addRecordItem(std::to_string(std::rand()));
  record.addRecordItem(std::to_string(std::rand()));
  record.addRecordItem(std::to_string(std::rand()));
  ReturnCode result = addRecord(record);
  if (!result.success()) {
    std::cout << "- Adding record error : " << result.what() << std::endl;
  }

  // schedule for the next SyncInterest Sending
  m_scheduler.schedule(time::seconds(10), [this] { startPerodicAddRecord(); });
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
  std::cout << "onRequestedData Called \n";
  // Context: this peer sent a Interest to ask for a Data
  // this function is to handle the replied Data.
  if (!checkValidityOfRecord(data)) {
    std::cout << "Requested data malformed \n";
    return;
  }
  else {
    //TODO: Additional functionality here where we, before adding it into the ledger, first check out its references
    //And also go back and ask for more, potentially not adding it at all
    auto producedBy = data.getName().get(-3).toUri();
    std::time_t present = std::time(0);
    m_rateCheck[producedBy] = present;
    m_backend.putRecord((make_shared<Data>(data)));
    m_tailingRecords.push_back(data.getFullName());
  }
  // maybe a static function outside this fun but in the same cpp file
  // checkValidityOfRecord
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  std::cout << "[LedgerImpl::onLedgerSyncRequest] Receive SYNC Interest" << std::endl;
  const auto& appParam = interest.getApplicationParameters();
  appParam.parse();
  std::cout << "- Received Tailing Record Names: \n";
  for (const auto& item : appParam.elements()) {
    Name recordName(item);
    std::cout << "-- " << recordName.toUri() << "\n";
    // Check if they're in our tailing records.
    // If they aren't, check if they're in the database.
    // If they aren't, put them in an unverified list.
    if (std::find(m_tailingRecords.begin(), m_tailingRecords.end(), recordName) != m_tailingRecords.end()) {
      std::cout << "--- This record is already in our tailing records \n";
    }
    else if (m_backend.getRecord(recordName)) {
      std::cout << "--- This record is already in our Ledger but not tailing any more \n";
    }
    else {
      Interest interestForRecord(recordName);
      interestForRecord.setCanBePrefix(false);
      interestForRecord.setMustBeFresh(true);
      std::cout << "--- Sending Record Fetching Interest " << interestForRecord.getName().toUri() << std::endl;
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
  std::cout << "[LedgerImpl::onRecordRequest] Recieve Interest to Fetch Record" << std::endl;
  auto desiredData = m_backend.getRecord(interest.getName().toUri());
  if (desiredData) {
    std::cout << "- Found desired Data, reply it." << std::endl;
    m_network.put(*desiredData);
  }
}

std::unique_ptr<Ledger>
Ledger::initLedger(const Config& config, security::KeyChain& keychain, Face& face, std::string id)
{
  return std::make_unique<LedgerImpl>(config, keychain, face, id);
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