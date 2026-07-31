cmake_minimum_required(VERSION 3.10)
project(NamekFramework VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core/include /usr/include/lua5.3)

set(CORE_SOURCES
    core/src/namek_utils.cpp
    core/src/namek_crypto.cpp
    core/src/namek_db.cpp
    core/src/namek_cli.cpp
    core/src/namek_c_api.cpp
    core/src/namek_obfuscator.cpp
    core/src/namek_syntax.cpp
    core/src/namek_toolbox_suite.cpp
    core/src/namek_binary_format.cpp
    core/src/namek_packager.cpp
    core/src/namek_lua.cpp
    core/src/namek_mod_sandbox.cpp
    core/src/namek_mod_marketplace.cpp
)

# Shared and Static C/C++ Libraries for Node and Python FFI
add_library(namek_core SHARED ${CORE_SOURCES})
target_link_libraries(namek_core PRIVATE lua5.3)

add_library(namek_core_static STATIC ${CORE_SOURCES})

# Namek CLI Executable
add_executable(namek cli/main.cpp)
target_link_libraries(namek PRIVATE namek_core_static lua5.3 pthread)

# Set output directories
set_target_properties(namek_core PROPERTIES OUTPUT_NAME "namek")
set_target_properties(namek PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
