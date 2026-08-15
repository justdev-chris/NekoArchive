cmake_minimum_required(VERSION 3.16)
project(NekoArchive VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ============================================
# FIX 1: Force Release build type (if not specified)
# ============================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# ============================================
# FIX 2: Static linking of C++ runtime (Windows)
# ============================================
if(MSVC)
    # Use /MT (static release) instead of /MD (dynamic)
    # This embeds the runtime in your EXE
    foreach(flag_var 
        CMAKE_CXX_FLAGS 
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
    
    # For GUI apps, also link statically
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
    
    # Remove debug DLL dependencies even in Debug mode
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "Runtime library" FORCE)
    
elseif(MINGW)
    # For GCC/MinGW, use static linking
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# ============================================
# FIX 3: Skip ALL Qt Tools (Core, Gui, Widgets)
# ============================================
set(Qt6CoreTools_FOUND TRUE CACHE BOOL "Skip Tools" FORCE)
set(Qt6GuiTools_FOUND TRUE CACHE BOOL "Skip Tools" FORCE)
set(Qt6WidgetsTools_FOUND TRUE CACHE BOOL "Skip Tools" FORCE)

# ============================================
# FIX 4: Find Qt (simplified, no Tools)
# ============================================
find_package(Qt6 COMPONENTS Widgets REQUIRED)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
endif()

# Force Qt to use release libraries (not debug)
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    # Qt will automatically link release versions
    # But we need to make sure we don't accidentally use debug
    set(QT_USE_DEBUG_LIBRARIES FALSE)
endif()

# ============================================
# FIX 5: Find compression libraries
# ============================================
find_package(zstd CONFIG REQUIRED)
find_package(liblzma CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(xxHash CONFIG REQUIRED)

# ============================================
# Core library
# ============================================
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

# Make sure static lib doesn't get debug runtime
if(MSVC)
    target_compile_options(nekoarchive PRIVATE /MT)
endif()

# ============================================
# CLI
# ============================================
add_executable(nekocli src/main.cpp)
target_link_libraries(nekocli nekoarchive)

# ============================================
# GUI
# ============================================
add_executable(nekogui gui/main.cpp gui/mainwindow.cpp)
target_link_libraries(nekogui nekoarchive Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    set_target_properties(nekogui PROPERTIES WIN32_EXECUTABLE TRUE)
    
    # ============================================
    # FIX 6: Copy Qt and other DLLs to output folder
    # ============================================
    add_custom_command(TARGET nekogui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying dependencies..."
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Widgets>
            $<TARGET_FILE_DIR:nekogui>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:Qt${QT_VERSION_MAJOR}::Core>
            $<TARGET_FILE_DIR:nekogui>
        COMMENT "Copying Qt DLLs to output folder"
    )
endif()

# ============================================
# Optional: Bundle all dependencies (Windows)
# ============================================
if(WIN32 AND MSVC)
    # Create a packaging step using windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt${QT_VERSION_MAJOR})
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET nekogui POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
                --release
                --no-compiler-runtime
                --no-translations
                --no-system-d3d-compiler
                $<TARGET_FILE:nekogui>
            COMMENT "Deploying Qt dependencies with windeployqt"
        )
    endif()
endif()

# ============================================
# FIX 7: Generate PDB for debugging (optional)
# ============================================
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        # Keep debug symbols but in a separate PDB file
        # (doesn't affect runtime DLL dependencies)
        set_target_properties(nekogui PROPERTIES
            COMPILE_PDB_NAME nekogui
            COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}/pdb
        )
    endif()
endif()