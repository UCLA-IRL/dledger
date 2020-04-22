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
};

/**
 * The header of a record.
 * The header is composed of a list of names, called pointers, each of which references to a preceding record.
 * The record header is carried in NDN Data Content.
 */
class RecordHeader
{
public:
  RecordHeader() = default;
  RecordHeader(std::list<Name> recordPointers);

  /**
   * Encode the record header into the block.
   * @note this function should be called before the wireEncode of the record body.
   * @p block, output, the Data Content block to carry the encoded header.
   */
  void
  wireEncode(Block& block) const;

  /**
   * Decode the record header from the Data Content.
   * @p dataContent, intput, the Data Content block carrying the encoded header.
   * @return a list of NDN full names, each of which is represents a preceding record that referenced by the current record.
   */
  const std::list<Name>&
  wireDecode(const Block& dataContent);

  /**
   * Add a new pointer to the record header.
   * @p pointer, intput, the full name of a preceding record.
   */
  void
  addPointer(const Name& pointer) {
    m_recordPointers.push_back(pointer);
  };

  /**
   * Check whether the record header is empty or not.
   */
  bool
  isEmpty() const {
    return m_recordPointers.size() == 0;
  }

private:
  /**
   * The TLV type of the record header in the NDN Data Content.
   */
  const static uint8_t T_RecordHeader = 129;
  /**
   * The list of pointers to preceding records.
   */
  std::list<Name> m_recordPointers;
};


/**
 * The body of a record.
 * The body is composed of a list of strings, each of which carries a piece of payload of the record.
 * The record body is carried in NDN Data Content.
 */
class RecordBody
{
public:
  RecordBody() = default;
  RecordBody(std::list<std::string> contentItems);

  /**
   * Encode the record body into the block.
   * @note this function should be called after the wireEncode of the record header.
   * @p block, output, the Data Content block to carry the encoded body.
   */
  void
  wireEncode(Block& block) const;

  /**
   * Decode the record body from the Data Content.
   * @p dataContent, intput, the Data Content block carrying the encoded body.
   * @return a list of record body items, each of which carries a piece of record payload.
   */
  const std::list<std::string>&
  wireDecode(const Block& dataContent);

  /**
   * Add a new record body item to the record header.
   * @p contentItem, intput, an item of record payload.
   */
  void
  addContentItem(const std::string& contentItem) {
    m_contentItems.push_back(contentItem);
  };

  /**
   * Check whether the record body is empty or not.
   */
  bool
  isEmpty() const {
    return m_contentItems.size() == 0;
  }

private:
  /**
   * The TLV type of the record body in the NDN Data Content.
   */
  const static uint8_t T_RecordContent = 130;
  /**
   * The TLV type of each item in the record body in the NDN Data Content.
   */
  const static uint8_t T_ContentItem = 131;
  /**
   * The data structure to carry the record body payload.
   */
  std::list<std::string> m_contentItems;
};

/**
 * The record.
 * Record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-identifier>/<timestamp>
 */
class Record
{
public:
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
  addRecordItem(const std::string& recordItem) {
    m_content.addContentItem(recordItem);
  };

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
  isEmpty() const {
    return m_data == nullptr && m_header.isEmpty() && m_content.isEmpty();
  };

private:
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
  addPointer(const Name& pointer) {
    m_header.addPointer(pointer);
  };

  /**
   * Encode the record header and body into the block.
   * @p block, output, the Data Content block to carry the encoded record.
   */
  void
  wireEncode(Block& block) const {
    m_header.wireEncode(block);
    m_content.wireEncode(block);
  };

private:
  RecordType m_type;
  std::string m_uniqueIdentifier;
  std::shared_ptr<const Data> m_data;
  RecordHeader m_header;
  RecordBody m_content;
  // std::vector<std::string> m_precedingRecords;
  // std::set<std::string> m_approvers;
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

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RECORD_H_

