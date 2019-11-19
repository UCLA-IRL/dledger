// AUTHOR: Zhiyi Zhang
// EMAIL: zhiyi@cs.ucla.edu
// License: LGPL v3.0

#include "peer.hpp"
#include <ndn-cxx/util/random.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>

namespace po = boost::program_options;
using namespace ndn;

static void
usage(std::ostream& os, const po::options_description& options)
{
  os << "Usage: DLedger Service\n"
     << options;
}

class Options
{
public:
  Options()
    : mcPrefix("/ndn/multicast/dledger")
    , routablePrefix("/ndn/multicast/dledger/node")
  {
    routablePrefix.append(std::to_string(ndn::random::generateWord64()));
  }

public:
  ndn::Name mcPrefix;
  ndn::Name routablePrefix;
};

namespace DLedger {

class Program
{
public:
  explicit
  Program(const Options& options)
    : peer(options.mcPrefix, options.routablePrefix)
  {
  }

  void
  run()
  {
    peer.run();
  }

private:
  Peer peer;
};

} // namespace DLedger

int
main(int argc, char** argv)
{
  Options opt;

  po::options_description options("Required options");
  options.add_options()
    ("help,h", "print help message")
    ("multicastPrefix,M", po::value<ndn::Name>(&opt.mcPrefix), "multicast prefix")
    ("routablePrefix,R", po::value<ndn::Name>(&opt.routablePrefix), "routable prefix");
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);
  }
  catch (boost::program_options::error&) {
    usage(std::cerr, options);
    return 2;
  }

  DLedger::Program program(opt);
  program.run();
  return 1;
}
