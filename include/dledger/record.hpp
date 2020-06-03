#ifndef DLEDGER_INCLUDE_RECORD_H_
#define DLEDGER_INCLUDE_RECORD_H_

#include <set>
#include <vector>
#include <list>
#include <ndn-cxx/data.hpp>
#include <ndn-cxx/security/v2/certificate.hpp>

using namespace ndn;
namespace dledger {

enum RecordType {
  BaseRecord = 0,
  GenericRecord = 1,
  CertificateRecord = 2,
  RevocationRecord = 3,
  GenesisRecord = 4,
};

/**
 * The record.
 * Record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-identifier>/<timestamp>
 */
class Record
{
public: // used for preparing a new record before appending it into the DLedger
  Record() = default;

  /**
   * Construct a new record.
   * @p type, input, the type of the record.
   * @p identifier, input, the unique identifer of the record.
   */
  Record(RecordType type, const std::string& identifer);

  /**
   * Add a new record payload item into the record.
   * @note This function should only be used to generate a record before adding it to the ledger.
   * @p recordItem, input, the record payload to add.
   */
  void
  addRecordItem(const std::string& recordItem);

  /**
   * Get the NDN Data full name of the record.
   * This name is not the identifier used in the constructor of the record.
   * The name is only generated when adding the record into the DLedger.
   * @note This function should only be used to parse a record returned from the ledger.
   *       This cannot be used when a record has not been appended into the ledger
   * @p recordItem, input, the record payload to add.
   */
  std::string
  getRecordName() const;

  /**
   * Get the record type of the record.
   */
  RecordType
  getType() const {
    return m_type;
  }

  /**
   * Get record payload items.
   */
  const std::list<std::string>&
  getRecordItems() const;

  /**
   * Check whether the record body is empty or not.
   */
  bool
  isEmpty() const;

public: // used for generating a new record before appending it into the DLedger
  /**
   * @note This constructor is supposed to be used by the LedgerImpl class only
   */
  Record(const std::shared_ptr<Data>& data);

  /**
   * @note This constructor is supposed to be used by the LedgerImpl class only
   */
  Record(Data data);

  /**
   * Get the pointers from the header.
   * @note This function is supposed to be used by the DLedger class only
   */
  const std::list<Name>&
  getPointersFromHeader() const;

  /**
   * Add new pointers to the header.
   * @note This function is supposed to be used by the DLedger class only
   */
  void
  addPointer(const Name& pointer);

  /**
   * Encode the record header and body into the block.
   * @p block, output, the Data Content block to carry the encoded record.
   */
  void
  wireEncode(Block& block) const;

  std::string
  getProducerID() const;

  time::system_clock::TimePoint
  getGenerationTimestamp() const;

  /**
   * Data packet with name
   * /<application-common-prefix>/<producer-name>/<record-type>/<record-name>/<timestamp>
   */
  std::shared_ptr<const Data> m_data;

private:
  void
  headerWireEncode(Block& block) const;

  void
  bodyWireEncode(Block& block) const;

  void
  headerWireDecode(const Block& dataContent);

  void
  bodyWireDecode(const Block& dataContent);

private:
  /**
   * The TLV type of the record header in the NDN Data Content.
   */
  const static uint8_t T_RecordHeader = 129;
  /**
   * The TLV type of the record body in the NDN Data Content.
   */
  const static uint8_t T_RecordContent = 130;
  /**
   * The TLV type of each item in the record body in the NDN Data Content.
   */
  const static uint8_t T_ContentItem = 131;

private:
  /**
   * The record-type in
   * /<application-common-prefix>/<producer-name>/<record-type>/<record-name>/<timestamp>
   */
  RecordType m_type;
  /**
   * The record-name in
   * /<application-common-prefix>/<producer-name>/<record-type>/<record-name>/<timestamp>
   */
  std::string m_uniqueIdentifier;
  /**
   * The list of pointers to preceding records.
   */
  std::list<Name> m_recordPointers;
  /**
   * The data structure to carry the record body payload.
   */
  std::list<std::string> m_contentItems;


  friend class LedgerImpl;
};

class GenericRecord : public Record
{
public:
  GenericRecord(const std::string& identifer);
};

class CertificateRecord : public Record
{
public:
  CertificateRecord(const std::string& identifer);

  void
  addCertificateItem(const security::v2::Certificate& certificate);

  const std::list<security::v2::Certificate>&
  getCertificates() const;
};

inline std::string
recordTypeToString(const RecordType& type)
{
  switch (type)
  {
  case RecordType::GenericRecord:
    return "Generic";

  case RecordType::CertificateRecord:
    return "Cert";

  case RecordType::RevocationRecord:
    return "Revocation";

  case RecordType::GenesisRecord:
    return "Genesis";

  default:
    return "Undefined";
  }
}

inline RecordType
stringToRecordType(const std::string& type)
{
  if (type == "Generic") return RecordType::GenericRecord;
  else if (type == "Cert") return RecordType::CertificateRecord;
  else if (type == "Revocation") return RecordType::RevocationRecord;
  else if (type == "Genesis") return RecordType::GenesisRecord;
  else return RecordType::BaseRecord;
}

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RECORD_H_

