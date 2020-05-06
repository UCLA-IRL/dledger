#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include <iostream>

#include <ndn-cxx/util/io.hpp>

using namespace dledger;

int main(int argc, char const *argv[])
{
  std::shared_ptr<Config> config = nullptr;
  try {
    config = Config::DefaultConfig();
  }
  catch(const std::exception& e) {
    std::cout << e.what() << std::endl;
    return 1;
  }

  security::KeyChain keychain;
  Face face;
  auto ledger = Ledger::initLedger(*config, keychain, face,"test");
  return 0;
}
