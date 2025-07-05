cmake_minimum_required(VERSION 3.22)
project(pyc C)

set(CMAKE_C_STANDARD 11)

add_executable(pyc src/main.c
        src/lexer.c
        src/lexer.h
        src/utils.h
        src/parser.c
        src/parser.h
        src/vec.h
        src/compiler.c
        src/compiler.h
        src/utils.c
)

target_link_libraries(pyc m gmp gmpxx)