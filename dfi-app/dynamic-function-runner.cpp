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

#include <wasmer.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <poll.h>
#include <utility>
#include <unistd.h>
#include <csignal>

const DynamicFunctionRunner *currentRunner;
wasmer_memory_t *currentMemory;

// Function to print the most recent error string from Wasmer if we have them
void print_wasmer_error()
{
    int error_len = wasmer_last_error_length();
    char *error_str = (char *) malloc(error_len);
    wasmer_last_error_message(error_str, error_len);
    printf("Error: `%s`\n", error_str);
}

std::vector<uint8_t> DynamicFunctionRunner::getBlockFromMemory(wasmer_memory_t *memory, size_t size, size_t offset){
    auto mem_arr = wasmer_memory_data(memory);
    std::vector<uint8_t> buf;
    memcpy(buf.data(), reinterpret_cast<uint8_t *>(mem_arr + offset), size);
    return std::move(buf);
}
void DynamicFunctionRunner::writeBlockToMemory(wasmer_memory_t *memory, const std::vector<uint8_t>& block, size_t offset){
    auto mem_arr = wasmer_memory_data(memory);
    memcpy(mem_arr + offset, block.data(), block.size());
}
std::vector<uint8_t> DynamicFunctionRunner::fileToVec(const std::string& fileName) {
    // Read our input file, which in this case is a wat text file.
    FILE *file = fopen(fileName.c_str(), "r");
    assert(file != nullptr);
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);
    std::vector<uint8_t> vector(file_size);
    assert(fread(vector.data(), file_size, 1, file) == 1);
    fclose(file);
    return std::move(vector);
}

// Function to create a wasmer memory instance, so we can import
// memory into a wasmer instance.
wasmer_memory_t *DynamicFunctionRunner::create_wasmer_memory() {
    // Create our initial size of the memory
    wasmer_memory_t *memory = NULL;
    // Create our maximum memory size.
    // .has_some represents wether or not the memory has a maximum size
    // .some is the value of the maxiumum size
    wasmer_limit_option_t max = { .has_some = true,
            .some = 1 };
    // Create our memory descriptor, to set our minimum and maximum memory size
    // .min is the minimum size of the memory
    // .max is the maximuum size of the memory
    wasmer_limits_t descriptor = { .min = 1,
            .max = max };

    // Create our memory instance, using our memory and descriptor,
    wasmer_result_t memory_result = wasmer_memory_new(&memory, descriptor);
    // Ensure the memory was instantiated successfully.
    if (memory_result != wasmer_result_t::WASMER_OK)
    {
        print_wasmer_error();
    }

    // Return the Wasmer Memory Instance
    return memory;
}

/*
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
 */

