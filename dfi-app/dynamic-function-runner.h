//
// Created by Tyler on 10/20/20.
//

#ifndef DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
#define DLEDGER_DYNAMIC_FUNCTION_RUNNER_H

#include <wasm.h>
#include <ndn-cxx/encoding/block.hpp>
#include <wasmtime.h>

class DynamicFunctionRunner{
public:
    DynamicFunctionRunner();
    ~DynamicFunctionRunner();

    void run_wasm_block(ndn::Block block);

private:
    void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);

    wasm_engine_t *engine;
    wasm_store_t *store;
};

#endif //DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
