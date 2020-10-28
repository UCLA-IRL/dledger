/*
Example of instantiating a WebAssembly which uses WASI imports.
You can compile and run this example on Linux with:
   cargo build --release -p wasmtime-c-api
   cc examples/wasi/main.c \
       -I crates/c-api/include \
       -I crates/c-api/wasm-c-api/include \
       target/release/libwasmtime.a \
       -lpthread -ldl -lm \
       -o wasi
   ./wasi
Note that on Windows and macOS the command will be similar, but you'll need
to tweak the `-lpthread` and such annotations.
*/

#include "dynamic-function-runner.h"

#include <wasi.h>
#include <wasm.h>
#include <wasmtime.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ndn-cxx/encoding/block.hpp>

DynamicFunctionRunner *currentRunner;
wasm_memory_t *currentMemory;

DynamicFunctionRunner::DynamicFunctionRunner()
{
  m_engine = wasm_engine_new();
  m_store = wasm_store_new(m_engine);
  assert(m_engine != NULL);
  assert(m_store != NULL);
}
DynamicFunctionRunner::~DynamicFunctionRunner()
{
  wasm_store_delete(m_store);
  wasm_engine_delete(m_engine);
}

void
DynamicFunctionRunner::runWatFile(const std::string &fileName)
{
  // Read our input file, which in this case is a wat text file.
  FILE *file = fopen(fileName.c_str(), "r");
  assert(file != NULL);
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t wat;
  wasm_byte_vec_new_uninitialized(&wat, file_size);
  assert(fread(wat.data, file_size, 1, file) == 1);
  fclose(file);

  // Parse the wat into the binary wasm format
  wasm_byte_vec_t wasm;
  wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
  if (error != NULL)
    exit_with_error("failed to parse wat", error, NULL);
  wasm_byte_vec_delete(&wat);

  wasm_module_t *module = this->compile(&wasm);
}

void
DynamicFunctionRunner::runWasmFile(const std::string &fileName)
{
  // Read our input file, which in this case is a wasm file.
  FILE *file = fopen(fileName.c_str(), "r");
  assert(file != NULL);
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t wasm;
  wasm_byte_vec_new_uninitialized(&wasm, file_size);
  assert(fread(wasm.data, file_size, 1, file) == 1);
  fclose(file);

  wasm_module_t *module = this->compile(&wasm);
}

void
DynamicFunctionRunner::runWasmBlock(const ndn::Block &block)
{
  // copy code to wasm byte vec
  wasm_byte_vec_t wasm;
  wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
  memcpy(wasm.data, block.value(), block.value_size());

  wasm_module_t *module = compile(&wasm);
}

wasm_module_t *
DynamicFunctionRunner::compile(wasm_byte_vec_t *wasm)
{
  // Compile our modules
  wasm_module_t *module = NULL;
  wasmtime_error_t *error = wasmtime_module_new(m_engine, wasm, &module);
  if (!module)
    exit_with_error("failed to compile module", error, NULL);
  wasm_byte_vec_delete(wasm);
  return module;
}

wasm_instance_t *
DynamicFunctionRunner::instantiate(wasm_module_t *module, const wasm_extern_t **imports, size_t import_length)
{
  // Instantiate.
  wasm_instance_t *instance = NULL;
  wasm_trap_t *trap = NULL;
  wasmtime_error_t *error = wasmtime_instance_new(m_store, module, imports, import_length, &instance, &trap);
  if (!instance)
    exit_with_error("failed to instantiate", error, trap);
  return instance;
}

wasmtime_linker_t *
DynamicFunctionRunner::instantiate_wasi(wasm_module_t *module)
{
  // Instantiate wasi
  wasi_config_t *wasi_config = wasi_config_new();
  assert(wasi_config);
  wasi_config_inherit_argv(wasi_config);
  wasi_config_inherit_env(wasi_config);
  wasi_config_inherit_stdin(wasi_config);
  wasi_config_inherit_stdout(wasi_config);
  wasi_config_inherit_stderr(wasi_config);
  wasm_trap_t *trap = NULL;
  wasi_instance_t *wasi = wasi_instance_new(m_store, "wasi_snapshot_preview1", wasi_config, &trap);
  if (wasi == NULL)
    exit_with_error("failed to instantiate WASI", NULL, trap);

  wasmtime_linker_t *linker = wasmtime_linker_new(m_store);
  wasmtime_error_t *error = wasmtime_linker_define_wasi(linker, wasi);
  if (error != NULL)
    exit_with_error("failed to link wasi", error, NULL);

  // Instantiate the module
  wasm_name_t empty;
  wasm_name_new_from_string(&empty, "");
  error = wasmtime_linker_module(linker, &empty, module);
  if (error != NULL)
    exit_with_error("failed to instantiate module", error, NULL);
  wasm_name_delete(&empty);

  return linker;
}

