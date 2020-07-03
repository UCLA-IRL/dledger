#include "ledger-impl.hpp"
#include "record_name.hpp"

#include <algorithm>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/sha256.hpp>
#include <ndn-cxx/util/time.hpp>
#include <random>
#include <sstream>

using namespace ndn;
namespace dledger {

const static size_t DEFAULT_GENESIS_BLOCKS = 10;
const static time::seconds RECORD_PRODUCTION_INTERVAL_RATE_LIMIT = time::seconds(1);
const static time::seconds ANCESTOR_FETCH_TIMEOUT = time::seconds(10);
const static time::seconds CLOCK_SKEW_TOLERANCE = time::seconds(120);
const static time::seconds MAX_SYNC_RATE = time::seconds(1);

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
    , m_certList(config)
{
  std::cout << "\nDLedger Initialization Start" << std::endl;

  //****STEP 0****
  //check validity of config
  if (m_config.appendWeight > m_config.contributionWeight) {
    std::cerr << "invalid weight configuration" << std::endl;
    BOOST_THROW_EXCEPTION(std::runtime_error("invalid weight configuration"));
  }
  if (m_config.peerPrefix.size() != m_config.trustAnchorCert->getIdentity().size()) {
    std::cerr << "trust Anchor should follow the peer prefix format" << std::endl;
    BOOST_THROW_EXCEPTION(std::runtime_error("trust Anchor should follow the peer prefix format"));
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
  for (int i = 0; i < DEFAULT_GENESIS_BLOCKS; i++) {
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
            << "- " << DEFAULT_GENESIS_BLOCKS << " genesis records have been added to the DLedger" << std::endl
            << "DLedger Initialization Succeed\n\n";

  //****STEP 3****
  //append AnchorCert to certificate list
  m_certList.insert(*m_config.trustAnchorCert);

  this->sendPeriodicSyncInterest();
}

LedgerImpl::~LedgerImpl()
{
    m_network.shutdown();
}

ReturnCode
LedgerImpl::createRecord(Record& record)
{
  std::cout << "[LedgerImpl::addRecord] Add new record" << std::endl;
  if (m_tailRecords.empty()) {
    return ReturnCode::noTailingRecord();
  }
  if (!m_certList.authorizedToGenerate()) {
      return ReturnCode::signingError("No Valid Certificate");
  }

  std::vector<std::pair<Name, int>> recordList;
  for (const auto &item : m_tailRecords) {
      if (item.second.refSet.size() <= m_config.appendWeight &&
            !m_config.peerPrefix.isPrefixOf(item.first) &&
            item.second.referenceVerified) {
          recordList.emplace_back(item.first, item.second.refSet.size());
      }
  }

  // randomly shuffle the tailing record list
  std::mt19937_64 eng{std::random_device{}()};
  std::shuffle(std::begin(recordList), std::end(recordList), eng);

  // fulfill the record content with preceding record IDs
  // removal of preceding record is done by addToTailingRecord() at the end
  if (recordList.size() < m_config.precedingRecordNum) {
      return ReturnCode::notEnoughTailingRecord();
  }
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
    list.remove_if([&](const auto& name) {return !(m_tailRecords.count(name) && !m_tailRecords.find(name)->second.referenceVerified);});
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

void
LedgerImpl::sendPeriodicSyncInterest()
{

  if ((time::system_clock::now() - m_lastSyncTime) >= MAX_SYNC_RATE) {
      std::cout << "[LedgerImpl::sendPeriodicSyncInterest] Send periodic SYNC Interest.\n";
      sendSyncInterest();
  } else {
      std::cout << "[LedgerImpl::sendPeriodicSyncInterest] SYNC Interest sending too fast.\n";
  }

  // schedule for the next SyncInterest Sending
  m_scheduler.schedule(time::seconds(5), [this] { sendPeriodicSyncInterest(); });
}

ReturnCode LedgerImpl::sendSyncInterest() {
    std::cout << "[LedgerImpl::sendSyncInterest] Send SYNC Interest.\n";
    // SYNC Interest Name: /<multicastPrefix>/SYNC/digest
    // construct SYNC Interest
    Name syncInterestName = m_config.multicastPrefix;
    syncInterestName.append("SYNC");
    Interest syncInterest(syncInterestName);
    Block appParam = makeEmptyBlock(tlv::ApplicationParameters);
    for (const auto &item : m_tailRecords) {
        if (item.second.refSet.empty())
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

    m_lastSyncTime = time::system_clock::now();
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
        dataRecord.checkPointerValidity(
                m_config.peerPrefix.getPrefix( -1), m_config.precedingRecordNum);
    } catch (const std::exception &e) {
        std::cout << "-- The Data format is not proper for DLedger record because " << e.what() << std::endl;
        return false;
    }

    std::cout << "- Step 2: Check signature" << std::endl;
    std::string producerID = dataRecord.getProducerID();
    if (!m_certList.verifySignature(data)){
        std::cout << "-- Bad Signature." << std::endl;
        return false;
    }
    if (dataRecord.getType() == RecordType::CERTIFICATE_RECORD) {
        if (!security::verifySignature(data, *m_config.trustAnchorCert)) {
            std::cout << "-- Certificate Record has bad Signature." << std::endl;
            return false;
        }
    }

    std::cout << "- Step 3: Check rating limit" << std::endl;
    auto tp = dataRecord.getGenerationTimestamp();
    if (tp > time::system_clock::now() + CLOCK_SKEW_TOLERANCE) {
        std::cout << "-- record from too far in the future" << std::endl;
        return false;
    }
    if (m_rateCheck.find(producerID) == m_rateCheck.end()) {
        m_rateCheck[producerID] = tp;
    } else {
        if ((time::abs(tp - m_rateCheck[producerID]) < RECORD_PRODUCTION_INTERVAL_RATE_LIMIT)) {
            std::cout << "-- record generation too fast from the peer" << std::endl;
            return false;
        } else {
            m_rateCheck[producerID] = tp;
        }
    }

    std::cout << "- Step 4: Check InterLock Policy" << std::endl;
    for (const auto &precedingRecordName : dataRecord.getPointersFromHeader()) {
        std::cout << "-- Preceding record from " << RecordName(precedingRecordName).getProducerID() << '\n';
        if (RecordName(precedingRecordName).getProducerID() == producerID) {
            std::cout << "--- From itself" << '\n';
            return false;
        }
    }

    std::cout << "- Step 5: Check certificate/revocation record format" << std::endl;
    if (dataRecord.getType() == RecordType::CERTIFICATE_RECORD) {
        try {
            auto certRecord = CertificateRecord(dataRecord);
            for (const auto& cert: certRecord.getCertificates()) {
                if (!security::verifySignature(cert, *m_config.trustAnchorCert)){
                    std::cout << "-- invalid certificate: "<< cert.getName() << std::endl;
                    return false;
                }
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad certificate record format. " << std::endl;
            return false;
        }
    } else if (dataRecord.getType() == RecordType::REVOCATION_RECORD) {
        try {
            auto revokeRecord = RevocationRecord(dataRecord);
            bool isAnchor = readString(m_config.trustAnchorCert->getIdentity().get(-1)) == revokeRecord.getProducerID();
            for (const auto& certName: revokeRecord.getRevokedCertificates()) {
                if (!certName.get(-1).isImplicitSha256Digest() || !security::v2::Certificate::isValidName(certName.getPrefix(-1))) {
                    std::cout << "-- invalid revoked certificate: "<< certName << std::endl;
                    return false;
                }
                if (!isAnchor &&
                    readString(m_certList.getCertificateNameIdentity(certName).get(-1)) != revokeRecord.getProducerID()) {
                    std::cout << "-- invalid revoked of other's certificate: "<< certName << std::endl;
                    return false;
                }
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad revocation record format. " << std::endl;
            return false;
        }
    } else {
        std::cout << "-- Not a certificate/revocation record" << std::endl;
    }

    std::cout << "- Step 6: Check App Logic" << std::endl;
    if (m_onRecordAppCheck != nullptr && !m_onRecordAppCheck(data)) {
        std::cout << "-- App Logic check failed" << std::endl;
        return false;
    }

    std::cout << "- All Syntax check Steps finished. Good Record" << std::endl;
    return true;
}

bool LedgerImpl::checkReferenceValidityOfRecord(const Data& data) {
    std::cout << "[LedgerImpl::checkReferenceValidityOfRecord] Check the reference validity of the record" << std::endl;
    Record dataRecord;
    try {
        // format check
        dataRecord = Record(data);
    } catch (const std::exception& e) {
        std::cout << "-- The Data format is not proper for DLedger record because " << e.what() << std::endl;
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

    std::cout << "- All Reference Check Steps finished. Good Record" << std::endl;
    return true;
}

void
LedgerImpl::onLedgerSyncRequest(const Interest& interest)
{
  std::cout << "[LedgerImpl::onLedgerSyncRequest] Receive SYNC Interest " << std::endl;
  // @TODO when new Interest signature format is supported by ndn-cxx, we need to change the way to obtain signature info.
  SignatureInfo info(interest.getName().get(-2).blockFromValue());
  if (m_config.peerPrefix.isPrefixOf(info.getKeyLocator().getName())) {
    std::cout << "- A SYNC Interest sent by myself. Ignore" << std::endl;
    return;
  }

  // verify the signature
  if (!m_certList.verifySignature(interest)) {
      std::cout << "- Bad Signature. " << std::endl;
      return;
  }

  const auto& appParam = interest.getApplicationParameters();
  appParam.parse();
  std::cout << "- Received Tailing Record Names: \n";
  bool shouldSendSync = false;
  for (const auto& item : appParam.elements()) {
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

        //fetch record
        std::mt19937_64 eng{std::random_device{}()};
        std::uniform_int_distribution<> dist{10, 100};
        m_scheduler.schedule(time::milliseconds(dist(eng)), [&, recordName] {
            fetchRecord(recordName);
        });
    }
  }
  if (shouldSendSync) {
      std::cout << "[LedgerImpl::onLedgerSyncRequest] send Sync interest so others can fetch new record\n";
      sendSyncInterest();
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

      if (!allPrecedingRecordsInLedger) {
          std::cout << "- Waiting for record to be added" << std::endl;
          return;
      }

  } catch (const std::exception& e) {
      std::cout << "- The Data format is not proper for DLedger record because " << e.what() << std::endl;
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
          } else if(time::abs(time::system_clock::now() - it->second) > ANCESTOR_FETCH_TIMEOUT){
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
    if (!badRecord && readyToAdd) {
        std::cout << "- Good record. Will add record in to the ledger" << std::endl;
        addToTailingRecord(record, checkReferenceValidityOfRecord(*(record.m_data)));
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
LedgerImpl::addToTailingRecord(const Record& record, bool verified) {
    if (m_tailRecords.count(record.getRecordName()) != 0) {
        std::cout << "[LedgerImpl::addToTailingRecord] Repeated add record: " << record.getRecordName()
                  << std::endl;
        return;
    }

    //verify if ancestor has correct reference policy
    bool refVerified = verified;
    if (verified) {
        for (const auto &precedingRecord : record.getPointersFromHeader()) {
            if (m_tailRecords.count(precedingRecord) != 0 &&
                !m_tailRecords[precedingRecord].referenceVerified) {
                refVerified = false;
                break;
            }
        }
    }

    //add record to tailing record
    m_tailRecords[record.getRecordName()] = TailingRecordState{refVerified, std::set<std::string>(), verified};
    m_backend.putRecord(record.m_data);
    if (refVerified) onRecordAccepted(record);

    //update weight of the system
    std::stack<Name> stack;
    stack.push(record.getRecordName());
    while (!stack.empty()) {
        RecordName currentRecordName(stack.top());
        stack.pop();
        if (currentRecordName.getRecordType() == GENESIS_RECORD) continue;
        Record currentRecord(m_backend.getRecord(currentRecordName));
        auto precedingRecordList = currentRecord.getPointersFromHeader();
        for (const auto &precedingRecord : precedingRecordList) {
            if (m_tailRecords.count(precedingRecord) != 0 &&
                m_tailRecords[precedingRecord].refSet.insert(record.getProducerID()).second) {
                stack.push(precedingRecord);
                std::cout << record.getProducerID() << " confirms " << precedingRecord.toUri() << std::endl;
            }
        }
    }

    //remove deep records
    int removeWeight = max(m_config.contributionWeight + 1, m_config.confirmWeight);
    bool referenceNeedUpdate = false;
    for (auto it = m_tailRecords.begin(); it != m_tailRecords.end();) {
        if (it->second.refSet.size() >= m_config.confirmWeight) {
            std::cout << "confirmed " << it->first.toUri() << std::endl;
            if (!it->second.referenceVerified) {
                it->second.referenceVerified = true;
                referenceNeedUpdate = true;
                onRecordAccepted(m_backend.getRecord(it->first));
            }
        }
        if (it->second.refSet.size() >= removeWeight) {
            it = m_tailRecords.erase(it);
        } else {
            it++;
        }
    }

    //update reference policy
    while (referenceNeedUpdate) {
        referenceNeedUpdate = false;
        for (auto &r : m_tailRecords) {
            if (!r.second.referenceVerified && r.second.recordPolicyVerified) {
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
                    onRecordAccepted(currentRecord);
                }
            }
        }
    }

    dumpList(m_tailRecords);
}

void LedgerImpl::onRecordAccepted(const Record &record){
    std::cout << "- [LedgerImpl::onRecordAccepted] accept record" << std::endl;
    if (record.getType() == RecordType::CERTIFICATE_RECORD) {
        try {
            auto certRecord = CertificateRecord(record);
            for (const auto& cert: certRecord.getCertificates()) {
                m_certList.insert(cert);
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad certificate record format. " << std::endl;
            return;
        }
    } else if (record.getType() == RecordType::REVOCATION_RECORD) {
        try {
            auto revokeRecord = RevocationRecord(record);
            bool isAnchor = readString(m_config.trustAnchorCert->getIdentity().get(-1)) == revokeRecord.getProducerID();
            for (const auto& certName: revokeRecord.getRevokedCertificates()) {
                m_certList.revoke(certName);
            }
        } catch (const std::exception &e) {
            std::cout << "-- Bad revocation record format. " << std::endl;
            return;
        }
    }

    if (m_onRecordAppAccepted != nullptr) {
        m_onRecordAppAccepted(record);
    }
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

}  // namespace dledger