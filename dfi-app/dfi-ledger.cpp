//
// Created by Tyler on 10/21/20.
//

#include "dynamic-function-runner.h"
#include <ndn-cxx/name.hpp>
using namespace ndn;

int main() {
    DynamicFunctionRunner runner;

    { // example of running a module
        runner.setGetBlockFunc([](ndn::Block block) -> optional<ndn::Block> {
            auto name = ndn::Name(block);
            printf("Called getBlock on: %s\n", name.toUri().c_str());
            return name.appendVersion().wireEncode();
        });
        auto ret = runner.runWatModule("dfi-app/test0.wat", ndn::Name("/a/b/c").wireEncode());
        auto name = ndn::Name(ret);
        printf("Returned with on: %s\n", name.toUri().c_str());
    }

    // example of running a program
    runner.runWasmProgram("dfi-app/test1.wasm");
    return 0;
}