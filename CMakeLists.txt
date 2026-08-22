cmake_minimum_required(VERSION 3.23)

project(KaleidoscopeLanguage)

add_executable(KaleidoscopeInterpreter)

target_sources(KaleidoscopeInterpreter
    PRIVATE
        src/lexer.cpp
)
