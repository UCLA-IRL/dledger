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
#include <poll.h>
#include <ndn-cxx/encoding/block.hpp>
#include <utility>
#include <boost/thread.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

DynamicFunctionRunner *currentRunner;
wasm_memory_t *currentMemory;

ndn::Block DynamicFunctionRunner::getBlockFromMemory(wasm_memory_t *memory, size_t size, size_t offset){
    auto mem_arr = wasm_memory_data(memory);
    return ndn::Block(reinterpret_cast<uint8_t *>(mem_arr + offset), size);
}
void DynamicFunctionRunner::writeBlockToMemory(wasm_memory_t *memory, const ndn::Block& block, size_t offset){
    auto mem_arr = wasm_memory_data(memory);
    memcpy(mem_arr + offset, block.wire(), block.size());
}
void DynamicFunctionRunner::fileToVec(const std::string& fileName, wasm_byte_vec_t* vector) {
    // Read our input file, which in this case is a wat text file.
    FILE *file = fopen(fileName.c_str(), "r");
    assert(file != nullptr);
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    wasm_byte_vec_new_uninitialized(vector, file_size);
    assert(fread(vector->data, file_size, 1, file) == 1);
    fclose(file);
}

DynamicFunctionRunner::DynamicFunctionRunner()
{
  m_engine = wasm_engine_new();
  m_store = wasm_store_new(m_engine);
  assert(m_engine != nullptr);
  assert(m_store != nullptr);
}
DynamicFunctionRunner::~DynamicFunctionRunner()
{
  wasm_store_delete(m_store);
  wasm_engine_delete(m_engine);
}

void
DynamicFunctionRunner::runWatProgram(const std::string &fileName){
    // Read our input file, which in this case is a wat text file.
    wasm_byte_vec_t wat;
    fileToVec(fileName, &wat);

    // Parse the wat into the binary wasm format
    wasm_byte_vec_t wasm;
    wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
    if (error != nullptr)
        exit_with_error("failed to parse wat", error, nullptr);
    wasm_byte_vec_delete(&wat);

    runWasmProgram(&wasm);
    wasm_byte_vec_delete(&wasm);
}

void
DynamicFunctionRunner::runWasmProgram(const std::string &fileName){
    // Read our input file, which in this case is a wasm file.
    wasm_byte_vec_t wasm;
    fileToVec(fileName, &wasm);

    runWasmProgram(&wasm);
    wasm_byte_vec_delete(&wasm);
}

void
DynamicFunctionRunner::runWasmProgram(const ndn::Block &block){
    // copy code to wasm byte vec
    wasm_byte_vec_t wasm;
    wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
    memcpy(wasm.data, block.value(), block.value_size());

    runWasmProgram(&wasm);
    wasm_byte_vec_delete(&wasm);
}

void
DynamicFunctionRunner::runWasmProgram(wasm_byte_vec_t *binary){
    wasm_module_t *module = compile(binary);
    return run_program(module);
}