/*
std::vector<uint8_t>
DynamicFunctionRunner::runWatModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wat text file.
    wasm_byte_vec_t wat;
    fileToVec(fileName, &wat);

    // Parse the wat into the binary wasm format
    wasm_byte_vec_t wasm;wasmer_
    wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &wasm);
    if (error != nullptr)
        exit_with_error("failed to parse wat", error, nullptr);
    wasm_byte_vec_delete(&wat);

    auto b = runWasmModule(&wasm, argument);
    wasm_byte_vec_delete(&wasm);
    return b;
}
*/

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(const std::string &fileName, const std::vector<uint8_t>& argument) const {
    // Read our input file, which in this case is a wasm file.
    auto binary = fileToVec(fileName);

    auto b = runWasmModule(binary, argument);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(const ndn::Block &block, const std::vector<uint8_t>& argument) const {
    // copy code to wasm byte vec
    std::vector<uint8_t> binary(block.value_size());
    memcpy(binary.data(), block.value(), block.value_size());

    auto b = runWasmModule(binary, argument);
    return b;
}

std::vector<uint8_t>
DynamicFunctionRunner::runWasmModule(std::vector<uint8_t>& binary, const std::vector<uint8_t>& argument) const {
    return run_module(binary, argument);
}

/*
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
DynamicFunctionRunner::runWasmPipeModule(const std::vector<uint8_t>& binary, const std::vector<uint8_t>& argument) const {
    wasm_module_t *module = this->compile(binary);
    return run_wasi_module(module, argument);
}
*/

wasmer_instance_t *
DynamicFunctionRunner::instantiate(std::vector<uint8_t>& module, wasmer_import_t *imports, size_t import_length) const{
    // Instantiate a WebAssembly Instance from Wasm bytes and imports
    wasmer_instance_t *instance = nullptr;
    wasmer_result_t compile_result = wasmer_instantiate(
            &instance, // Our reference to our Wasm instance
            module.data(), // The bytes of the WebAssembly modules
            module.size(), // The length of the bytes of the WebAssembly module
            imports, // The Imports array the will be used as our importObject
            import_length // The number of imports in the imports array
    );
    if (compile_result != wasmer_result_t::WASMER_OK)
    {
        print_wasmer_error();
    }
    // Assert the Wasm instantion completed
    assert(compile_result == wasmer_result_t::WASMER_OK);
    return instance;
}


wasmer_instance_t *
DynamicFunctionRunner::instantiate_wasi(std::vector<uint8_t>& binary) const
{
    wasmer_module_t *module = NULL;
    wasmer_result_t compile_result = wasmer_compile(&module, binary.data(), binary.size());
    if (compile_result != wasmer_result_t::WASMER_OK)
    {
        print_wasmer_error();
    }
    // Assert the Wasm instantion completed
    assert(compile_result == wasmer_result_t::WASMER_OK);

    // find out what version of WASI the module is
    Version wasi_version = wasmer_wasi_get_version(module);
    char* program = "dfi-program";
    wasmer_byte_array args[] = { { .bytes = reinterpret_cast<const uint8_t *>(program),
                                   .bytes_len = static_cast<uint32_t>(strlen(program)) } };
    wasmer_import_object_t * import_object = wasmer_wasi_generate_import_object_for_version(
            static_cast<unsigned char>(wasi_version), args, 1, nullptr, 0, nullptr,
            0, NULL, 0);

    // Instantiate a WebAssembly Instance from Wasm bytes and imports
    wasmer_instance_t *instance = nullptr;
    wasmer_result_t instantiate_result = wasmer_module_import_instantiate(
            &instance, // Our reference to our Wasm instance
            module, // The compiled module
            import_object // The Imports array the will be used as our importObject
    );
    if (instantiate_result != wasmer_result_t::WASMER_OK)
    {
        print_wasmer_error();
    }
    // Assert the Wasm instantiation completed
    assert(instantiate_result == wasmer_result_t::WASMER_OK);
    return instance;
}
/*
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
*/

std::vector<uint8_t>
DynamicFunctionRunner::run_module(std::vector<uint8_t>& module,const std::vector<uint8_t>& argument) const
{
  //make imports
  wasmer_memory_t *memory = create_wasmer_memory();
  currentRunner = this;
  currentMemory = memory;


  std::string module_name = "env";
  wasmer_byte_array module_name_bytes = { .bytes = reinterpret_cast<const uint8_t *>(module_name.data()),
                                          .bytes_len = static_cast<uint32_t>(module_name.size()) };

  std::string import_memory_name = "memory";
  wasmer_byte_array import_memory_name_bytes = { .bytes = reinterpret_cast<const uint8_t *>(import_memory_name.data()),
                                                 .bytes_len = static_cast<uint32_t>(import_memory_name.size()) };

  // Set the memory to our import object
  wasmer_import_t memory_import = { .module_name = module_name_bytes,
            .import_name = import_memory_name_bytes,
            .tag = wasmer_import_export_kind::WASM_MEMORY };
  memory_import.value.memory = memory;

  // Define our function import
  wasmer_value_tag param_format[1] = {wasmer_value_tag::WASM_I32};
  wasmer_import_func_t *func = wasmer_import_func_new(
            (void (*)(void *))(uint32_t (*)(wasmer_instance_context_t *, uint32_t))[](wasmer_instance_context_t *ctx, uint32_t in_len) -> uint32_t {
                return currentRunner->executeCallback(in_len, currentMemory);
                },
            param_format, 1,
            param_format, 1
            );
  std::string import_function_name = "callback";
  wasmer_byte_array import_function_name_bytes = { .bytes = reinterpret_cast<const uint8_t *>(import_function_name.data()),
            .bytes_len = static_cast<uint32_t>(import_function_name.size()) };
  wasmer_import_t callback_import = { .module_name = module_name_bytes,
            .import_name = import_function_name_bytes,
            .tag = wasmer_import_export_kind::WASM_FUNCTION,
            .value.func = func };

  // Define an array containing our imports
  wasmer_import_t imports[2] = {memory_import, callback_import};

  wasmer_instance_t *instance = instantiate(module, imports, 2);

  //prep argument
  memcpy(wasmer_memory_data(memory), argument.data(), argument.size());
  wasmer_value_t arg_val = {.tag=wasmer_value_tag::WASM_I32, .value.I32=static_cast<int32_t>(argument.size())};
  wasmer_value_t ret_val = {.tag=wasmer_value_tag::WASM_I32, .value.I32=0};

  // Call the Wasm function
  wasmer_result_t call_result = wasmer_instance_call(
          instance, // Our Wasm Instance
          "run", // the name of the exported function we want to call on the guest Wasm module
          &arg_val, // Our array of parameters
          1, // The number of parameters
          &ret_val, // Our array of results
          1 // The number of results
  );
  if (call_result != wasmer_result_t::WASMER_OK) {
      print_wasmer_error();
  }
  // Assert the Wasm instantion completed
  assert(call_result == wasmer_result_t::WASMER_OK);

  //cleanup
  std::vector<uint8_t> return_block = getBlockFromMemory(memory, static_cast<size_t>(ret_val.value.I32));
  wasmer_memory_destroy(memory);
  wasmer_instance_destroy(instance);
  return std::move(return_block);
}

void
DynamicFunctionRunner::setCallback(std::string name, std::function<std::vector<uint8_t>(std::vector<uint8_t>)> func){
    assert(name.length() == callbackNameSize);
    m_callbackList[name] = std::move(func);
}

int
DynamicFunctionRunner::executeCallback(int len, wasmer_memory_t *memory) const {
    std::string func_name((char *)wasmer_memory_data(memory), callbackNameSize);
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