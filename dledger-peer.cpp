#include "dledger-peer.hpp" 

namespace ndn {
namespace dledger {

LedgerRecord::LedgerRecord(shared_ptr<const Data> contentObject,
                           int weight, int entropy, bool isArchived)
  : block(contentObject)
  , weight(weight)
  , entropy(entropy)
  , isArchived(isArchived)
{
  //Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  //int num = static_cast<int>(x->GetValue()*100);
  //if (num > 95) {
  //{
  //  isASample = true;
  //  creationTime = Simulator::Now();
  //}
}

Peer::Peer()
  : m_firstTime(true)
  , m_syncFirstTime(true)
{
}

std::vector<std::string>
Peer::GetApprovedBlocks(shared_ptr<const Data> data)
{
  std::vector<std::string> approvedBlocks;
  auto content = ::ndn::encoding::readString(data->getContent());
  int nSlash = 0;
  const char *st, *ed;
  for(st = ed = content.c_str(); *ed && *ed != '*'; ed ++){
    if(*ed == ':'){
      if(nSlash >= 2){
        approvedBlocks.push_back(std::string(st, ed));
      }
      nSlash = 0;
      st = ed + 1;
    }else if(*ed == '/'){
      nSlash ++;
    }
  }
  if(nSlash >= 2){
    approvedBlocks.push_back(std::string(st, ed));
  }

  return approvedBlocks;
}

}
}

int
main(int argc, char** argv)
{
  return 1;
}
