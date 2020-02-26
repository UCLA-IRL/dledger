#include <leveldb/db.h>
#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace dledger {

class Backend
{
public:
  Backend();
  ~Backend();

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