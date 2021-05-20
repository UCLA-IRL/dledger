#include "ledger-impl.hpp"
#include "record_name.hpp"

#include <algorithm>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/time.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <random>
#include <sstream>

NDN_LOG_INIT(dledger.impl);

using namespace ndn;
namespace dledger {

int max(int a, int b) {
    return a > b ? a : b;
}

void
LedgerImpl::dumpList(const std::map<Name, TailingRecordState>& weight)
{
    NDN_LOG_TRACE("Dump " << weight.size() << " Tailing Records");
  for (const auto& item : weight) {
    NDN_LOG_TRACE((item.second.referenceVerified ? "OK " : "NO ") << item.second.refSet.size() << "\t" << item.first.toUri());
  }
}

LedgerImpl::LedgerImpl(const Config& config,
                       security::KeyChain& keychain,
                       Face& network)
    : Ledger()
    , m_config(config)
    , m_keychain(keychain)
    , m_network(network)
    , m_scheduler(network.getIoService())
    , m_backend(config.databasePath)
{
  NDN_LOG_INFO("DLedger Initialization Start");

  //****STEP 0****
  //check validity of config
  if (m_config.appendWeight > m_config.contributionWeight) {
    NDN_LOG_ERROR("invalid weight configuration");
    BOOST_THROW_EXCEPTION(std::runtime_error("invalid weight configuration"));
  }

  //****STEP 1****
  // Register the prefix to local NFD
  Name syncName = m_config.multicastPrefix;
  syncName.append("SYNC");
  m_network.setInterestFilter(m_config.peerPrefix, bind(&LedgerImpl::onRecordRequest, this, _2), nullptr, nullptr);
  m_network.setInterestFilter(syncName, bind(&LedgerImpl::onLedgerSyncRequest, this, _2), nullptr, nullptr);
  NDN_LOG_INFO("STEP 1" << std::endl
            << "- Prefixes " << m_config.peerPrefix.toUri() << ","
            << syncName.toUri()
            << " have been registered.");

  //****STEP 2****
  // Make the genesis data
  for (int i = 0; i < m_config.numGenesisBlock; i++) {
    GenesisRecord genesisRecord((std::to_string(i)));
    RecordName recordName = RecordName::generateRecordName(config, genesisRecord);
    auto data = make_shared<Data>(recordName);
    auto contentBlock = makeEmptyBlock(tlv::Content);
    genesisRecord.wireEncode(contentBlock);
    data->setContent(contentBlock);
    m_keychain.sign(*data, signingWithSha256());
    genesisRecord.m_data = data;
    addToTailingRecord(genesisRecord, true);
  }
  NDN_LOG_INFO("STEP 2" << std::endl
            << "- " << m_config.numGenesisBlock << " genesis records have been added to the DLedger");
  NDN_LOG_INFO("DLedger Initialization Succeed");

  this->sendSyncInterest();
}

LedgerImpl::~LedgerImpl()
{
    if (m_syncEventID) m_syncEventID.cancel();
}

ReturnCode
LedgerImpl::createRecord(Record& record)
{
  NDN_LOG_INFO("[LedgerImpl::addRecord] Add new record");
  if (m_tailRecords.empty()) {
    return ReturnCode::noTailingRecord();
  }
  if (!m_config.certificateManager->authorizedToGenerate()) {
      return ReturnCode::signingError("No Valid Certificate");
  }
  if (time::system_clock::now() - m_rateCheck[readString(m_config.peerPrefix.get(-1))] < m_config.recordProductionRateLimit) {
      return ReturnCode::timingError("record generation too fast");
  }

  if (record.getType() == CERTIFICATE_RECORD) {
      for (const auto& certName: m_lastCertRecords) {
          NDN_LOG_INFO("[LedgerImpl::addRecord] Certificate record: Add previous cert record: " << certName);
          record.addRecordItem(KeyLocator(certName).wireEncode());
      }
  }

  std::vector<std::pair<Name, int>> recordList;
  for (const auto &item : m_tailRecords) {
    if (item.second.refSet.size() <= m_config.appendWeight &&
        !m_config.peerPrefix.isPrefixOf(item.first) &&
        item.second.referenceVerified) {
      recordList.emplace_back(item.first, item.second.refSet.size());
    }
  }

  // fulfill the record content with preceding record IDs
  // removal of preceding record is done by addToTailingRecord() at the end
  if (recordList.size() < m_config.precedingRecordNum) {
    return ReturnCode::notEnoughTailingRecord();
  }

  // randomly shuffle the tailing record list
  std::shuffle(std::begin(recordList), std::end(recordList), m_randomEngine);

  for (const auto &tailRecord : recordList) {
    record.addPointer(tailRecord.first);
    if (record.getPointersFromHeader().size() >= m_config.precedingRecordNum)
      break;
  }

  Name dataName = RecordName::generateRecordName(m_config, record);
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
    return ReturnCode::signingError(e.what());
  }
  record.m_data = data;
  NDN_LOG_INFO("[LedgerImpl::addRecord] Added a new record:" << data->getFullName().toUri());

