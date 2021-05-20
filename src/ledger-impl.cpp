#include "ledger-impl.hpp"
#include "record_name.hpp"

#include <algorithm>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/time.hpp>
#include <random>
#include <sstream>

using namespace ndn;
namespace dledger {

int max(int a, int b) {
    return a > b ? a : b;
}

void
LedgerImpl::dumpList(const std::map<Name, TailingRecordState>& weight)
{
    std::cout << "Dump " << weight.size() << " Tailing Records" << std::endl;
  for (const auto& item : weight) {
    std::cout << (item.second.referenceVerified ? "OK " : "NO ") << item.second.refSet.size() << "\t" << item.first.toUri() << std::endl;
  }
  std::cout << std::endl << std::endl;
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
  std::cout << "\nDLedger Initialization Start" << std::endl;

  //****STEP 0****
  //check validity of config
  if (m_config.appendWeight > m_config.contributionWeight) {
    std::cerr << "invalid weight configuration" << std::endl;
    BOOST_THROW_EXCEPTION(std::runtime_error("invalid weight configuration"));
  }

  //****STEP 1****
  // Register the prefix to local NFD
  Name syncName = m_config.multicastPrefix;
  syncName.append("SYNC");
  m_network.setInterestFilter(m_config.peerPrefix, bind(&LedgerImpl::onRecordRequest, this, _2), nullptr, nullptr);
  m_network.setInterestFilter(syncName, bind(&LedgerImpl::onLedgerSyncRequest, this, _2), nullptr, nullptr);
  std::cout << "STEP 1" << std::endl
            << "- Prefixes " << m_config.peerPrefix.toUri() << ","
            << syncName.toUri()
            << " have been registered." << std::endl;

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
  std::cout << "STEP 2" << std::endl
            << "- " << m_config.numGenesisBlock << " genesis records have been added to the DLedger" << std::endl
            << "DLedger Initialization Succeed\n\n";

  this->sendSyncInterest();
}

LedgerImpl::~LedgerImpl()
{
    if (m_syncEventID) m_syncEventID.cancel();
}

ReturnCode
LedgerImpl::createRecord(Record& record)
{
  std::cout << "[LedgerImpl::addRecord] Add new record" << std::endl;
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
          std::cout << "-- Certificate record: Add previous cert record: " << certName << std::endl;
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
  std::cout << "- Finished the generation of the new record:" << std::endl
            << "Name: " << data->getFullName().toUri() << std::endl;

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
  std::cout << "getRecord Called \n";
  Name rName = recordName;
  if (m_tailRecords.count(rName) && !m_tailRecords.find(rName)->second.referenceVerified) {
      return nullopt;
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
LedgerImpl::hasRecord(const std::string& recordName) const
{
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

ReturnCode LedgerImpl::sendSyncInterest() {
    std::cout << "[LedgerImpl::sendSyncInterest] Send SYNC Interest.\n";
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
    std::cout << "[LedgerImpl::checkSyntaxValidityOfRecord] Check the format validity of the record" << std::endl;
    std::cout << "- Step 1: Check whether it is a valid record following DLedger record spec" << std::endl;
    Record dataRecord;
    try {
        // format check
        dataRecord = Record(data);
        dataRecord.checkPointerCount(m_config.precedingRecordNum);
    } catch (const std::exception &e) {
        std::cout << "-- The Data format is not proper for DLedger record because " << e.what() << std::endl;
        return false;
    }

    std::cout << "- Step 2: Check signature" << std::endl;
    Name producerID = dataRecord.getProducerPrefix();
    if (!m_config.certificateManager->verifySignature(data)) {
        std::cout << "-- Bad Signature." << std::endl;
        return false;
    }

    std::cout << "- Step 3: Check rating limit" << std::endl;
    auto tp = dataRecord.getGenerationTimestamp();
    if (tp > time::system_clock::now() + m_config.clockSkewTolerance) {
        std::cout << "-- record from too far in the future" << std::endl;
        return false;
    }
    if (m_rateCheck.find(producerID) == m_rateCheck.end()) {
        m_rateCheck[producerID] = tp;
    } else {
        if ((time::abs(tp - m_rateCheck.at(producerID)) < m_config.recordProductionRateLimit)) {
            std::cout << "-- record generation too fast from the peer" << std::endl;
            return false;
        }
    }

    std::cout << "- Step 4: Check InterLock Policy" << std::endl;
    for (const auto &precedingRecordName : dataRecord.getPointersFromHeader()) {
        std::cout << "-- Preceding record from " << RecordName(precedingRecordName).getProducerPrefix() << '\n';
        if (RecordName(precedingRecordName).getProducerPrefix() == producerID) {
            std::cout << "--- From itself" << '\n';
            return false;
        }
    }

    std::cout << "- Step 5: Check certificate/revocation record format" << std::endl;
    if (dataRecord.getType() == CERTIFICATE_RECORD || dataRecord.getType() == REVOCATION_RECORD) {
        if (!m_config.certificateManager->verifyRecordFormat(dataRecord)) {
            std::cout << "-- bad certificate/revocation record" << std::endl;
            return false;
        }
    } else {
        std::cout << "-- Not a certificate/revocation record" << std::endl;
    }

    std::cout << "- All Syntax check Steps finished. Good Record" << std::endl;
    return true;
}

bool LedgerImpl::checkEndorseValidityOfRecord(const Data& data) {
    std::cout << "[LedgerImpl::checkEndorseValidityOfRecord] Check the reference validity of the record" << std::endl;
    Record dataRecord;
    try {
        // format check
        dataRecord = Record(data);
    } catch (const std::exception& e) {
        std::cout << "-- The Data format is not proper for DLedger record because " << e.what() << std::endl;
        return false;
    }

    std::cout << "- Step 6: Check Revocation" << std::endl;
    if (!m_config.certificateManager->endorseSignature(data)) {
        std::cout << "-- certificate revoked" << std::endl;
        return false;
    }

    std::cout << "- Step 7: Check Contribution Policy" << std::endl;
    for (const auto& precedingRecordName : dataRecord.getPointersFromHeader()) {
        if (m_tailRecords.count(precedingRecordName) != 0) {
            std::cout << "-- Preceding record has weight " << m_tailRecords[precedingRecordName].refSet.size() << '\n';
            if (m_tailRecords[precedingRecordName].refSet.size() > m_config.contributionWeight) {
                std::cout << "--- Weight too high " << m_tailRecords[precedingRecordName].refSet.size() << '\n';
                return false;
            }
        } else {
            if (m_backend.getRecord(precedingRecordName) != nullptr) {
                std::cout << "-- Preceding record too deep" << '\n';
            } else {
                std::cout << "-- Preceding record Not found" << '\n';
            }
            return false;
        }
    }

    std::cout << "- Step 8: Check App Logic" << std::endl;
    if (m_onRecordAppCheck != nullptr && !m_onRecordAppCheck(data)) {
        std::cout << "-- App Logic check failed" << std::endl;
        return false;
    }

    std::cout << "- All Reference Check Steps finished. Good Record" << std::endl;
    return true;
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  std::cout << "[LedgerImpl::onLedgerSyncRequest] Receive SYNC Interest " << std::endl;
  /*// @TODO when new Interest signature format is supported by ndn-cxx, we need to change the way to obtain signature info.
  SignatureInfo info(interest.getName().get(-2).blockFromValue());
  if (m_config.peerPrefix.isPrefixOf(info.getKeyLocator().getName())) {
    std::cout << "- A SYNC Interest sent by myself. Ignore" << std::endl;
    return;
  }*/

  // verify the signature
  if (!m_config.certificateManager->verifySignature(interest)) {
      std::cout << "- Bad Signature. " << std::endl;
      return;
  }

  //cancel previous reply
  if (m_replySyncEventID) m_replySyncEventID.cancel();

  const auto& appParam = interest.getApplicationParameters();
  appParam.parse();
  std::cout << "- Received Tailing Record Names: \n";
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
            if (!m_backend.getRecord(certName)) {
                std::cout << "--- Fetch unseen certificate record "<< l.getName() << std::endl;
                fetchRecord(certName);
                isCertPending = true;
            }
        } catch (const std::exception& e) {
            std::cout << "--- Error on keyLocator \n";
        }
        continue;
    }
    if (isCertPending) continue;
    Name recordName(item);
    std::cout << "-- " << recordName.toUri() << "\n";
    if (m_tailRecords.count(recordName) != 0 && m_tailRecords[recordName].refSet.empty()) {
      std::cout << "--- This record is already in our tailing records \n";
    }
    else if (m_backend.getRecord(recordName)) {
      std::cout << "--- This record is already in our Ledger but not tailing any more \n";
      shouldSendSync = true;
    }
    else {
        std::cout << "--- Fetch unseen tailing record \n";
        //fetch record
        fetchRecord(recordName);
    }
  }
  if (shouldSendSync) {
      std::cout << "[LedgerImpl::onLedgerSyncRequest] send Sync interest so others can fetch new record\n";
      std::uniform_int_distribution<> dist{10, 200};
      m_replySyncEventID = m_scheduler.schedule(time::milliseconds(dist(m_randomEngine)), [&] {
          sendSyncInterest();
      });
  }
}

void
LedgerImpl::onRecordRequest(const Interest& interest)
{
  std::cout << "[LedgerImpl::onRecordRequest] Receive Interest to Fetch Record" << std::endl;
  auto desiredData = m_backend.getRecord(interest.getName().toUri());
  if (desiredData) {
    std::cout << "- Found desired Data, reply it." << std::endl;
    m_network.put(*desiredData);
  }
}

void
LedgerImpl::fetchRecord(const Name& recordName)
{
  std::cout << "[LedgerImpl::fetchRecord] Fetch the missing record" << std::endl;
  Interest interestForRecord(recordName);
  interestForRecord.setCanBePrefix(false);
  interestForRecord.setMustBeFresh(true);
  std::cout << "- Sending Record Fetching Interest " << interestForRecord.getName().toUri() << std::endl;
  m_network.expressInterest(interestForRecord,
                            bind(&LedgerImpl::onFetchedRecord, this, _1, _2),
                            bind(&LedgerImpl::onNack, this, _1, _2),
                            bind(&LedgerImpl::onTimeout, this, _1));
}

void
LedgerImpl::onFetchedRecord(const Interest& interest, const Data& data)
{
  std::cout << "[LedgerImpl::onFetchedRecordForSync] fetched record " << data.getFullName().toUri() << std::endl;
  if (hasRecord(data.getFullName().toUri())) {
    std::cout << "- Record already exists in the ledger. Ignore" << std::endl;
    return;
  }
  if (m_badRecords.count(data.getFullName()) != 0) {
      std::cout << "- Known bad record. Ignore" << std::endl;
      return;
  }
  for (const auto& stackRecord : m_syncStack) {
      if (stackRecord.first.getRecordName() == data.getFullName()) {
          std::cout << "- Record in sync stack already. Ignore" << std::endl;
          return;
      }
  }

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
          if (m_backend.getRecord(precedingRecordName)) {
              std::cout << "- Preceding Record " << precedingRecordName << " already in the ledger" << std::endl;
          } else {
              allPrecedingRecordsInLedger = false;
              fetchRecord(precedingRecordName);
          }
      }
      if (record.getType() == CERTIFICATE_RECORD) {
          std::cout << "- Checking previous cert record" << std::endl;
          CertificateRecord certRecord(record);
          for (const auto &prevCertName : certRecord.getPrevCertificates()) {
              if (prevCertName.empty()) continue;
              if (m_backend.getRecord(prevCertName)) {
                  std::cout << "- Preceding Cert Record " << prevCertName << " already in the ledger" << std::endl;
              } else {
                  std::cout << "- Preceding Cert Record " << prevCertName << " unseen" << std::endl;
                  allPrecedingRecordsInLedger = false;
                  fetchRecord(prevCertName);
              }
          }
      }

