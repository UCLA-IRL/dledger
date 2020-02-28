# cmake version to be used
cmake_minimum_required(VERSION 3.5)

# project name
project(dledger)

# flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)
find_package(leveldb REQUIRED)

# files
set(DLEDGER_LIB_SOURCE_FILES
    ./src/backend.hpp
    ./src/backend.cpp
    ./src/ledger-impl.hpp
    ./src/ledger-impl.cpp
    ./src/record.cpp
    )

# include
include_directories(./include ./src)
include_directories(${NDN_CXX_INCLUDE_DIRS})

# link
link_directories(${NDN_CXX_LIBRARY_DIRS})

# target
add_executable(backend-test ./test/backend-test.cpp ${DLEDGER_LIB_SOURCE_FILES})
target_compile_options(backend-test PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(backend-test PUBLIC ${NDN_CXX_LIBRARIES} leveldb)

add_executable(record-test ./test/record-test.cpp ${DLEDGER_LIB_SOURCE_FILES})
target_compile_options(record-test PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(record-test PUBLIC ${NDN_CXX_LIBRARIES} leveldb)

add_library(dledger SHARED ${DLEDGER_LIB_SOURCE_FILES})
target_compile_options(dledger PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(dledger PUBLIC ${NDN_CXX_LIBRARIES} leveldb)