  // add new record into the ledger
  addToTailingRecord(record, true);

  //send sync interest
  auto rc = sendSyncInterest();
  if (rc.success())
    return ReturnCode::noError(data->getFullName().toUri());
  else return rc;
}

optional<Record>
LedgerImpl::getRecord(const std::string& recordName) const
{
  NDN_LOG_DEBUG("getRecord Called");
  Name rName = recordName;
  return getRecord(rName);
}

bool
LedgerImpl::hasRecord(const std::string& recordName) const
{
  if (m_tailRecords.count(recordName)) {
    if (!m_tailRecords.find(recordName)->second.referenceVerified) {
      return false;
    }
    return true;
  }
  auto dataPtr = m_backend.getRecord(Name(recordName));
  return dataPtr != nullptr;
}

std::list<Name>
LedgerImpl::listRecord(const std::string& prefix) const
{
    auto list = m_backend.listRecord(Name(prefix));
    list.remove_if([&](const auto& name) {return m_tailRecords.count(name) && !m_tailRecords.find(name)->second.referenceVerified;});
    return list;
}

optional<Record>
LedgerImpl::getRecord(const Name& rName) const
{
  if (m_tailRecords.count(rName)) {
    if (!m_tailRecords.find(rName)->second.referenceVerified) {
      return nullopt;
    }
    return m_tailRecords.find(rName)->second.record;
  }
  auto dataPtr = m_backend.getRecord(rName);
  if (dataPtr != nullptr) {
    return Record(dataPtr);
  }
  else {
    return nullopt;
  }
}

bool
LedgerImpl::seenRecord(const Name& recordName) const
{
  if (m_tailRecords.count(recordName)) {
    return true;
  }
  auto dataPtr = m_backend.getRecord(Name(recordName));
  return dataPtr != nullptr;
}


void
LedgerImpl::onNack(const Interest&, const lp::Nack& nack)
{
  NDN_LOG_ERROR("Received Nack with reason " << nack.getReason());
}

void
LedgerImpl::onTimeout(const Interest& interest)
{
  NDN_LOG_ERROR("Timeout for " << interest);
}

ReturnCode
LedgerImpl::sendSyncInterest() {
    NDN_LOG_INFO("[LedgerImpl::sendSyncInterest] Send SYNC Interest.");
    // SYNC Interest Name: /<multicastPrefix>/SYNC/digest
    // construct SYNC Interest
    Name syncInterestName = m_config.multicastPrefix;
    syncInterestName.append("SYNC");
    Interest syncInterest(syncInterestName);
    Block appParam = makeEmptyBlock(tlv::ApplicationParameters);
    for (const auto &certName: m_lastCertRecords) {
        appParam.push_back(KeyLocator(certName).wireEncode());
    }
    for (const auto &item : m_tailRecords) {
        if (item.second.referenceVerified && item.second.refSet.empty())
            appParam.push_back(item.first.wireEncode());
    }
    appParam.parse();
    syncInterest.setApplicationParameters(appParam);
    syncInterest.setCanBePrefix(false);
    syncInterest.setMustBeFresh(true);
    try {
        m_keychain.sign(syncInterest, signingByIdentity(m_config.peerPrefix));
    } catch (const std::exception& e) {
        return ReturnCode::signingError(e.what());
    }
    // nullptrs for data and timeout callbacks because a sync Interest is not expecting a Data back
    m_network.expressInterest(syncInterest, nullptr,
                              bind(&LedgerImpl::onNack, this, _1, _2), nullptr);


    // schedule for the next SyncInterest Sending
    if (m_syncEventID) m_syncEventID.cancel();
    m_syncEventID = m_scheduler.schedule(m_config.syncInterval, [this] { sendSyncInterest(); });
    return ReturnCode::noError();
}

