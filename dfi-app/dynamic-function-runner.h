//
// Created by Tyler on 10/20/20.
//

#ifndef DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
#define DLEDGER_DYNAMIC_FUNCTION_RUNNER_H

#include <wasm.h>
#include <wasmtime.h>

#include <ndn-cxx/encoding/block.hpp>

/**
 * Because of C integration issue, there should be only one of this object running
 * at any given time.
 */
class DynamicFunctionRunner {
public:
  DynamicFunctionRunner();
  ~DynamicFunctionRunner();

  void
  runWatProgram(const std::string &fileName);

  void
  runWasmProgram(const std::string &fileName);

  void
  runWasmProgram(const ndn::Block &block);

  void
  runWasmProgram(wasm_byte_vec_t *binary);

  ndn::Block
  runWatModule(const std::string &fileName, const ndn::Block& argument);

  ndn::Block
  runWasmModule(const std::string &fileName, const ndn::Block& argument);

  ndn::Block
  runWasmModule(const ndn::Block &block, const ndn::Block& argument);

  ndn::Block
  runWasmModule(wasm_byte_vec_t *binary,const ndn::Block& argument);

  void
  setGetBlockFunc(std::function<ndn::optional<ndn::Block>(ndn::Block)> getBlock);

  void
  setSetBlockFunc(std::function<int(ndn::Block, ndn::Block)> setBlock);

private:
    static ndn::Block getBlockFromMemory(wasm_memory_t *memory, size_t size, size_t offset = 0);
    static void writeBlockToMemory(wasm_memory_t *memory, const ndn::Block& block, size_t offset = 0);
    static void fileToVec(const std::string& fileName, wasm_byte_vec_t* vector);

private:

  wasm_module_t *
  compile(wasm_byte_vec_t *wasm);

  wasm_instance_t *
  instantiate(wasm_module_t *module, const wasm_extern_t **imports, size_t import_length);

  wasmtime_linker_t *
  instantiate_wasi(wasm_module_t *module);

  void
  run_program(wasm_module_t *module);

  ndn::Block
  run_wasi_module(wasm_module_t *module, const ndn::Block& argument);

  ndn::Block
  run_module(wasm_module_t *module, const ndn::Block& argument);

  void
  exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);

  int
  getBlockCallback(int len, wasm_memory_t *memory);
  int
  setBlockCallback(int len_param, int len_value, wasm_memory_t *memory);

  std::function<ndn::optional<ndn::Block>(ndn::Block)> m_getBlock;
  std::function<int(ndn::Block, ndn::Block)> m_setBlock;
  wasm_engine_t *m_engine;
  wasm_store_t *m_store;
};

#endif  //DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
