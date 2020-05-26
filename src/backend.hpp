#ifndef DLEDGER_SRC_BACKEND_H_
#define DLEDGER_SRC_BACKEND_H_

#include <leveldb/db.h>

#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace dledger {

class Backend {
public:
  Backend(const std::string& dbDir = "/tmp/dledger-db");
  ~Backend();

  // @param the recordName must be a full name (i.e., containing explicit digest component)
  shared_ptr<Data>
  getRecord(const Name& recordName);

  bool
  putRecord(const shared_ptr<const Data>& recordData);

  void
  deleteRecord(const Name& recordName);

  void
  initDatabase(const std::string& dbDir);

private:
  leveldb::DB* m_db;
};

}  // namespace dledger

#endif  // DLEDGER_SRC_BACKEND_H_