bool
LedgerImpl::checkSyntaxValidityOfRecord(const Data& data) {
    NDN_LOG_INFO("[LedgerImpl::checkSyntaxValidityOfRecord] Check the format validity of the record");
    NDN_LOG_TRACE("- Step 1: Check whether it is a valid record following DLedger record spec");
    Record dataRecord;
    try {
        // format check
        dataRecord = Record(data);
        dataRecord.checkPointerCount(m_config.precedingRecordNum);
    } catch (const std::exception &e) {
        NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] The Data format is not proper for DLedger record " << dataRecord.getRecordName() << " because " << e.what());
        return false;
    }

    NDN_LOG_TRACE("- Step 2: Check signature");
    Name producerID = dataRecord.getProducerPrefix();
    if (!m_config.certificateManager->verifySignature(data)) {
        NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] Bad Signature for " << dataRecord.getRecordName());
        return false;
    }

    NDN_LOG_TRACE("- Step 3: Check rating limit");
    auto tp = dataRecord.getGenerationTimestamp();
    if (tp > time::system_clock::now() + m_config.clockSkewTolerance) {
        NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] record from too far in the future" << dataRecord.getRecordName());
        return false;
    }
    if (m_rateCheck.find(producerID) == m_rateCheck.end()) {
        m_rateCheck[producerID] = tp;
    } else {
        if ((time::abs(tp - m_rateCheck.at(producerID)) < m_config.recordProductionRateLimit)) {
            NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] record generation too fast from the peer" << dataRecord.getRecordName());
            return false;
        }
    }

    NDN_LOG_TRACE("- Step 4: Check InterLock Policy");
    for (const auto &precedingRecordName : dataRecord.getPointersFromHeader()) {
        NDN_LOG_TRACE("-- Preceding record from " << RecordName(precedingRecordName).getProducerPrefix());
        if (RecordName(precedingRecordName).getProducerPrefix() == producerID) {
            NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] Preceding record From itself: " << dataRecord.getRecordName());
            return false;
        }
    }

    NDN_LOG_TRACE("- Step 5: Check certificate/revocation record format");
    if (dataRecord.getType() == CERTIFICATE_RECORD || dataRecord.getType() == REVOCATION_RECORD) {
        if (!m_config.certificateManager->verifyRecordFormat(dataRecord)) {
            NDN_LOG_ERROR("[LedgerImpl::checkSyntaxValidityOfRecord] bad certificate/revocation record: " << dataRecord.getRecordName());
            return false;
        }
    } else {
      NDN_LOG_TRACE("-- Not a certificate/revocation record");
    }

    NDN_LOG_INFO("- All Syntax check Steps finished. Good Record");
    return true;
}

