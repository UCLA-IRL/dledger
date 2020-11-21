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
  runWatProgram(const std::string &fileName) const;

  void
  runWasmProgram(const std::string &fileName) const;

  void
  runWasmProgram(const ndn::Block &block) const;

  void
  runWasmProgram(wasm_byte_vec_t *binary) const;

  std::vector<uint8_t>
  runWatModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmModule(wasm_byte_vec_t *binary,const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWatPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(wasm_byte_vec_t *binary,const std::vector<uint8_t>& argument) const;

  void
  setCallback(std::string name, std::function<std::vector<uint8_t>(std::vector<uint8_t>)> func);

private:
    static std::vector<uint8_t> getBlockFromMemory(wasm_memory_t *memory, size_t size, size_t offset = 0);
    static void writeBlockToMemory(wasm_memory_t *memory, const std::vector<uint8_t>& block, size_t offset = 0);
    static void fileToVec(const std::string& fileName, wasm_byte_vec_t* vector);
    static const int callbackNameSize = 4;

private:

  wasm_module_t *
  compile(wasm_byte_vec_t *wasm) const;

  wasm_instance_t *
  instantiate(wasm_module_t *module, const wasm_extern_t **imports, size_t import_length) const;

  wasmtime_linker_t *
  instantiate_wasi(wasm_module_t *module) const;

  void
  run_program(wasm_module_t *module) const;

    std::vector<uint8_t>
  run_wasi_module(wasm_module_t *module, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  run_module(wasm_module_t *module, const std::vector<uint8_t>& argument) const;

  static void
  exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);

  int
  executeCallback(int len, wasm_memory_t *memory) const;

  void
  executeCallback(FILE *wasms_out, FILE *wasms_in, std::vector<uint8_t>& return_buffer) const;

  std::map<std::string, std::function<std::vector<uint8_t>(std::vector<uint8_t>)>> m_callbackList;
  wasm_engine_t *m_engine;
  wasm_store_t *m_store;
};

#endif  //DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
