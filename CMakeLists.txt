cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Find OpenSSL only (others optional for now)
find_package(OpenSSL REQUIRED)

# Core library
add_library(nekoarchive STATIC
    src/archive.cpp
    src/compressor.cpp
    src/decompressor.cpp
)

target_include_directories(nekoarchive PUBLIC include)
target_link_libraries(nekoarchive OpenSSL::Crypto)

# CLI
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)

# GUI
add_executable(nekogui gui/main.cpp gui/mainwindow.cpp)
target_link_libraries(nekogui nekoarchive Qt6::Widgets)

if(WIN32)
    set_target_properties(nekogui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
