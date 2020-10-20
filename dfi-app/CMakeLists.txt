if (NOT EXISTS wasmtime_c_api)
    execute_process(COMMAND bash ./install_wasmtime_c_api.sh
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

include_directories(wasmtime-c-api/include)
link_directories(wasmtime-c-api/lib)

add_executable(ledger-dfi ./wasi.c)
target_link_libraries(ledger-dfi PUBLIC dledger)
target_link_libraries(ledger-dfi PUBLIC wasmtime)