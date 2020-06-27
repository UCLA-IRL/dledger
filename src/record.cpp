#include "dledger/record.hpp"
#include "record_name.hpp"

#include <sstream>
#include <utility>

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

Name
Record::getRecordName() const
{
  if (m_data != nullptr)
    return m_data->getFullName();
  return Name();
}

const std::list<Name>&
Record::getPointersFromHeader() const
{
  return m_recordPointers;
}

void
Record::addRecordItem(const Block& recordItem)
{
  m_contentItems.push_back(recordItem);
}

const std::list<Block>&
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
Record::headerWireDecode(const Block& dataContent) {
    m_recordPointers.clear();
    dataContent.parse();
    const auto &headerBlock = dataContent.get(T_RecordHeader);
    headerBlock.parse();
    Name pointer;
    for (const auto &item : headerBlock.elements()) {
        if (item.type() == tlv::Name) {
            try {
                pointer.wireDecode(item);
            } catch (const tlv::Error &e) {
                std::cout << (e.what());
            }

            m_recordPointers.push_back(pointer);
        } else {
            BOOST_THROW_EXCEPTION(std::runtime_error("Bad header item type"));
        }
    }
}

void
Record::bodyWireEncode(Block& block) const
{
  auto body = makeEmptyBlock(T_RecordContent);
  for (const auto& item : m_contentItems) {
    body.push_back(item);
  }
  body.parse();
  block.push_back(body);
  block.parse();
};

void
Record::bodyWireDecode(const Block& dataContent) {
    m_contentItems.clear();
    dataContent.parse();
    const auto &contentBlock = dataContent.get(T_RecordContent);
    contentBlock.parse();
    for (const auto &item : contentBlock.elements()) {
        m_contentItems.push_back(item);
    }
}

void
Record::checkPointerValidity(const Name& prefix, int numPointers) const{
    if (getPointersFromHeader().size() != numPointers) {
        throw std::runtime_error("Less preceding record than expected");
    }
    if (RecordName(m_data->getFullName()).getApplicationCommonPrefix() !=
        prefix.toUri()){
        throw std::runtime_error("Wrong App common prefix");
    }

    std::set<Name> nameSet;
    for (const auto& pointer: getPointersFromHeader()) {
        nameSet.insert(pointer);
    }
    if (nameSet.size() != numPointers) {
        throw std::runtime_error("Repeated preceding Records");
    }
}

GenericRecord::GenericRecord(const std::string& identifer)
    : Record(RecordType::GENERIC_RECORD, identifer)
{
}

CertificateRecord::CertificateRecord(const std::string& identifer)
    : Record(RecordType::CERTIFICATE_RECORD, identifer)
{
}

CertificateRecord::CertificateRecord(Record record)
    : Record(std::move(record))
{
    if (this->getType() != RecordType::CERTIFICATE_RECORD) {
        BOOST_THROW_EXCEPTION(std::runtime_error("incorrect record type"));
    }
    for (const Block& block : this->getRecordItems()) {
        m_cert_list.emplace_back(block);
    }
}

void
CertificateRecord::addCertificateItem(const security::v2::Certificate& certificate)
{
    m_cert_list.emplace_back(certificate);
    addRecordItem(certificate.wireEncode());
}

const std::list<security::v2::Certificate> &
CertificateRecord::getCertificates() const
{
    return m_cert_list;
}

RevocationRecord::RevocationRecord(const std::string &identifer):
    Record(RecordType::REVOCATION_RECORD, identifer) {
}

RevocationRecord::RevocationRecord(Record record):
    Record(std::move(record)){
    if (this->getType() != RecordType::REVOCATION_RECORD) {
        BOOST_THROW_EXCEPTION(std::runtime_error("incorrect record type"));
    }
    for (const Block& block : this->getRecordItems()) {
        m_revoked_cert_list.emplace_back(block);
    }
}

void
RevocationRecord::addCertificateNameItem(const Name &certificateName){
    m_revoked_cert_list.emplace_back(certificateName);
    addRecordItem(certificateName.wireEncode());
}

const std::list<Name> &
RevocationRecord::getRevokedCertificates() const{
    return m_revoked_cert_list;
}

GenesisRecord::GenesisRecord(const std::string &identifier) :
    Record(RecordType::GENESIS_RECORD, identifier) {}

}  // namespace dledger