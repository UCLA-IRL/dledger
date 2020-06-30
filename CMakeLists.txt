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
    ./src/config.cpp
    ./src/record_name.cpp
    ./src/record_name.hpp
    ./src/cert-list.cpp
    ./src/cert-list.h)

# include
include_directories(${NDN_CXX_INCLUDE_DIRS})

# link
link_directories(${NDN_CXX_LIBRARY_DIRS})

# target

add_library(dledger SHARED "")
target_sources(dledger PRIVATE ${DLEDGER_LIB_SOURCE_FILES})
target_include_directories(dledger PUBLIC ./include)
target_include_directories(dledger PRIVATE ./src)
target_compile_options(dledger PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(dledger PUBLIC ${NDN_CXX_LIBRARIES} leveldb)

add_executable(backend-test ./test/backend-test.cpp)
target_include_directories(backend-test PRIVATE ./src)
target_link_libraries(backend-test PUBLIC dledger)

add_executable(record-test ./test/record-test.cpp)
target_link_libraries(record-test PUBLIC dledger)

add_executable(ledger-impl-test ./test/ledger-impl-test.cpp)
target_link_libraries(ledger-impl-test PUBLIC dledger)

add_executable(ledger-impl-test-anchor ./test/ledger-impl-test-anchor.cpp)
target_link_libraries(ledger-impl-test-anchor PUBLIC dledger)

include(GNUInstallDirs)
install(TARGETS dledger
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

install(TARGETS ledger-impl-test
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS ledger-impl-test-anchor
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})