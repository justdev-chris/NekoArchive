cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt
find_package(Qt6 COMPONENTS Widgets REQUIRED)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
endif()

# Find compression libraries
find_package(zstd CONFIG REQUIRED)
find_package(liblzma CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(xxHash CONFIG REQUIRED)

# Core library
add_library(nekoarchive STATIC
    src/archive.cpp
    src/compressor.cpp
    src/decompressor.cpp
)

target_include_directories(nekoarchive PUBLIC include)
target_link_libraries(nekoarchive 
    zstd::libzstd 
    liblzma::liblzma 
    OpenSSL::Crypto
    xxHash::xxhash
)

# CLI
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)

# GUI
add_executable(nekogui gui/main.cpp gui/mainwindow.cpp)
target_link_libraries(nekogui nekoarchive Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    set_target_properties(nekogui PROPERTIES WIN32_EXECUTABLE TRUE)
endif()