#include "backend.hpp"
#include <iostream>
#include <cassert>

namespace dledger {

Backend::Backend()
{
  this->initDatabase("/tmp/dledger-db");
}

Backend::~Backend()
{
  delete m_db;
}

shared_ptr<Data>
Backend::getRecord(const Name& recordName)
{
  const auto& nameStr = recordName.toUri();
  leveldb::Slice key = nameStr;
  std::string value;
  leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
  if (false == s.ok()) {
    std::cerr << "Unable to get value from database, key: " << nameStr << std::endl;
    std::cerr << s.ToString() << std::endl;
    return nullptr;
  }
  else {
    ndn::Block block((const uint8_t*)value.c_str(), value.size());
    return make_shared<Data>(block);
  }
}

bool
Backend::putRecord(const shared_ptr<Data>& recordData)
{
  const auto& nameStr = recordData->getName().toUri();
  leveldb::Slice key = nameStr;
  auto recordBytes = recordData->wireEncode();
  leveldb::Slice value((const char*)recordBytes.wire(), recordBytes.size());
  leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
  if (false == s.ok()) {
    std::cerr << "Unable to get value from database, key: " << nameStr << std::endl;
    std::cerr << s.ToString() << std::endl;
    return false;
  }
  return true;
}

void
Backend::deleteRecord(const Name& recordName)
{
  const auto& nameStr = recordName.toUri();
  leveldb::Slice key = nameStr;
  leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), key);
  if (false == s.ok()) {
    std::cerr << "Unable to delete value from database, key: " << nameStr << std::endl;
    std::cerr << s.ToString() << std::endl;
  }
}

void
Backend::initDatabase(const std::string& dbDir)
{
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, dbDir, &m_db);
  if (false == status.ok()) {
    std::cerr << "Unable to open/create test database " << dbDir << std::endl;
    std::cerr << status.ToString() << std::endl;
  }
}

} // namespace dledger