void
DynamicFunctionRunner::run_program(wasmtime_linker_t *linker_program)
{
  // Run it.
  wasm_func_t *func;
  wasm_name_t empty;
  wasm_trap_t *trap = NULL;
  wasmtime_error_t *error = wasmtime_linker_get_default(linker_program, &empty, &func);
  if (error != NULL)
    exit_with_error("failed to locate default export for module", error, NULL);
  error = wasmtime_func_call(func, NULL, 0, NULL, 0, &trap);
  if (error != NULL)
    exit_with_error("error calling default export", error, trap);
  wasm_name_delete(&empty);
}

void
DynamicFunctionRunner::runWasmModule(wasm_module_t *module)
{
  //make imports
  wasm_limits_t memory_limit = {.min = 1, .max = 1};
  wasm_memorytype_t *memorytype = wasm_memorytype_new(&memory_limit);
  wasm_memory_t *memory = wasm_memory_new(m_store, memorytype);
  wasm_memorytype_delete(memorytype);
  currentRunner = this;
  currentMemory = memory;

  auto i32type = wasm_valtype_new_i32();
  wasm_functype_t *getter_ty = wasm_functype_new_1_1(i32type, i32type);
  wasm_func_t *getter = wasm_func_new(m_store, getter_ty, [](const wasm_val_t args[], wasm_val_t results[]) -> wasm_trap_t * {
    results[0].kind = WASM_I32;
    results[0].of.i32 = currentRunner->getBlockCallback(args[0].of.i32, currentMemory);
    return NULL;
  });
  wasm_functype_t *setter_ty = wasm_functype_new_2_1(i32type, i32type, i32type);
  wasm_func_t *setter = wasm_func_new(m_store, setter_ty, [](const wasm_val_t args[], wasm_val_t results[]) -> wasm_trap_t * {
    results[0].kind = WASM_I32;
    results[0].of.i32 = currentRunner->setBlockCallback(args[0].of.i32, args[1].of.i32, currentMemory);
    return NULL;
  });
  wasm_valtype_delete(i32type);

  const wasm_extern_t *imports[] = {
      wasm_memory_as_extern(memory),
      /*Getter*/ wasm_func_as_extern(getter),
      /*Setter*/ wasm_func_as_extern(setter)};

  wasm_instance_t *instance = instantiate(module, imports, 3);

  //get exports
  wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  if (exports.size < 1) {
    BOOST_THROW_EXCEPTION(std::runtime_error("wasm does not have required export"));
  }
  wasm_func_t *exec_func = wasm_extern_as_func(exports.data[0]);
  if (!exec_func) {
    BOOST_THROW_EXCEPTION(std::runtime_error("wasm does not have required export"));
  }

  // And call it!
  wasm_trap_t *trap;
  wasmtime_error_t *error = wasmtime_func_call(exec_func, NULL, 0, NULL, 0, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);

  //cleanup
  wasm_module_delete(module);
}

void
DynamicFunctionRunner::exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap)
{
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  }
  else {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int)error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}

void
DynamicFunctionRunner::setGetBlockFunc(std::function<ndn::optional<ndn::Block>(ndn::Block)> getBlock)
{
  m_getBlock = getBlock;
}
void
DynamicFunctionRunner::setSetBlockFunc(std::function<int(ndn::Block, ndn::Block)> setBlock)
{
  m_setBlock = setBlock;
}

int
DynamicFunctionRunner::getBlockCallback(int len, wasm_memory_t *memory)
{
  auto mem_arr = wasm_memory_data(memory);
  ndn::Block request_param = ndn::Block(reinterpret_cast<uint8_t *>(mem_arr), len);
  if (!m_getBlock) {
    return 0;
  }
  auto result = m_getBlock(request_param);
  if (!result) {
    return 0;
  }
  memcpy(mem_arr, result->wire(), result->size());
  return result->size();
}

int
DynamicFunctionRunner::setBlockCallback(int len_param, int len_value, wasm_memory_t *memory)
{
  auto mem_arr = wasm_memory_data(memory);
  ndn::Block param_block = ndn::Block(reinterpret_cast<uint8_t *>(mem_arr), len_param);
  ndn::Block value_block = ndn::Block(reinterpret_cast<uint8_t *>(mem_arr + len_param), len_value);
  if (!m_getBlock) {
    return 0;
  }
  return m_setBlock(param_block, value_block);
}