cmake_minimum_required(VERSION 3.14)

IF(APPLE)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
endif()

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NDN_CXX REQUIRED libndn-cxx)

# include
include_directories(${NDN_CXX_INCLUDE_DIRS})

# link
link_directories(${NDN_CXX_LIBRARY_DIRS})

# wasmer include/lib
find_package(PkgConfig REQUIRED)
pkg_check_modules(WASMER REQUIRED wasmer)
include_directories(${WASMER_INCLUDE_DIRS})
link_directories(${WASMER_LIBRARY_DIRS})

add_executable(ledger-dfi dynamic-function-runner.cpp dfi-ledger.cpp)
target_compile_options(ledger-dfi PUBLIC ${NDN_CXX_CFLAGS} )
target_link_libraries(ledger-dfi PUBLIC ${NDN_CXX_LIBRARIES} dledger ${WASMER_LIBRARIES})

add_executable(dfi-test dynamic-function-runner.cpp dfi-test.cpp)
target_compile_options(dfi-test PUBLIC ${NDN_CXX_CFLAGS})
target_link_libraries(dfi-test PUBLIC ${NDN_CXX_LIBRARIES} ${WASMER_LIBRARIES})

if (BUILD_EXAMPLES)
    add_subdirectory(c-test1)
endif(BUILD_EXAMPLES)