bool
LedgerImpl::checkEndorseValidityOfRecord(const Data& data) {
    NDN_LOG_INFO("[LedgerImpl::checkEndorseValidityOfRecord] Check the reference validity of the record");
    Record dataRecord;
    try {
        // format check
        dataRecord = Record(data);
    } catch (const std::exception& e) {
        NDN_LOG_INFO("-- The Data format is not proper for DLedger record because " << e.what());
        return false;
    }

    NDN_LOG_TRACE("- Step 6: Check Revocation");
    if (!m_config.certificateManager->endorseSignature(data)) {
        NDN_LOG_INFO("[LedgerImpl::checkEndorseValidityOfRecord] certificate revoked for " << dataRecord.getRecordName());
        return false;
    }

    NDN_LOG_TRACE("- Step 7: Check Contribution Policy");
    for (const auto& precedingRecordName : dataRecord.getPointersFromHeader()) {
        if (m_tailRecords.count(precedingRecordName) != 0) {
            NDN_LOG_TRACE("-- Preceding record " << precedingRecordName << " has weight " << m_tailRecords[precedingRecordName].refSet.size());
            if (m_tailRecords[precedingRecordName].refSet.size() > m_config.contributionWeight) {
                NDN_LOG_WARN("[LedgerImpl::checkEndorseValidityOfRecord] Weight too high for " << dataRecord.getRecordName() << " with weight " << m_tailRecords[precedingRecordName].refSet.size());
                return false;
            }
        } else {
            if (m_backend.getRecord(precedingRecordName) != nullptr) {
                NDN_LOG_WARN("[LedgerImpl::checkEndorseValidityOfRecord] Preceding record " << precedingRecordName << " too deep");
            } else {
                NDN_LOG_WARN("[LedgerImpl::checkEndorseValidityOfRecord] Preceding record " << precedingRecordName << " Not found");
            }
            return false;
        }
    }

    NDN_LOG_TRACE("- Step 8: Check App Logic");
    if (m_onRecordAppCheck != nullptr && !m_onRecordAppCheck(data)) {
        NDN_LOG_ERROR("-- App Logic check failed");
        return false;
    }

    NDN_LOG_INFO("- All Reference Check Steps finished. Good Record");
    return true;
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  // verify the signature
  if (!m_config.certificateManager->verifySignature(interest)) {
      NDN_LOG_ERROR("[LedgerImpl::onLedgerSyncRequest] Bad SYNC Signature: " << interest.getName());
      return;
  }
  NDN_LOG_INFO("[LedgerImpl::onLedgerSyncRequest] Receive SYNC Interest");

  //cancel previous reply
  if (m_replySyncEventID) m_replySyncEventID.cancel();

  const auto& appParam = interest.getApplicationParameters();
  appParam.parse();
  bool shouldSendSync = false;
  bool isCertPending = false;
  for (const auto& item : appParam.elements()) {
    if (item.type() == tlv::KeyLocator) {
        try {
            auto l = KeyLocator(item);
            RecordName certName(l.getName());
            if (certName.getRecordType() != CERTIFICATE_RECORD) {
                BOOST_THROW_EXCEPTION(std::runtime_error(""));
            }
            if (!seenRecord(certName)) {
                NDN_LOG_INFO("[LedgerImpl::onLedgerSyncRequest] Fetch unseen certificate record "<< l.getName());
                fetchRecord(certName);
                isCertPending = true;
            }
        } catch (const std::exception& e) {
            NDN_LOG_ERROR("[LedgerImpl::onLedgerSyncRequest] Error on keyLocator");
        }
        continue;
    }
    if (isCertPending) continue;
    Name recordName(item);
    if (m_tailRecords.count(recordName) != 0 && m_tailRecords[recordName].refSet.empty()) {
      NDN_LOG_TRACE("--- " << recordName.toUri() << " is already in our tailing records");
    }
    else if (seenRecord(recordName)) {
      NDN_LOG_TRACE("--- " << recordName.toUri() << " is already in our Ledger but not tailing any more");
      shouldSendSync = true;
    }
    else {
        NDN_LOG_TRACE("--- " << recordName.toUri() << "is unseen. Fetch");
        //fetch record
        fetchRecord(recordName);
    }
  }
  if (shouldSendSync) {
      NDN_LOG_INFO("[LedgerImpl::onLedgerSyncRequest] send Sync interest so others can fetch new record");
      std::uniform_int_distribution<> dist{10, 200};
      m_replySyncEventID = m_scheduler.schedule(time::milliseconds(dist(m_randomEngine)), [&] {
          sendSyncInterest();
      });
  }
}

