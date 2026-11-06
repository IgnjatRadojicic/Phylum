cmake_minimum_required(VERSION 3.20.4)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
# set(CMAKE_CXX_FLAGS_DEBUG "-fconcepts")
# set(CMAKE_CXX_FLAGS_MINSIZEREL "-fconcepts")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-fconcepts")
# set(CMAKE_CXX_FLAGS_RELEASE "-fconcepts")

project(recap_server VERSION 0.7.2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CPM package manager setup
include(cmake/get_cpm.cmake)

file(GLOB_RECURSE RECAP_HEADERS "${CMAKE_CURRENT_LIST_DIR}/source/*.h")
file(GLOB_RECURSE RECAP_SOURCES "${CMAKE_CURRENT_LIST_DIR}/source/*.cpp")

add_definitions(-D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
add_definitions(-DBOOST_NETWORK_ENABLE_HTTPS -DBOOST_ASIO_DISABLE_CONCEPTS)
add_definitions(-DRAPIDJSON_HAS_STDSTRING)
add_definitions(-DSOL_STRINGS_ARE_NUMBERS=1 -DSOL_ALL_SAFETIES_ON=1 -DSOL_SAFE_FUNCTION=1 -DSOL_EXCEPTIONS_SAFE_PROPAGATION=1)

add_executable(recap_server ${RECAP_HEADERS} ${RECAP_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RECAP_HEADERS} ${RECAP_SOURCES})
target_include_directories(recap_server PRIVATE source)
set_target_properties(recap_server PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

if(MSVC)
    target_compile_options(recap_server PRIVATE /bigobj)
    set_target_properties(recap_server PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE /INCREMENTAL:NO /Brepro")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Version info
include(cmake/version_info.cmake)
generate_version_header(
    OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/source/version.h"
    PREFIX "RECAP"
    MAJOR ${PROJECT_VERSION_MAJOR}
    MINOR ${PROJECT_VERSION_MINOR}
    PATCH ${PROJECT_VERSION_PATCH})
if(WIN32)
    get_target_property(TARGET_BINARY_DIR recap_server BINARY_DIR)
    set(version_output_file "${TARGET_BINARY_DIR}/version.rc")
    generate_version_resource(recap_server
        OUTPUT_FILE "${version_output_file}"
        NAME "Resurrection Capsule Server"
        FILENAME "recap_server.exe"
        MAJOR_VERSION ${PROJECT_VERSION_MAJOR}
        MINOR_VERSION ${PROJECT_VERSION_MINOR}
        PATCH_VERSION ${PROJECT_VERSION_PATCH})
    target_sources(recap_server PRIVATE ${version_output_file})
endif()

# Boost
set(BOOST_INCLUDE_LIBRARIES "asio;bind;regex;beast")
CPMAddPackage(
    NAME Boost
    VERSION 1.84.0
    URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
    URL_HASH SHA256=2e64e5d79a738d0fa6fb546c6e5c2bd28f88d268a2a080546f74e5ff98f29d0e
    OPTIONS "BOOST_ENABLE_CMAKE ON"
            "BOOST_RUNTIME_LINK shared")
target_link_libraries(recap_server PRIVATE boost_regex boost_asio boost_beast boost_bind)

# pugixml
CPMAddPackage("gh:zeux/pugixml@1.15")
target_link_libraries(recap_server PRIVATE pugixml)

# RapidJSON
CPMAddPackage(NAME rapidjson
    GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
    GIT_TAG ebd87cb468fb4cb060b37e579718c4a4125416c1
    OPTIONS "RAPIDJSON_BUILD_EXAMPLES OFF"
            "RAPIDJSON_BUILD_TESTS OFF"
            "RAPIDJSON_BUILD_DOC OFF")
target_include_directories(recap_server PRIVATE "${rapidjson_SOURCE_DIR}/include")

# OpenSSL
if(MSVC)
    # OpenSSL's Configure reads CC. The Ninja generator leaves a forward-slash path there,
    # which cmd treats as a drive change, so nmake "runs" the compiler without compiling anything.
    set(ENV{CC} cl)
endif()
CPMAddPackage(
    GIT_REPOSITORY https://github.com/jimmy-park/openssl-cmake.git
    GIT_TAG 9f0eb150763420789a38787554a839447ec5f88c
    OPTIONS "OPENSSL_TARGET_VERSION 1.1.1b"
            "OPENSSL_ENABLE_PARALLEL OFF"
            "OPENSSL_BUILD_VERBOSE ON"
            "OPENSSL_CONFIGURE_OPTIONS enable-ssl3\\\\;enable-ssl3-method\\\\;enable-weak-ssl-ciphers"
            "OPENSSL_CONFIGURE_VERBOSE ON")
target_link_libraries(recap_server PRIVATE OpenSSL::SSL OpenSSL::Crypto OpenSSL::applink)

# Lua
# The game's compiled scripts are Lua 5.1 bytecode, which only standard Lua 5.1 can run.
CPMAddPackage(
    NAME lua
    VERSION 5.1.5
    URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
    URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333
    DOWNLOAD_ONLY YES)
if(lua_ADDED)
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/src/*.c")
    list(REMOVE_ITEM LUA_SOURCES
        "${lua_SOURCE_DIR}/src/lua.c"
        "${lua_SOURCE_DIR}/src/luac.c"
        "${lua_SOURCE_DIR}/src/print.c")
    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua SYSTEM PUBLIC $<BUILD_INTERFACE:${lua_SOURCE_DIR}/src>)
    target_link_libraries(recap_server PRIVATE lua)
endif()

# Tests
CPMAddPackage(
    NAME doctest
    GITHUB_REPOSITORY doctest/doctest
    VERSION 2.4.11
    DOWNLOAD_ONLY YES)
add_executable(recap_tests
    tests/main.cpp
    tests/scriptsource_test.cpp
    tests/scriptgaps_test.cpp
    tests/projectilesweep_test.cpp
    tests/lobarc_test.cpp
    tests/attributestack_test.cpp
    tests/modifiermanager_test.cpp
    source/game/scriptsource.cpp
    source/game/scriptgaps.cpp
    source/game/projectilesweep.cpp
    source/game/lobarc.cpp
    source/game/attributestack.cpp
    source/game/modifiermanager.cpp)
target_include_directories(recap_tests PRIVATE source ${doctest_SOURCE_DIR})
target_link_libraries(recap_tests PRIVATE lua)

# Sol
CPMAddPackage("gh:ThePhD/sol2@3.3.1")
target_link_libraries(recap_server PRIVATE sol2)

# GLM
CPMAddPackage(
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 8b94f970d9666f77f4400103e31aaa4a79a7edef)
target_link_libraries(recap_server PRIVATE glm::glm)
target_link_libraries(recap_tests PRIVATE glm::glm)

# cpp-base64
CPMAddPackage(
    NAME cpp_base64
    GIT_REPOSITORY https://github.com/ReneNyffenegger/cpp-base64.git
    GIT_TAG V2.rc.08
    DOWNLOAD_ONLY YES)
if(cpp_base64_ADDED)
    FILE(GLOB cpp_base64_sources ${cpp_base64_SOURCE_DIR}/*.cpp)
    list(REMOVE_ITEM cpp_base64_sources "${cpp_base64_SOURCE_DIR}/test.cpp"
                                        "${cpp_base64_SOURCE_DIR}/test-google.cpp")
    add_library(cpp_base64 STATIC ${cpp_base64_sources})
    target_include_directories(cpp_base64 PUBLIC $<BUILD_INTERFACE:${cpp_base64_SOURCE_DIR}>)
    target_link_libraries(recap_server PRIVATE cpp_base64)
endif()

# lodepng
CPMAddPackage(
    NAME lodepng
    GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
    GIT_TAG 0b1d9ccfc2093e5d6620cd9a11d03ee6ff6705f5
    DOWNLOAD_ONLY YES)
if(lodepng_ADDED)
    FILE(GLOB lodepng_sources ${lodepng_SOURCE_DIR}/*.cpp)
    list(REMOVE_ITEM lodepng_sources "${lodepng_SOURCE_DIR}/lodepng_benchmark.cpp"
                                     "${lodepng_SOURCE_DIR}/lodepng_fuzzer.cpp"
                                     "${lodepng_SOURCE_DIR}/lodepng_unittest.cpp")
    add_library(lodepng STATIC ${lodepng_sources})
    target_include_directories(lodepng PUBLIC $<BUILD_INTERFACE:${lodepng_SOURCE_DIR}>)
    target_link_libraries(recap_server PRIVATE lodepng)
endif()

# RakNet
CPMAddPackage(
    NAME raknet
    VERSION 3.902
    URL https://web.archive.org/web/2020id_/http://www.jenkinssoftware.com/raknet/downloads/RakNet-3.902.zip
    URL_HASH SHA256=8c45d0c25a77ef095e060bcbbc01e198eb98a7a1f713fdf067bfe01a919e4274
    DOWNLOAD_ONLY YES)
if(raknet_ADDED)
    # Apply patches
    set(RAKNET_PATCH "${CMAKE_CURRENT_LIST_DIR}/cmake/patches/RakNet-3.902.patch")
    execute_process(COMMAND git init WORKING_DIRECTORY ${raknet_SOURCE_DIR})
    execute_process(COMMAND git apply --ignore-whitespace --verbose ${RAKNET_PATCH} WORKING_DIRECTORY ${raknet_SOURCE_DIR})
    set(RAKNET_BUILD_SHARED OFF)
    set(RAKNET_BUILD_STATIC ON)
    add_subdirectory(${raknet_SOURCE_DIR} ${raknet_BINARY_DIR})
    target_link_libraries(recap_server PRIVATE RakNetStatic)
endif()

include(GNUInstallDirs)
install(TARGETS recap_server
    EXPORT recap-targets
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT
        recap_server)

if (UNIX AND NOT APPLE)
    foreach(SIZE 16 32 48 64 128 256)
        install(FILES res/icon/recap_${SIZE}x${SIZE}.png
            DESTINATION share/icons/hicolor/${SIZE}x${SIZE}/apps
            RENAME com.resurrectioncapsule.server.png
            COMPONENT recap_server)
    endforeach()

    install(FILES res/com.resurrectioncapsule.server.desktop
        DESTINATION share/applications
        COMPONENT recap_server)
elseif(APPLE)
    install(TARGETS recap_server
        BUNDLE DESTINATION "Applications"
        COMPONENT recap_server)
endif()
