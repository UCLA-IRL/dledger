#include "dledger/record.hpp"
#include "record_name.hpp"

#include <sstream>

namespace dledger {

Record::Record(RecordType type, const std::string& identifer)
    : m_data(nullptr),
      m_type(type),
      m_uniqueIdentifier(identifer)
{
}

Record::Record(const std::shared_ptr<Data>& data)
    : m_data(data)
{
  RecordName name(m_data->getName());
  m_type = name.getRecordType();
  m_uniqueIdentifier = name.getRecordUniqueIdentifier();
  headerWireDecode(m_data->getContent());
  bodyWireDecode(m_data->getContent());
}

Record::Record(ndn::Data data)
    : Record(std::make_shared<ndn::Data>(std::move(data)))
{
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
  return RecordName(m_data->getName()).getProducerID();
}

time::system_clock::TimePoint
Record::getGenerationTimestamp() const
{
  return RecordName(m_data->getName()).getGenerationTimestamp();
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
       try{
         pointer.wireDecode(item);
       } catch (const tlv::Error& e){
         NFD_LOG_DEBUG("Malformed data to header record: " << e.what() );
       }
        
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