void
LedgerImpl::onRecordRequest(const Interest& interest)
{
  auto desiredData = getRecord(interest.getName());
  if (desiredData) {
    NDN_LOG_INFO("[LedgerImpl::onRecordRequest] Reply Data: " << interest.getName());
    m_network.put(*desiredData->m_data);
  } else {
    NDN_LOG_ERROR("[LedgerImpl::onRecordRequest] Data not Found: " << interest.getName());
  }
}

void
LedgerImpl::fetchRecord(const Name& recordName)
{
  Interest interestForRecord(recordName);
  interestForRecord.setCanBePrefix(false);
  interestForRecord.setMustBeFresh(true);
  NDN_LOG_INFO("[LedgerImpl::fetchRecord] Fetch the record: " << interestForRecord.getName().toUri());
  m_network.expressInterest(interestForRecord,
                            bind(&LedgerImpl::onFetchedRecord, this, _1, _2),
                            bind(&LedgerImpl::onNack, this, _1, _2),
                            bind(&LedgerImpl::onTimeout, this, _1));
}

void
LedgerImpl::onFetchedRecord(const Interest& interest, const Data& data)
{
  if (seenRecord(data.getFullName())) {
    NDN_LOG_INFO("[LedgerImpl::onFetchedRecord] Record already exists in the ledger. Ignore " << data.getFullName());
    return;
  }
  if (m_badRecords.count(data.getFullName()) != 0) {
      NDN_LOG_INFO("[LedgerImpl::onFetchedRecord] Known bad record. Ignore " << data.getFullName());
      return;
  }
  for (const auto& stackRecord : m_syncStack) {
      if (stackRecord.first.getRecordName() == data.getFullName()) {
          NDN_LOG_INFO("[LedgerImpl::onFetchedRecord] Record in sync stack already. Ignore " << data.getFullName());
          return;
      }
  }
  NDN_LOG_INFO("[LedgerImpl::onFetchedRecord] fetched new record " << data.getFullName());

  try {
      Record record(data);
      if (record.getType() == RecordType::GENESIS_RECORD) {
          throw std::runtime_error("We should not get Genesis record");
      }

      if (!checkSyntaxValidityOfRecord(data)) {
          throw std::runtime_error("Record Syntax error");
      }

      m_syncStack.emplace_back(record, time::system_clock::now());
      auto precedingRecordNames = record.getPointersFromHeader();
      bool allPrecedingRecordsInLedger = true;
      for (const auto &precedingRecordName : precedingRecordNames) {
          if (seenRecord(precedingRecordName)) {
              NDN_LOG_TRACE("- Preceding Record " << precedingRecordName << " already in the ledger");
          } else {
              allPrecedingRecordsInLedger = false;
              fetchRecord(precedingRecordName);
          }
      }
      if (record.getType() == CERTIFICATE_RECORD) {
          NDN_LOG_INFO("- Checking previous cert record");
          CertificateRecord certRecord(record);
          for (const auto &prevCertName : certRecord.getPrevCertificates()) {
              if (prevCertName.empty()) continue;
              if (seenRecord(prevCertName)) {
                  NDN_LOG_TRACE("- Preceding Cert Record " << prevCertName << " already in the ledger");
              } else {
                  NDN_LOG_TRACE("- Preceding Cert Record " << prevCertName << " unseen");
                  allPrecedingRecordsInLedger = false;
                  fetchRecord(prevCertName);
              }
          }
      }

      if (!allPrecedingRecordsInLedger) {
          NDN_LOG_INFO("- Waiting for record to be added");
          return;
      }

  } catch (const std::exception& e) {
      NDN_LOG_ERROR("- The Data format of " << data.getFullName() << " is not proper for DLedger record because " << e.what());
      m_badRecords.insert(data.getFullName());
      return;
  }

  int stackSize = INT_MAX;
  while (stackSize != m_syncStack.size()) {
      stackSize = m_syncStack.size();
      NDN_LOG_INFO("[LedgerImpl::onFetchedRecord] SyncStack size " << m_syncStack.size());
      for (auto it = m_syncStack.begin(); it != m_syncStack.end();) {
          if (checkRecordAncestor(it->first)) {
              it = m_syncStack.erase(it);
          } else if(time::abs(time::system_clock::now() - it->second) > m_config.ancestorFetchTimeout){
              NDN_LOG_WARN("[LedgerImpl::onFetchedRecord] Timeout on fetching ancestor for " << it->first.getRecordName().toUri());
              it = m_syncStack.erase(it);
          } else {
              // else, some preceding records are not yet fetched
              it++;
          }
      }
  }
}

