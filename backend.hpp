#include "ledger-record.hpp"
#include <leveldb/db.h>

namespace DLedger {

class Backend
{
public:
  Backend();
  ~Backend();

  LedgerRecord
  getRecord(const std::string& recordId);

  bool
  putRecord(const LedgerRecord& record);

  void
  deleteRecord(const std::string& recordId);

private:
  void
  initDatabase(const std::string& dbDir);

private:
  leveldb::DB* m_db;
};

} // namespace DLedger