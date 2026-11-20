cmake_minimum_required(VERSION 3.23)

project(btree C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(repl)

add_library(btree)

target_sources(repl
  PRIVATE
    src/repl.c
    src/btree.c
)

target_sources(btree
  PRIVATE
    src/btree.c
  PUBLIC
    FILE_SET HEADERS
    FILES
      src/btree.h
)
