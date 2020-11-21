cmake_minimum_required(VERSION 3.6)
project(dfi-filter1 C)
set(CMAKE_C_STANDARD 11)

if (EMSCRIPTEN)
    if(JS)
        set(CMAKE_EXECUTABLE_SUFFIX ".js")
    else()
        set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
    endif()
endif()


add_executable(dfi-filter1 main.c)