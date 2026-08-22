cmake_minimum_required(VERSION 3.23)

project(KaleidoscopeLanguage)

add_executable(KaleidoscopeInterpreter)

target_sources(KaleidoscopeInterpreter
    PRIVATE
        src/lexer.hpp
        src/lexer.cpp
        src/ast.hpp
        src/parser.cpp
)

set_property(TARGET KaleidoscopeInterpreter PROPERTY CXX_STANDARD 23)
