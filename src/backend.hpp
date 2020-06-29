#ifndef DLEDGER_SRC_BACKEND_H_
#define DLEDGER_SRC_BACKEND_H_

#include <leveldb/db.h>

#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace dledger {

class Backend {
public:
  Backend(const std::string& dbDir);

public:
  ~Backend();

  // @param the recordName must be a full name (i.e., containing explicit digest component)
  shared_ptr<Data>
  getRecord(const Name& recordName) const;

  bool
  putRecord(const shared_ptr<const Data>& recordData);

  void
  deleteRecord(const Name& recordName);

  std::list<Name>
  listRecord(const Name& prefix) const;

private:
  leveldb::DB* m_db;
};

}  // namespace dledger

#endif  // DLEDGER_SRC_BACKEND_H_