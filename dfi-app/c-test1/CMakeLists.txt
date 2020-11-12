cmake_minimum_required(VERSION 3.6)
project(c-test1 C)
set(CMAKE_C_STANDARD 11)

if (EMSCRIPTEN)
    if(JS)
        set(CMAKE_EXECUTABLE_SUFFIX ".js")
    else()
        set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
    endif()
endif()


add_executable(c-test1 main.c)