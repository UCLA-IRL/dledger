//
// Created by Tyler on 10/21/20.
//

#include "dynamic-function-runner.h"
#include <ndn-cxx/name.hpp>
using namespace ndn;

int main() {
    DynamicFunctionRunner runner;

    { // example of running a module
        runner.setCallback("GETB", [](std::vector<uint8_t> buf) -> std::vector<uint8_t> {
            std::string input(reinterpret_cast<char *>(buf.data()), buf.size());
            printf("Called callback on: %s\n", input.c_str());
            std::string ans = "Hello from host!";
            std::vector<uint8_t> vec(ans.size());
            memcpy(vec.data(), ans.c_str(), ans.size());
            return std::move(vec);
        });
        std::string argument = "argument from host";
        std::vector<uint8_t> argument_vec(argument.size());
        memcpy(argument_vec.data(), argument.c_str(), argument.size());
        auto ret = runner.runWasmModule("dfi-app/test2.wasm", argument_vec);
        std::string return_val(reinterpret_cast<char *>(ret.data()), ret.size());
        printf("Returned with : %s\n\n", return_val.c_str());
        fflush(stdout);

        // try out C
        ret = runner.runWasmModule("dfi-app/c-test1/wasm-build/c-test1.wasm", argument_vec);
        std::string c_return_val(reinterpret_cast<char *>(ret.data()), ret.size());
        printf("C Returned with : %s\n", c_return_val.c_str());
    }

    // example of running a program
    runner.runWasmProgram("dfi-app/test1.wasm");
    return 0;
}