cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(tower-of-au)

# required since warp/opengl.h is leaking :/
# detect OS
if (WIN32)
    add_definitions(-DWINDOWS)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux") 
    add_definitions(-DLINUX)
# APPLE variable does not mean OS X
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin") 
    add_definitions(-DOSX)
endif()

# detect compiler
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic-errors -Wstrict-aliasing=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fstack-protector")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security -Wpointer-arith")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error-unused-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error-unused-parameter")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error-unused-function")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # using GCC
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic-errors -Wstrict-aliasing=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fstack-protector")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-security -Wpointer-arith")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    # using Visual Studio C++
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox")  # Full optimization 
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ot")  # Favor speed instead of size
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")  # Whole program optimization
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")  # Static runtime library
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GR-") # Disable RTTI

    # Disable exceptions:
    string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif()

# Get version:
exec_program (
    "git"
    ${CMAKE_CURRENT_SOURCE_DIR}
    ARGS "describe" "--tags" "--dirty" "--always"
    OUTPUT_VARIABLE VERSION
)
add_definitions( -DVERSION="${VERSION}" )

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR})
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_LIB_DIR libs)
set(ASSETS_DIRECTORY ${CMAKE_SOURCE_DIR}/assets)

if(DEFINED ENV{ASSET_DIR})
    set(ASSETS_DIRECTORY ${CMAKE_SOURCE_DIR}/$ENV{ASSET_DIR})
    message("### hey, hey! " ${ASSETS_DIRECTORY})
endif()

set(PARSON_SOURCES "${PROJECT_LIB_DIR}/parson/parson.c")
add_library(parson STATIC ${PARSON_SOURCES})

file(GLOB SOURCES *.cpp)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_INCLUDE_DIR})

enable_testing(true)
add_subdirectory(warp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} warp)
target_link_libraries(${PROJECT_NAME} parson)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

if(WIN32)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIRS})
    include_directories(${PROJECT_SOURCE_DIR}/warp/libs/SDL/include)
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${ASSETS_DIRECTORY} $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
