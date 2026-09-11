cmake_minimum_required(VERSION 3.23)

project(KaleidoscopeLanguage)

find_package(LLVM REQUIRED)

add_executable(KaleidoscopeInterpreter)

target_sources(KaleidoscopeInterpreter
        PRIVATE
        src/lexer.hpp
        src/lexer.cpp
        src/ast.hpp
        src/ast.cpp
        src/parser.hpp
        src/parser.cpp
        src/main.cpp
        src/jit.hpp
        src/jit.cpp
)

set_property(TARGET KaleidoscopeInterpreter PROPERTY CXX_STANDARD 23)

target_include_directories(KaleidoscopeInterpreter PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(KaleidoscopeInterpreter PRIVATE LLVM)

set(CMAKE_COLOR_DIAGNOSTICS On)
