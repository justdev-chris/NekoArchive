cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(zstd CONFIG REQUIRED)
find_package(liblzma REQUIRED)
find_package(xxHash CONFIG REQUIRED)

# Core library
add_library(nekoarchive STATIC
    src/archive.cpp
    src/compressor.cpp
    src/decompressor.cpp
    src/crypto/aes.c
)

# Force aes.c to be compiled as C
set_source_files_properties(src/crypto/aes.c PROPERTIES LANGUAGE C)

target_include_directories(nekoarchive PUBLIC include)

target_link_libraries(nekoarchive PRIVATE 
    zstd::libzstd
    LibLZMA::LibLZMA
    xxHash::xxhash
)

# CLI executable
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)
