#include "dledger/record.hpp"

#include <sstream>

namespace dledger {

Record::Record(RecordType type, const std::string& identifer)
    : m_data(nullptr),
      m_type(type),
      m_uniqueIdentifier(identifer)
{
}

Record::Record(const std::shared_ptr<Data>& data)
    : m_data(data),
      m_type(GenericRecord)
{
  // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>
  m_type = stringToRecordType(readString(m_data->getName().get(2)));
  m_uniqueIdentifier = readString(m_data->getName().get(3));
  headerWireDecode(m_data->getContent());
  bodyWireDecode(m_data->getContent());
}

Record::Record(ndn::Data data)
    : m_data(std::make_shared<ndn::Data>(std::move(data))),
      m_type(GenericRecord)
{
  // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>
  m_type = stringToRecordType(readString(m_data->getName().get(2)));
  m_uniqueIdentifier = readString(m_data->getName().get(3));
  headerWireDecode(m_data->getContent());
  bodyWireDecode(m_data->getContent());
}

std::string
Record::getRecordName() const
{
  if (m_data != nullptr)
    return m_data->getFullName().toUri();
  return "";
}

const std::list<Name>&
Record::getPointersFromHeader() const
{
  return m_recordPointers;
}

void
Record::addRecordItem(const std::string& recordItem)
{
  m_contentItems.push_back(recordItem);
}

const std::list<std::string>&
Record::getRecordItems() const
{
  return m_contentItems;
}

bool
Record::isEmpty() const
{
  return m_data == nullptr && m_recordPointers.empty() && m_contentItems.empty();
}

void
Record::addPointer(const Name& pointer)
{
  m_recordPointers.push_back(pointer);
}

void
Record::wireEncode(Block& block) const
{
  headerWireEncode(block);
  bodyWireEncode(block);
}

std::string
Record::getProducerID() const
{
  return readString(m_data->getName().get(-4));
}

time::system_clock::TimePoint
Record::getGenerationTimestamp() const
{
  return m_data->getName().get(-1).toTimestamp();
}

void
Record::headerWireEncode(Block& block) const
{
  auto header = makeEmptyBlock(T_RecordHeader);
  for (const auto& pointer : m_recordPointers) {
    header.push_back(pointer.wireEncode());
  }
  header.parse();
  block.push_back(header);
  block.parse();
};

void
Record::headerWireDecode(const Block& dataContent)
{
  m_recordPointers.clear();
  dataContent.parse();
    const auto& headerBlock = dataContent.get(T_RecordHeader);
    headerBlock.parse();
    Name pointer;
    for (const auto& item : headerBlock.elements()) {
      if (item.type() == tlv::Name) {
        pointer.wireDecode(item);
        m_recordPointers.push_back(pointer);
      }
    }
}

void
Record::bodyWireEncode(Block& block) const
{
  auto body = makeEmptyBlock(T_RecordContent);
  for (const auto& item : m_contentItems) {
    body.push_back(makeStringBlock(T_ContentItem, item));
  }
  body.parse();
  block.push_back(body);
  block.parse();
};

void
Record::bodyWireDecode(const Block& dataContent)
{
  m_contentItems.clear();
  dataContent.parse();
    const auto& contentBlock = dataContent.get(T_RecordContent);
    contentBlock.parse();
    for (const auto& item : contentBlock.elements()) {
      if (item.type() == T_ContentItem) {
        m_contentItems.push_back(readString(item));
      }
    }
}

GenericRecord::GenericRecord(const std::string& identifer)
    : Record(RecordType::GenericRecord, identifer)
{
}

CertificateRecord::CertificateRecord(const std::string& identifer)
    : Record(RecordType::CertificateRecord, identifer)
{
}

void
CertificateRecord::addCertificateItem(const security::v2::Certificate& certificate)
{
}

const std::list<security::v2::Certificate>&
CertificateRecord::getCertificates() const
{
}

}  // namespace dledger