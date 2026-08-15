cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL only
find_package(OpenSSL REQUIRED)

# Core library
add_library(nekoarchive STATIC
    src/archive.cpp
    src/compressor.cpp
    src/decompressor.cpp
)

target_include_directories(nekoarchive PUBLIC include)
target_link_libraries(nekoarchive OpenSSL::Crypto)

# CLI executable (no GUI)
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)