      if (!allPrecedingRecordsInLedger) {
          std::cout << "- Waiting for record to be added" << std::endl;
          return;
      }

  } catch (const std::exception& e) {
      std::cout << "- The Data format is not proper for DLedger record because " << e.what() << std::endl;
      std::cout << "--" << data.getFullName() << std::endl;
      m_badRecords.insert(data.getFullName());
      return;
  }

  int stackSize = INT_MAX;
  while (stackSize != m_syncStack.size()) {
      stackSize = m_syncStack.size();
      std::cout << "- SyncStack size " << m_syncStack.size() << std::endl;
      for (auto it = m_syncStack.begin(); it != m_syncStack.end();) {
          if (checkRecordAncestor(it->first)) {
              it = m_syncStack.erase(it);
          } else if(time::abs(time::system_clock::now() - it->second) > m_config.ancestorFetchTimeout){
              std::cout << "-- Timeout on fetching ancestor for " << it->first.getRecordName().toUri() << std::endl;
              it = m_syncStack.erase(it);
          } else {
              // else, some preceding records are not yet fetched
              it++;
          }
      }
  }
}

bool LedgerImpl::checkRecordAncestor(const Record &record) {
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
            if (!prevCertName.empty() && !m_backend.getRecord(prevCertName)) {
                readyToAdd = false;
            }
        }
    }
    if (!badRecord && readyToAdd) {
        std::cout << "- Good record. Will add record in to the ledger" << std::endl;
        addToTailingRecord(record, checkEndorseValidityOfRecord(*(record.m_data)));
        return true;
    }
    if (badRecord) {
        std::cout << "- Bad record. Will remove it and all its later records" << std::endl;
        m_badRecords.insert(record.getRecordName());
        return true;
    }
    return false;
}

