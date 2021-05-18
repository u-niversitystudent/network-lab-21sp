cmake_minimum_required(VERSION 3.5)
project(prj_10 C)

set(CMAKE_C_STANDARD 99)

add_executable(prj_10 main.c trie.c fmt.c fread.c poptrie.c poptrie.h)

include_directories(include)