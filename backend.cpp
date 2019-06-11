#include "backend.hpp"
#include <ndn-cxx/data.hpp>
#include <iostream>
#include <cassert>

namespace DLedger {

Backend::Backend()
{
  this->initDatabase("/tmp/dledger-db");
}

Backend::~Backend()
{
  delete m_db;
}

LedgerRecord
Backend::getRecord(const std::string& recordId)
{
  leveldb::Slice key = recordId;
  std::string value;
  leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
  if (false == s.ok()) {
    std::cerr << "Unable to get value from database, key: " << recordId << std::endl;
    std::cerr << s.ToString() << std::endl;
    return LedgerRecord();
  }
  else {
    ndn::Block block((const uint8_t*)value.c_str(), value.size());
    auto data = std::make_shared<ndn::Data>(block);
    return LedgerRecord(data);
  }
}

bool
Backend::putRecord(const LedgerRecord& record)
{
  leveldb::Slice key = record.m_id;
  leveldb::Slice value((const char*)record.m_data.wire(), record.m_data.size());
  leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
  if (false == s.ok()) {
    std::cerr << "Unable to get value from database, key: " << record.m_id << std::endl;
    std::cerr << s.ToString() << std::endl;
    return false;
  }
}

void
Backend::deleteRecord(const std::string& recordId)
{
  leveldb::Slice key = recordId;
  leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), key);
  if (false == s.ok()) {
    std::cerr << "Unable to delete value from database, key: " << recordId << std::endl;
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

}