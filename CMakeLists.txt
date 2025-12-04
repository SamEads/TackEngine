cmake_minimum_required(VERSION 3.28)

project(Tack-Engine LANGUAGES CXX C)

set(USE_LUA_JIT true)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

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
    SYSTEM
)
FetchContent_MakeAvailable(SFML)

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

add_executable(${PROJECT_NAME})

if(MSVC)
    set(CMAKE_CXX_FLAGS "/Ox /EHsc /MP")
endif()

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

target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Audio
)

#add_custom_command(
#    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_scripts.h
#    COMMAND ${CMAKE_COMMAND} 
#        -DSCRIPTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets/scripts
#        -DOUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_scripts.h
#        -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_scripts.cmake
#    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/embed_scripts.cmake
#    COMMENT "Generating embedded_scripts.h"
#    VERBATIM
#)
#add_custom_target(generate_embedded_scripts DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_scripts.h)
#add_dependencies(${PROJECT_NAME} generate_embedded_scripts)
target_compile_definitions(${PROJECT_NAME} PRIVATE EMBED_SCRIPTS)
    
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