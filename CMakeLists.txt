cmake_minimum_required(VERSION 3.28)

project(Tack-Engine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# sfml
include(CMakePackageConfigHelpers)
include(CheckCXXCompilerFlag)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.c"
)

set(PROJECT_INCLUDE "src/")

add_executable(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE -Oz)

target_sources(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_INCLUDE}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Graphics
    SFML::Audio
)