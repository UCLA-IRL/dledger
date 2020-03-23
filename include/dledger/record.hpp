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
  GenericRecord = 0,
  CertificateRecord = 1,
  RevocationRecord = 2,
};

class RecordHeader
{
public:
  RecordHeader() = default;
  RecordHeader(std::list<Name> recordPointers);

  void
  wireEncode(Block& block) const;

  const std::list<Name>&
  wireDecode(const Block& dataContent);

  void
  addPointer(const Name& pointer) {
    m_recordPointers.push_back(pointer);
  };

  bool
  isEmpty() {
    return m_recordPointers.size() == 0;
  }

private:
  const static uint8_t T_RecordHeader = 129;
  std::list<Name> m_recordPointers;
};

class RecordContent
{
public:
  RecordContent() = default;
  RecordContent(std::list<std::string> contentItems);

  void
  wireEncode(Block& block) const;

  const std::list<std::string>&
  wireDecode(const Block& dataContent);

  void
  addContentItem(const std::string& contentItem) {
    m_contentItems.push_back(contentItem);
  };

  bool
  isEmpty() {
    return m_contentItems.size() == 0;
  }

private:
  const static uint8_t T_RecordContent = 130;
  const static uint8_t T_ContentItem = 131;
  std::list<std::string> m_contentItems;
};

// Record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-identifier>/<timestamp>
class Record
{
public:
  Record();

  // only used to generate a record before adding to the ledger
  void
  addRecordItem(const std::string& recordItem) {
    m_content.addContentItem(recordItem);
  };

  // only used to parse a record returned from the ledger
  std::string
  getRecordName() const;

  // only used to parse a record returned from the ledger
  const std::list<std::string>&
  getRecordItems() const;

  bool
  isEmpty() {
    return m_data == nullptr && m_header.isEmpty() && m_content.isEmpty();
  }

  void
  setRecordIdentifier(const std::string& recordIdentifier) {
    m_uniqueIdentifier = recordIdentifier;
  }

  RecordType m_type;
  std::string m_uniqueIdentifier;

private:
  // supposed to be used by the Ledger class only
  Record(const std::shared_ptr<Data>& data);
  Record(Data data);

  // supposed to be used by the Ledger class only
  const std::list<Name>&
  getPointersFromHeader() const;

  // supposed to be used by the Ledger class only
  void
  addPointer(const Name& pointer) {
    m_header.addPointer(pointer);
  };

  void
  wireEncode(Block& block) const {
    m_header.wireEncode(block);
    m_content.wireEncode(block);
  }

private:
  std::shared_ptr<const Data> m_data;
  RecordHeader m_header;
  RecordContent m_content;
  // std::vector<std::string> m_precedingRecords;
  // std::set<std::string> m_approvers;
  friend class LedgerImpl;
};

class CertificateRecord : public Record
{
public:
  CertificateRecord();

  void
  addCertificateItem(const security::v2::Certificate& certificate);

  const std::list<security::v2::Certificate>&
  getCertificates() const;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RECORD_H_