bool
LedgerImpl::checkRecordAncestor(const Record &record) {
    bool readyToAdd = true;
    bool badRecord = false;
    for (const auto& precedingRecordName : record.getPointersFromHeader()) {
        if (m_badRecords.count(precedingRecordName) != 0) {
            // has preceding record being bad record
            badRecord = true;
            readyToAdd = false;
            break;
        }
        if (!hasRecord(precedingRecordName.toUri())) {
            readyToAdd = false;
            break;
        }
    }
    if (record.getType() == CERTIFICATE_RECORD) {
        CertificateRecord certRecord(record);
        for (const auto &prevCertName : certRecord.getPrevCertificates()) {
            if (!prevCertName.empty() && !seenRecord(prevCertName)) {
                readyToAdd = false;
            }
        }
    }
    if (!badRecord && readyToAdd) {
        addToTailingRecord(record, checkEndorseValidityOfRecord(*(record.m_data)));
        return true;
    }
    if (badRecord) {
        NDN_LOG_TRACE("[LedgerImpl::checkRecordAncestor] Bad record: " << record.getRecordName());
        m_badRecords.insert(record.getRecordName());
        return true;
    }
    return false;
}

void
LedgerImpl::addToTailingRecord(const Record& record, bool endorseVerified) {
    if (m_tailRecords.count(record.getRecordName()) != 0) {
        NDN_LOG_INFO("[LedgerImpl::addToTailingRecord] Repeated add record: " << record.getRecordName());
        return;
    }
    NDN_LOG_TRACE("[LedgerImpl::addToTailingRecord] adding record" << record.getRecordName());

  //verify if ancestor has correct reference policy
    bool refVerified = endorseVerified;
    if (endorseVerified) {
        for (const auto &precedingRecord : record.getPointersFromHeader()) {
            if (m_tailRecords.count(precedingRecord) != 0 &&
                !m_tailRecords[precedingRecord].referenceVerified) {
                refVerified = false;
                break;
            }
        }
    }

    //add record to tailing record
    m_tailRecords[record.getRecordName()] = TailingRecordState{refVerified, std::set<Name>(), endorseVerified,
                                                               record, time::system_clock::now()};

    //update weight of the system
    std::stack<Name> stack;
    std::set<Name> updatedRecords;

    //only count the weight if the record is valid for all policies
    if (endorseVerified) {
        stack.push(record.getRecordName());
    }
    while (!stack.empty()) {
        RecordName currentRecordName(stack.top());
        stack.pop();
        if (currentRecordName.getRecordType() == GENESIS_RECORD) continue;
        Record currentRecord = *getRecord(currentRecordName);
        auto precedingRecordList = currentRecord.getPointersFromHeader();
        for (const auto &precedingRecord : precedingRecordList) {
            if (RecordName(precedingRecord).getProducerPrefix() == record.getProducerPrefix()) continue;
            if (m_tailRecords.count(precedingRecord) != 0 &&
                m_tailRecords[precedingRecord].refSet.insert(record.getProducerPrefix()).second) {
                stack.push(precedingRecord);
                updatedRecords.insert(precedingRecord);
                NDN_LOG_TRACE("[LedgerImpl::addToTailingRecord]" << record.getProducerPrefix() << " confirms " << precedingRecord.toUri());
            }
        }
    }

    //remove deep records
    int removeWeight = max(m_config.contributionWeight + 1, m_config.confirmWeight);
    bool referenceNeedUpdate = false;
    for (const auto & updatedRecord : updatedRecords) {
        auto& tailingState = m_tailRecords[updatedRecord];
        if (tailingState.refSet.size() == m_config.confirmWeight) {
            NDN_LOG_INFO("[LedgerImpl::addToTailingRecord]" << updatedRecord.toUri()  << " is confirmed");
            if (!tailingState.referenceVerified) {
                tailingState.referenceVerified = true;
                referenceNeedUpdate = true;
            }
            onRecordConfirmed(*getRecord(updatedRecord));
        }
        if (tailingState.refSet.size() >= removeWeight) {
            m_tailRecords.erase(updatedRecord);
        }
    }

    //update reference policy
    while (referenceNeedUpdate) {
        referenceNeedUpdate = false;
        for (auto &r : m_tailRecords) {
            if (!r.second.referenceVerified && r.second.endorseVerified) {
                bool referenceVerified = true;
                Record currentRecord(*getRecord(r.first));
                for (const auto &precedingRecord : currentRecord.getPointersFromHeader()) {
                    if ((m_tailRecords.count(precedingRecord) &&
                            m_tailRecords[precedingRecord].refSet.size() < m_config.confirmWeight) &&
                        !m_tailRecords[precedingRecord].referenceVerified) {
                        referenceVerified = false;
                    }
                }

                if (referenceVerified) {
                    r.second.referenceVerified = referenceVerified;
                    referenceNeedUpdate = true;
                }
            }
        }
    }

    removeTimeoutRecords();

    dumpList(m_tailRecords);
}

