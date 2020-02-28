#include "dledger/record.hpp"

using namespace dledger;

int main(int argc, char const *argv[])
{
  Record record;
  record.addRecordItem("hello1");
  record.addRecordItem("hello2");
  return 0;
}
