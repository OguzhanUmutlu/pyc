cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(pyc LANGUAGES C CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(pyc
        src/main.cpp
        src/lexer.c
        src/parser.c
        src/compiler.cpp
        src/utils.c
        src/lexer.h
        src/parser.h
        src/compiler.h
        src/utils.h
        src/vec.h
)

llvm_map_components_to_libnames(llvm_libs
        core
        support
        irreader
        executionengine
        native
        orcjit
)

target_link_libraries(pyc m ${llvm_libs})