void
LedgerImpl::onRecordConfirmed(const Record &record){
    NDN_LOG_INFO("[LedgerImpl::onRecordConfirmed] accept record" << record.getRecordName());

    //register current time
    if (m_rateCheck[record.getProducerPrefix()] < record.getGenerationTimestamp())
            m_rateCheck[record.getProducerPrefix()] = record.getGenerationTimestamp();

    //add to backend database
    m_backend.putRecord(record.m_data);

    if (record.getType() == RecordType::CERTIFICATE_RECORD) {
        try {
            auto certRecord = CertificateRecord(record);
            m_lastCertRecords.push_back(certRecord.getRecordName());
            for (const auto &c : certRecord.getPrevCertificates()) {
                m_lastCertRecords.remove(c);
            }
        } catch (const std::exception &e) {
            NDN_LOG_ERROR("[LedgerImpl::onRecordConfirmed] Bad certificate record format for " << record.getRecordName());
            return;
        }
    }

    if (record.getType() == RecordType::CERTIFICATE_RECORD || record.getType() == RecordType::REVOCATION_RECORD) {
        m_config.certificateManager->acceptRecord(record);
    }

    if (m_onRecordAppConfirmed != nullptr) {
        m_onRecordAppConfirmed(record);
    }
}

void
LedgerImpl::removeTimeoutRecords()
{
  std::set<Name> timeoutList;
  auto timeBefore = time::system_clock::now() - m_config.blockConfirmationTimeout;
  for (const auto& record : m_tailRecords) {
    if (record.second.refSet.size() < m_config.confirmWeight && //unconfirmed
        record.second.addedTime < timeBefore) {
      timeoutList.insert(record.first);
    }
  }

  while (!timeoutList.empty()) {
    for (const auto& i : timeoutList) {
      m_tailRecords.erase(i);
      NDN_LOG_INFO("[LedgerImpl::removeTimeoutRecords] remove timeout record " << i);
    }
    std::set<Name> childrenList;
    for (const auto& record : m_tailRecords) {
      for (const auto& parent : record.second.record.getPointersFromHeader()) {
        if (timeoutList.count(parent)) { // if parent in the set
          childrenList.insert(record.first); //child should be removed
        }
      }
    }
    timeoutList.swap(childrenList);
  }
}

std::unique_ptr<Ledger>
Ledger::initLedger(const Config& config, security::KeyChain& keychain, Face& face)
{
  return std::make_unique<LedgerImpl>(config, keychain, face);
}

}  // namespace dledger