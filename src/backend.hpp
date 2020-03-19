#ifndef DLEDGER_SRC_BACKEND_H_
#define DLEDGER_SRC_BACKEND_H_

#include <leveldb/db.h>
#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace dledger {

class Backend
{
public:
  Backend();
  ~Backend();

  // @param the recordName must be a full name (i.e., containing explicit digest component)
  shared_ptr<Data>
  getRecord(const Name& recordName);

  bool
  putRecord(const shared_ptr<Data>& recordData);

  void
  deleteRecord(const Name& recordName);

private:
  void
  initDatabase(const std::string& dbDir);

private:
  leveldb::DB* m_db;
};

} // namespace DLedger

#endif // DLEDGER_SRC_BACKEND_H_