cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core library with tiny-AES-C
add_library(nekoarchive STATIC
    src/archive.cpp
    src/compressor.cpp
    src/decompressor.cpp
    src/crypto/aes.c
)

target_include_directories(nekoarchive PUBLIC include)

# CLI executable
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)
