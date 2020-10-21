cmake_minimum_required(VERSION 3.14)

# download wasm
if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api)
    if (APPLE)
        # download
        file(DOWNLOAD
                https://github.com/bytecodealliance/wasmtime/releases/download/v0.20.0/wasmtime-v0.20.0-x86_64-macos-c-api.tar.xz
                ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api.tar.xz)
        execute_process(COMMAND tar -xJvf -
                 INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api.tar.xz
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        file(RENAME wasmtime-v0.20.0-x86_64-macos-c-api wasmtime-c-api)
    elseif(UNIX)
        # linux download
        file(DOWNLOAD
                https://github.com/bytecodealliance/wasmtime/releases/download/v0.20.0/wasmtime-v0.20.0-x86_64-linux-c-api.tar.xz
                ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api.tar.xz)
        execute_process(COMMAND tar -xJvf -
                INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api.tar.xz
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        file(RENAME wasmtime-v0.20.0-x86_64-linux-c-api wasmtime-c-api)
    endif()
    file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/wasmtime-c-api.tar.xz)
endif()

set(CMAKE_CXX_STANDARD 14)

include_directories(wasmtime-c-api/include)
link_directories(wasmtime-c-api/lib)

add_executable(ledger-dfi ./wasi.c)
target_link_libraries(ledger-dfi PUBLIC wasmtime)