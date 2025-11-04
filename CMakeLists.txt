cmake_minimum_required(VERSION 3.28)

project(Tack-Engine LANGUAGES CXX C)

set(USE_LUA_JIT true)
# set(USE_RAYLIB_BACKEND true)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

if(USE_RAYLIB_BACKEND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
        GIT_TAG "master"
        GIT_PROGRESS TRUE
    )
    if(NOT raylib_POPULATED)
        FetchContent_MakeAvailable(raylib)
    endif()
else()
    # sfml
    include(CMakePackageConfigHelpers)
    include(CheckCXXCompilerFlag)
    include(CMakeDependentOption)
    include(GNUInstallDirs)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.1
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
    FetchContent_MakeAvailable(SFML)
endif()

if(NOT USE_LUA_JIT)
    file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
        "external/lua/src/*.c"
    )
endif()

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    ${PROJECT_SOURCES}
    "src/*.cpp"
    "src/*.c"
    "external/sys_info/src/*.cpp"
)

if(USE_RAYLIB_BACKEND)
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*renderer_sfml\\.cpp$")
else()
    list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*renderer_raylib\\.cpp$")
endif()

add_executable(${PROJECT_NAME})
set(CMAKE_CXX_FLAGS "/Ox /EHsc /MP")

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    "src/"
    "external/sys_info/include"
)

if(NOT USE_LUA_JIT)
    target_include_directories(${PROJECT_NAME} PRIVATE
        "external/lua/include"
    )
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        "external/lua/jit_include"
    )
endif()

if(USE_RAYLIB_BACKEND)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        raylib
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_RAYLIB_BACKEND)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SFML::Graphics
        SFML::Audio
    )
endif()
    
if(USE_LUA_JIT)
    target_link_directories(${PROJECT_NAME} PRIVATE
        "external/lua/lib"
    )
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_LUA_JIT)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        lua51
        luajit
        buildvm
        minilua
    )
endif()