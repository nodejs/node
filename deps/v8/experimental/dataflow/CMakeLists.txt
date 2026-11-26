# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.15)
project(cpp-dataflow-analyzer C CXX)

# Default to Release build type to ensure maximum performance by default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable RTTI to match LLVM/Clang
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")


# Hint CMake where to find LLVM/Clang
if(DEFINED ENV{LLVM_DIR} AND EXISTS "$ENV{LLVM_DIR}")
    set(LLVM_DIR "$ENV{LLVM_DIR}")
    get_filename_component(_llvm_lib_parent "${LLVM_DIR}/.." ABSOLUTE)
    if(EXISTS "${_llvm_lib_parent}/clang")
        set(Clang_DIR "${_llvm_lib_parent}/clang")
    endif()
elseif(EXISTS "$ENV{HOME}/external/llvm/install")
    set(LLVM_DIR "$ENV{HOME}/external/llvm/install/lib/cmake/llvm")
    set(Clang_DIR "$ENV{HOME}/external/llvm/install/lib/cmake/clang")
elseif(EXISTS "/usr/lib/llvm-19/lib/cmake/llvm")
    set(LLVM_DIR "/usr/lib/llvm-19/lib/cmake/llvm")
    set(Clang_DIR "/usr/lib/llvm-19/lib/cmake/clang")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(SQLite3 REQUIRED)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_definitions(${LLVM_DEFINITIONS})

add_executable(analyzer
    src/analyzer.cc
    src/csr_index.cc
    src/database.cc
    src/dfg.cc
    src/dfg_visitor.cc
    src/index.cc
    src/query.cc
    src/server.cc
    src/wrapper_config.cc
)

# LLVM and Clang libraries needed for Tooling and AST traversal
target_link_libraries(analyzer PRIVATE
    clangAST
    clangASTMatchers
    clangBasic
    clangFrontend
    clangIndex
    clangLex
    clangSerialization
    clangTooling
    LLVMSupport
    LLVMCore
)

# Clang/LLVM requires some system libraries
target_link_libraries(analyzer PRIVATE
    pthread
    z
    tinfo
    dl
    SQLite::SQLite3
)

# Create symlink for Clang resource headers to enable standalone execution
get_filename_component(LLVM_LIB_DIR "${LLVM_DIR}/../.." ABSOLUTE)
add_custom_command(TARGET analyzer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${LLVM_LIB_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/lib"
    COMMENT "Creating symlink for Clang resource headers"
)
