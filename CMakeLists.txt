# options
set(USE_LUA_JIT false)

cmake_minimum_required(VERSION 3.28)

project(Tack-Engine LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(CMakePackageConfigHelpers)
include(CheckCXXCompilerFlag)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(ExternalProject)
include(FetchContent)
set(FETCHCONTENT_QUIET TRUE)

# sfml lib
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(SFML)

# lua (non-jit) lib
if(NOT USE_LUA_JIT)
    FetchContent_Declare(
        lua
        URL "https://www.lua.org/ftp/lua-5.4.8.tar.gz"
    )
    FetchContent_MakeAvailable(lua)
    file(GLOB LUA_SOURCES
        "${lua_SOURCE_DIR}/src/*.c"
    )
    list(REMOVE_ITEM LUA_SOURCES
        "${lua_SOURCE_DIR}/src/lua.c"
        "${lua_SOURCE_DIR}/src/luac.c"
    )
    add_library(lua STATIC
        ${LUA_SOURCES}
    )
    target_include_directories(lua PUBLIC
        "${lua_SOURCE_DIR}/src"
    )
endif()

# main src files
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.c"
    "external/sys_info/src/*.cpp"
)

add_executable(${PROJECT_NAME})

# compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS
        "/Ox /EHsc /MP"
    )
endif()

# target src files we specified in project sources
target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})

# target include files so you can see header defs
target_include_directories(${PROJECT_NAME} PRIVATE
    "src/"
    "external/sys_info/include"
)

# link against sfml
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Audio
)

# link against lua
if(NOT USE_LUA_JIT)
    target_link_libraries(${PROJECT_NAME} PRIVATE lua)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        "lua/jit_include"
    )
    target_link_directories(${PROJECT_NAME} PRIVATE
        "external/lua/lib"
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC
        USE_LUA_JIT
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        lua51
        luajit
        buildvm
        minilua
    )
endif()