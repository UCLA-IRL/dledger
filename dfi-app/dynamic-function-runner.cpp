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
#include <utility>
#include <unistd.h>
#include <csignal>

const DynamicFunctionRunner *currentRunner;
wasm_memory_t *currentMemory;

std::vector<uint8_t> DynamicFunctionRunner::getBlockFromMemory(wasm_memory_t *memory, size_t size, size_t offset){
    auto mem_arr = wasm_memory_data(memory);
    std::vector<uint8_t> buf;
    memcpy(buf.data(), reinterpret_cast<uint8_t *>(mem_arr + offset), size);
    return std::move(buf);
}
void DynamicFunctionRunner::writeBlockToMemory(wasm_memory_t *memory, const std::vector<uint8_t>& block, size_t offset){
    auto mem_arr = wasm_memory_data(memory);
    memcpy(mem_arr + offset, block.data(), block.size());
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
DynamicFunctionRunner::runWatProgram(const std::string &fileName) const {
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
DynamicFunctionRunner::runWasmProgram(const std::string &fileName) const {
    // Read our input file, which in this case is a wasm file.
    wasm_byte_vec_t wasm;
    fileToVec(fileName, &wasm);

    runWasmProgram(&wasm);
    wasm_byte_vec_delete(&wasm);
}

void
DynamicFunctionRunner::runWasmProgram(const ndn::Block &block) const {
    // copy code to wasm byte vec
    wasm_byte_vec_t wasm;
    wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
    memcpy(wasm.data, block.value(), block.value_size());

    runWasmProgram(&wasm);
    wasm_byte_vec_delete(&wasm);
}

void
DynamicFunctionRunner::runWasmProgram(wasm_byte_vec_t *binary) const {
    wasm_module_t *module = compile(binary);
    return run_program(module);
}

std::vector<uint8_t>
DynamicFunctionRunner::runWatModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wat text file.
    wasm_byte_vec_t wat;
    fileToVec(fileName, &wat);

    // Parse the wat into the binary wasm format
    wasm_byte_vec_t wasm;
    wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
    if (error != nullptr)
        exit_with_error("failed to parse wat", error, nullptr);
    wasm_byte_vec_delete(&wat);

    auto b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wasm file.
    wasm_byte_vec_t wasm;
    fileToVec(fileName, &wasm);

    auto b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const {
    // copy code to wasm byte vec
    wasm_byte_vec_t wasm;
    wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
    memcpy(wasm.data, block.value(), block.value_size());

    auto b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(wasm_byte_vec_t *binary, const std::vector<uint8_t>& argument) const {
    wasm_module_t *module = this->compile(binary);
    return run_module(module, argument);
}

std::vector<uint8_t>
DynamicFunctionRunner::runWatPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wat text file.
    wasm_byte_vec_t wat;
    fileToVec(fileName, &wat);

    // Parse the wat into the binary wasm format
    wasm_byte_vec_t wasm;
    wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
    if (error != nullptr)
        exit_with_error("failed to parse wat", error, nullptr);
    wasm_byte_vec_delete(&wat);

    auto b = runWasmPipeModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmPipeModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wasm file.
    wasm_byte_vec_t wasm;
    fileToVec(fileName, &wasm);

    auto b = runWasmPipeModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmPipeModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const {
    // copy code to wasm byte vec
    wasm_byte_vec_t wasm;
    wasm_byte_vec_new_uninitialized(&wasm, block.value_size());
    memcpy(wasm.data, block.value(), block.value_size());

    auto b = runWasmPipeModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmPipeModule(wasm_byte_vec_t *binary, const std::vector<uint8_t>& argument) const {
    wasm_module_t *module = this->compile(binary);
    return run_wasi_module(module, argument);
}

wasm_module_t *
DynamicFunctionRunner::compile(wasm_byte_vec_t *wasm) const
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
DynamicFunctionRunner::instantiate(wasm_module_t *module, const wasm_extern_t **imports, size_t import_length) const
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
DynamicFunctionRunner::instantiate_wasi(wasm_module_t *module) const
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
DynamicFunctionRunner::run_program(wasm_module_t *module) const
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

std::vector<uint8_t>
DynamicFunctionRunner::run_wasi_module(wasm_module_t *module, const std::vector<uint8_t>& argument) const{
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
        close(stdin_pipe_fds[1]);
        close(stdout_pipe_fds[0]);

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
        fclose(stdin);
        fclose(stdout);
        exit(0);
    }

    std::vector<uint8_t> buf(8800);

    //pipe open
    FILE * stdin_pipe = fdopen(stdin_pipe_fds[1], "w");
    FILE * stdout_pipe = fdopen(stdout_pipe_fds[0], "r");
    assert(stdin_pipe && stdout_pipe);
    close(stdin_pipe_fds[0]);
    close(stdout_pipe_fds[1]);
    pollfd poll_structs[1] = {
            {stdout_pipe_fds[0], POLLIN, 0}
    };

    //argument
    int argument_size = argument.size();
    assert(fwrite(&argument_size, 4, 1, stdin_pipe) == 1);
    assert(fwrite(argument.data(), argument_size, 1, stdin_pipe) == 1);
    fflush(stdin_pipe);

    //waiting
    bool done = false;
    std::vector<uint8_t> return_buffer;
    for (int i = 0; i < 200; i ++) { // 2 second wait total
        //check io
        if (poll(poll_structs, 1, 10) != 0) {
            if (poll_structs[0].revents & POLLIN) {
                executeCallback(stdout_pipe, stdin_pipe, return_buffer);
            }
            if ((poll_structs[0].revents & POLLHUP) || (poll_structs[0].revents & POLLERR)) {
                done = true;
                break;
            }
        }
        if ((done = ((waitpid(pid, nullptr, WNOHANG) != 0)))) {
            break;
        }
    }

    if (!done) {
        kill(pid, SIGKILL);
    }
    fclose(stdin_pipe);
    fclose(stdout_pipe);
    return std::move(return_buffer);
}

std::vector<uint8_t>
DynamicFunctionRunner::run_module(wasm_module_t *module,const std::vector<uint8_t>& argument) const
{
  //make imports
  wasm_limits_t memory_limit = {.min = 1, .max = 1};
  wasm_memorytype_t *memorytype = wasm_memorytype_new(&memory_limit);
  wasm_memory_t *memory = wasm_memory_new(m_store, memorytype);
  wasm_memorytype_delete(memorytype);
  currentRunner = this;
  currentMemory = memory;

  auto i32type = wasm_valtype_new_i32();
  wasm_functype_t *callback_ty = wasm_functype_new_1_1(i32type, i32type);
  wasm_func_t *callback = wasm_func_new(m_store, callback_ty, [](const wasm_val_t args[], wasm_val_t results[]) -> wasm_trap_t * {
    results[0].kind = WASM_I32;
    results[0].of.i32 = currentRunner->executeCallback(args[0].of.i32, currentMemory);
    return nullptr;
  });
  wasm_valtype_delete(i32type);

  const wasm_extern_t *imports[] = {
      wasm_memory_as_extern(memory),
      /*Getter*/ wasm_func_as_extern(callback)};

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
  memcpy(wasm_memory_data(memory), argument.data(), argument.size());
  wasm_val_t arg_val = {.kind=WASM_I32, .of.i32=static_cast<int32_t>(argument.size())};
  wasm_val_t ret_val = {0};

  // And call it!
  wasm_trap_t *trap = nullptr;
  wasmtime_error_t *error = wasmtime_func_call(exec_func, &arg_val, 1, &ret_val, 1, &trap);
  if (error != nullptr || trap != nullptr)
    exit_with_error("failed to call function", error, trap);

  //cleanup
  wasm_module_delete(module);
  std::vector<uint8_t> return_block = getBlockFromMemory(memory, static_cast<size_t>(ret_val.of.i32));
  wasm_memory_delete(memory);
  return std::move(return_block);
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
DynamicFunctionRunner::setCallback(std::string name, std::function<std::vector<uint8_t>(std::vector<uint8_t>)> func){
    assert(name.length() == callbackNameSize);
    m_callbackList[name] = std::move(func);
}

int
DynamicFunctionRunner::executeCallback(int len, wasm_memory_t *memory) const {
    std::string func_name(wasm_memory_data(memory), callbackNameSize);
    std::vector<uint8_t> request_param = getBlockFromMemory(memory, len, callbackNameSize);
    auto it = m_callbackList.find(func_name);
    if (it == m_callbackList.end()) {
        fprintf(stderr, "Call function error: %s\n", func_name.c_str());
        return 0;
    }
    auto result = it->second(request_param);
    writeBlockToMemory(memory, result);
    return result.size();
}

void
DynamicFunctionRunner::executeCallback(FILE *wasms_out, FILE *wasms_in, std::vector<uint8_t>& return_buffer) const {
    std::vector<uint8_t> buf(8800);
    int ret = fread(buf.data(), 1, callbackNameSize, wasms_out);
    if (ret == 0) return;
    assert(ret == callbackNameSize);
    std::string func_name(reinterpret_cast<char *>(buf.data()), callbackNameSize);
    assert(fread(buf.data(), 4, 1, wasms_out) == 1);
    uint32_t block_size = *reinterpret_cast<uint32_t *>(buf.data());
    if (func_name == "DONE") {
        return_buffer.resize(block_size);
        assert(fread(return_buffer.data(), block_size, 1, wasms_out) == 1);
        return;
    }
    buf.resize(block_size);
    assert(fread(buf.data(), block_size, 1, wasms_out) == 1);
    auto it = m_callbackList.find(func_name);
    if (it == m_callbackList.end()) {
        fprintf(stderr, "Call function error: %s\n", func_name.c_str());
        return;
    }
    auto result = it->second(buf);

    block_size = result.size();
    assert(fwrite(&block_size, 4, 1, wasms_in) == 1);
    assert(fwrite(result.data(), block_size, 1, wasms_in) == 1);
    fflush(wasms_in);
}