void
LedgerImpl::addToTailingRecord(const Record& record, bool endorseVerified) {
    if (m_tailRecords.count(record.getRecordName()) != 0) {
        std::cout << "[LedgerImpl::addToTailingRecord] Repeated add record: " << record.getRecordName()
                  << std::endl;
        return;
    }

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
    m_tailRecords[record.getRecordName()] = TailingRecordState{refVerified, std::set<Name>(), endorseVerified};
    m_backend.putRecord(record.m_data);

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
        Record currentRecord(m_backend.getRecord(currentRecordName));
        auto precedingRecordList = currentRecord.getPointersFromHeader();
        for (const auto &precedingRecord : precedingRecordList) {
            if (RecordName(precedingRecord).getProducerPrefix() == record.getProducerPrefix()) continue;
            if (m_tailRecords.count(precedingRecord) != 0 &&
                m_tailRecords[precedingRecord].refSet.insert(record.getProducerPrefix()).second) {
                stack.push(precedingRecord);
                updatedRecords.insert(precedingRecord);
                std::cout << record.getProducerPrefix() << " confirms " << precedingRecord.toUri() << std::endl;
            }
        }
    }

    //remove deep records
    int removeWeight = max(m_config.contributionWeight + 1, m_config.confirmWeight);
    bool referenceNeedUpdate = false;
    for (const auto & updatedRecord : updatedRecords) {
        auto& tailingState = m_tailRecords[updatedRecord];
        if (tailingState.refSet.size() == m_config.confirmWeight) {
            std::cout << "confirmed " << updatedRecord.toUri() << std::endl;
            if (!tailingState.referenceVerified) {
                tailingState.referenceVerified = true;
                referenceNeedUpdate = true;
            }
            onRecordConfirmed(m_backend.getRecord(updatedRecord));
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
                Record currentRecord(m_backend.getRecord(r.first));
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

    dumpList(m_tailRecords);
}

void LedgerImpl::onRecordConfirmed(const Record &record){
    std::cout << "- [LedgerImpl::onRecordConfirmed] accept record" << std::endl;

    //register current time
    if (m_rateCheck[record.getProducerPrefix()] < record.getGenerationTimestamp())
            m_rateCheck[record.getProducerPrefix()] = record.getGenerationTimestamp();

    if (record.getType() == RecordType::CERTIFICATE_RECORD) {
        try {
            auto certRecord = CertificateRecord(record);
            m_lastCertRecords.push_back(certRecord.getRecordName());
            for (const auto &c : certRecord.getPrevCertificates()) {
                m_lastCertRecords.remove(c);
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad certificate record format. " << std::endl;
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

std::unique_ptr<Ledger>
Ledger::initLedger(const Config& config, security::KeyChain& keychain, Face& face)
{
  return std::make_unique<LedgerImpl>(config, keychain, face);
}

}  // namespace dledger