ndn::Block
DynamicFunctionRunner::runWatModule(const std::string &fileName, const ndn::Block& argument){
    // Read our input file, which in this case is a wat text file.
    wasm_byte_vec_t wat;
    fileToVec(fileName, &wat);

    // Parse the wat into the binary wasm format
    wasm_byte_vec_t wasm;
    wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
    if (error != nullptr)
        exit_with_error("failed to parse wat", error, nullptr);
    wasm_byte_vec_delete(&wat);

    ndn::Block b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

ndn::Block
DynamicFunctionRunner::runWasmModule(const std::string &fileName, const ndn::Block& argument){
    // Read our input file, which in this case is a wasm file.
    wasm_byte_vec_t wasm;
    fileToVec(fileName, &wasm);

    ndn::Block b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

ndn::Block
DynamicFunctionRunner::runWasmModule(const ndn::Block &block, const ndn::Block& argument){
    // copy code to wasm byte vec
    wasm_byte_vec_t wasm;
    wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
    memcpy(wasm.data, block.value(), block.value_size());

    ndn::Block b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

ndn::Block
DynamicFunctionRunner::runWasmModule(wasm_byte_vec_t *binary, const ndn::Block& argument){
    wasm_module_t *module = this->compile(binary);
    return run_wasi_module(module, argument);
}

wasm_module_t *
DynamicFunctionRunner::compile(wasm_byte_vec_t *wasm)
{
  // Compile our modules
  wasm_module_t *module = nullptr;
  wasmtime_error_t *error = wasmtime_module_new(m_engine, wasm, &module);
  if (!module)
    exit_with_error("failed to compile module", error, nullptr);
  wasm_byte_vec_delete(wasm);
  return module;
}

wasm_instance_t *
DynamicFunctionRunner::instantiate(wasm_module_t *module, const wasm_extern_t **imports, size_t import_length)
{
  // Instantiate.
  wasm_instance_t *instance = nullptr;
  wasm_trap_t *trap = nullptr;
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
  wasm_trap_t *trap = nullptr;
  wasi_instance_t *wasi = wasi_instance_new(m_store, "wasi_snapshot_preview1", wasi_config, &trap);
  if (wasi == nullptr)
    exit_with_error("failed to instantiate WASI", nullptr, trap);

  wasmtime_linker_t *linker = wasmtime_linker_new(m_store);
  wasmtime_error_t *error = wasmtime_linker_define_wasi(linker, wasi);
  if (error != nullptr)
    exit_with_error("failed to link wasi", error, nullptr);

  // Instantiate the module
  wasm_name_t empty;
  wasm_name_new_from_string(&empty, "");
  error = wasmtime_linker_module(linker, &empty, module);
  if (error != nullptr)
    exit_with_error("failed to instantiate module", error, nullptr);
  wasm_name_delete(&empty);

  return linker;
}

void
DynamicFunctionRunner::run_program(wasm_module_t *module)
{
    //instantiate
    auto linked_program = instantiate_wasi(module);

    // Run it.
    wasm_func_t *func;
    wasm_name_t empty;
    wasm_name_new_from_string(&empty, "");
    wasm_trap_t *trap = nullptr;
    wasmtime_error_t *error = wasmtime_linker_get_default(linked_program, &empty, &func);
    if (error != nullptr)
        exit_with_error("failed to locate default export for module", error, nullptr);
    error = wasmtime_func_call(func, nullptr, 0, nullptr, 0, &trap);
    if (error != nullptr)
        exit_with_error("error calling default export", error, trap);
    wasm_name_delete(&empty);
}

ndn::Block
DynamicFunctionRunner::run_wasi_module(wasm_module_t *module, const ndn::Block& argument){
    //pipe creation (read end, write end)
    int stdin_pipe_fds[2], stdout_pipe_fds[2];
    pipe(stdin_pipe_fds);
    pipe(stdout_pipe_fds);

    pid_t pid = fork();
    if (pid == 0) {
        //set pipes
        close(0);
        close(1);
        dup(stdin_pipe_fds[0]);
        dup(stdout_pipe_fds[1]);
        close(stdin_pipe_fds[0]);
        close(stdout_pipe_fds[1]);

        //instantiate
        auto linked_program = instantiate_wasi(module);

        // Run it.
        wasm_func_t *func;
        wasm_name_t empty;
        wasm_name_new_from_string(&empty, "");
        wasm_trap_t *trap = nullptr;
        wasmtime_error_t *error = wasmtime_linker_get_default(linked_program, &empty, &func);
        if (error != nullptr)
            exit_with_error("failed to locate default export for module", error, nullptr);
        error = wasmtime_func_call(func, nullptr, 0, nullptr, 0, &trap);
        if (error != nullptr)
            exit_with_error("error calling default export", error, trap);
        wasm_name_delete(&empty);
        exit(0);
    }

    std::vector<uint8_t> buf(8800);

    //pipe open
    FILE * stdin_pipe = fdopen(stdin_pipe_fds[1], "w");
    FILE * stdout_pipe = fdopen(stdout_pipe_fds[0], "r");
    assert(stdin_pipe && stdout_pipe);
    pollfd poll_structs[1] = {
            {stdout_pipe_fds[0], POLLIN, 0}
    };

    //argument
    int argument_size = argument.size();
    assert(fwrite(&argument_size, 4, 1, stdin_pipe) == 1);
    assert(fwrite(argument.wire(), argument_size, 1, stdin_pipe) == 1);
    fflush(stdin_pipe);

    //waiting
    bool done = false;
    ndn::Block return_block;
    for (int i = 0; i < 200; i ++) { // 2 second wait total
        //check io
        if (poll(poll_structs, 1, 10) != 0) {
            if ((poll_structs[0].revents & POLLHUP) || (poll_structs[0].revents & POLLERR)) {
                break;
            }
            assert(fread(buf.data(), 1, 3, stdout_pipe) == 3);
            if (!memcmp(buf.data(), "GET", 3)) {
                assert(fread(buf.data(), 4, 1, stdout_pipe) == 1);
                uint32_t block_size = *reinterpret_cast<uint32_t *>(buf.data());
                assert(fread(buf.data(), block_size, 1, stdout_pipe) == 1);
                auto name_block = ndn::Block(buf.data(), block_size);
                auto result_block = m_getBlock(name_block);
                if (result_block) {
                    block_size = result_block->size();
                    assert(fwrite(&block_size, 4, 1, stdin_pipe) == 1);
                    assert(fwrite(result_block->wire(), block_size, 1, stdin_pipe) == 1);
                    fflush(stdin_pipe);
                } else {
                    block_size = 0;
                    assert(fwrite(&block_size, 4, 1, stdin_pipe) == 1);
                    fflush(stdin_pipe);
                }
            } else if (!memcmp(buf.data(), "SET", 3)) {
                assert(fread(buf.data(), 4, 1, stdout_pipe) == 1);
                uint32_t name_size = *reinterpret_cast<uint32_t *>(buf.data());
                assert(fread(buf.data(), 4, 1, stdout_pipe) == 1);
                uint32_t block_size = *reinterpret_cast<uint32_t *>(buf.data());
                assert(fread(buf.data(), name_size, 1, stdout_pipe) == 1);
                auto name_block = ndn::Block(buf.data(), name_size);
                assert(fread(buf.data(), block_size, 1, stdout_pipe) == 1);
                auto block_block = ndn::Block(buf.data(), block_size);
                auto result = m_setBlock(name_block, block_block);
                assert(fwrite(&result, 4, 1, stdin_pipe) == 1);
                fflush(stdin_pipe);
            } else if (!memcmp(buf.data(), "DNE", 3)) {
                assert(fread(buf.data(), 4, 1, stdout_pipe) == 1);
                uint32_t block_size = *reinterpret_cast<uint32_t *>(buf.data());
                assert(fread(buf.data(), block_size, 1, stdout_pipe) == 1);
                return_block = ndn::Block(buf.data(), block_size);
            }
        }
        if ((done = kill(pid, 0) == -1))
            break;
    }

    if (!done) {
        kill(pid, SIGKILL);
    }
    fclose(stdin_pipe);
    fclose(stdout_pipe);
    return return_block;
}

ndn::Block
DynamicFunctionRunner::run_module(wasm_module_t *module,const ndn::Block& argument)
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
    return nullptr;
  });
  wasm_functype_t *setter_ty = wasm_functype_new_2_1(i32type, i32type, i32type);
  wasm_func_t *setter = wasm_func_new(m_store, setter_ty, [](const wasm_val_t args[], wasm_val_t results[]) -> wasm_trap_t * {
    results[0].kind = WASM_I32;
    results[0].of.i32 = currentRunner->setBlockCallback(args[0].of.i32, args[1].of.i32, currentMemory);
    return nullptr;
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

  //prep argument
  memcpy(wasm_memory_data(memory), argument.wire(), argument.size());
  wasm_val_t arg_val = {.kind=WASM_I32, .of.i32=static_cast<int32_t>(argument.size())};
  wasm_val_t ret_val = {0};

  // And call it!
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(exec_func, &arg_val, 1, &ret_val, 1, &trap);
  if (error != nullptr || trap != nullptr)
    exit_with_error("failed to call function", error, trap);

  //cleanup
  wasm_module_delete(module);
  ndn::Block return_block = getBlockFromMemory(memory, static_cast<size_t>(ret_val.of.i32));
  wasm_memory_delete(memory);
  return return_block;
}

void
DynamicFunctionRunner::exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap)
{
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  wasm_byte_vec_new_empty(&error_message);
  if (error != nullptr) {
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
  m_getBlock = std::move(getBlock);
}
void
DynamicFunctionRunner::setSetBlockFunc(std::function<int(ndn::Block, ndn::Block)> setBlock)
{
  m_setBlock = std::move(setBlock);
}

int
DynamicFunctionRunner::getBlockCallback(int len, wasm_memory_t *memory)
{
  ndn::Block request_param = getBlockFromMemory(memory, len);
  if (!m_getBlock) {
    return 0;
  }
  auto result = m_getBlock(request_param);
  if (!result) {
    return 0;
  }
  writeBlockToMemory(memory, *result);
  return result->size();
}

int
DynamicFunctionRunner::setBlockCallback(int len_param, int len_value, wasm_memory_t *memory)
{
  ndn::Block param_block = getBlockFromMemory(memory, len_param);
  ndn::Block value_block = getBlockFromMemory(memory, len_value, len_param);
  if (!m_getBlock) {
    return 0;
  }
  return m_setBlock(param_block, value_block);
}