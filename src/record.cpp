#include "dledger/record.hpp"
#include <sstream>

namespace dledger {

RecordHeader::RecordHeader(std::list<Name> recordPointers)
  : m_recordPointers(recordPointers)
{}

void
RecordHeader::wireEncode(Block& block) const
{
  auto header = makeEmptyBlock(T_RecordHeader);
  for (const auto& pointer : m_recordPointers) {
    header.push_back(pointer.wireEncode());
  }
  header.parse();
  block.push_back(header);
  block.parse();
};

const std::list<Name>&
RecordHeader::wireDecode(const Block& dataContent)
{
  if (m_recordPointers.size() > 0) {
    return m_recordPointers;
  }
  dataContent.parse();
  try {
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
  catch (const std::exception& e) {
    return m_recordPointers;
  }
  return m_recordPointers;
}

RecordContent::RecordContent(std::list<std::string> contentItems)
  : m_contentItems(contentItems)
{}

void
RecordContent::wireEncode(Block& block) const
{
  auto body = makeEmptyBlock(T_RecordContent);
  for (const auto& item : m_contentItems) {
    body.push_back(makeStringBlock(T_ContentItem, item));
  }
  body.parse();
  block.push_back(body);
  block.parse();
};

const std::list<std::string>&
RecordContent::wireDecode(const Block& dataContent)
{
  if (m_contentItems.size() > 0) {
    return m_contentItems;
  }
  dataContent.parse();
  try {
    const auto& contentBlock = dataContent.get(T_RecordContent);
    contentBlock.parse();
    for (const auto& item : contentBlock.elements()) {
      if (item.type() == T_ContentItem) {
        m_contentItems.push_back(readString(item));
      }
    }
  }
  catch (const std::exception& e) {
    return m_contentItems;
  }
  return m_contentItems;
}

Record::Record()
  : m_data(nullptr)
{}

Record::Record(const std::shared_ptr<Data>& data)
  : m_data(data)
{
  m_header.wireDecode(m_data->getContent());
  m_content.wireDecode(m_data->getContent());
}

Record::Record(ndn::Data data)
  : m_data(std::make_shared<ndn::Data>(std::move(data)))
{
  m_header.wireDecode(m_data->getContent());
  m_content.wireDecode(m_data->getContent());
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
  RecordHeader header;
  return header.wireDecode(m_data->getContent());
}

const std::list<std::string>&
Record::getRecordItems() const
{
  RecordContent content;
  return content.wireDecode(m_data->getContent());
}

} // namespace DLedger