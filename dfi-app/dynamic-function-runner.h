//
// Created by Tyler on 10/20/20.
//

#ifndef DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
#define DLEDGER_DYNAMIC_FUNCTION_RUNNER_H

#include <wasmer.hh>

#include <ndn-cxx/encoding/block.hpp>

/**
 * Because of C integration issue, there should be only one of this object running
 * at any given time.
 */
class DynamicFunctionRunner {
public:

  /*
  void
  runWatProgram(const std::string &fileName) const;

  void
  runWasmProgram(const std::string &fileName) const;

  void
  runWasmProgram(const ndn::Block &block) const;

  void
  runWasmProgram(const std::vector<uint8_t>& binary) const;
   */
  /*
  std::vector<uint8_t>
  runWatModule(std::string &fileName, const std::vector<uint8_t>& argument) const;
   */

  std::vector<uint8_t>
  runWasmModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmModule(std::vector<uint8_t>& binary,const std::vector<uint8_t>& argument) const;

  /*
  std::vector<uint8_t>
  runWatPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  runWasmPipeModule(std::vector<uint8_t>& binary,const std::vector<uint8_t>& argument) const;
   */

  void
  setCallback(std::string name, std::function<std::vector<uint8_t>(std::vector<uint8_t>)> func);

private:
    static std::vector<uint8_t> getBlockFromMemory(wasmer_memory_t *memory, size_t size, size_t offset = 0);
    static void writeBlockToMemory(wasmer_memory_t *memory, const std::vector<uint8_t>& block, size_t offset = 0);
    static std::vector<uint8_t> fileToVec(const std::string& fileName);
    static const int callbackNameSize = 4;
    static wasmer_memory_t *create_wasmer_memory();

private:

    wasmer_instance_t *
    instantiate(std::vector<uint8_t>& module, wasmer_import_t *imports, size_t import_length) const;

    wasmer_instance_t *
    instantiate_wasi(std::vector<uint8_t>& module) const;

//  void
//  run_program(wasm_module_t *module) const;

//  std::vector<uint8_t>
//  run_wasi_module(wasm_module_t *module, const std::vector<uint8_t>& argument) const;

  std::vector<uint8_t>
  run_module(std::vector<uint8_t>& module, const std::vector<uint8_t>& argument) const;

  int
  executeCallback(int len, wasmer_memory_t *memory) const;

  void
  executeCallback(FILE *wasms_out, FILE *wasms_in, std::vector<uint8_t>& return_buffer) const;

  std::map<std::string, std::function<std::vector<uint8_t>(std::vector<uint8_t>)>> m_callbackList;
};

#endif  //DLEDGER_DYNAMIC_FUNCTION_RUNNER_H
