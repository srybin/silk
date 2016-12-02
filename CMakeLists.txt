# ====================================
# Task Parallel Framework build script
# ====================================

cmake_minimum_required(VERSION 2.8)

project(tpf)

if( "${CMAKE_BUILD_TYPE}" STREQUAL "" OR NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
    set(CMAKE_BUILD_TYPE Release)
endif()

set( MAIN_DIRECTORY ${CMAKE_SOURCE_DIR} )
set( RESULT_DIRECTORY ${MAIN_DIRECTORY}/outputs)
set( MAIN_LIB_DIR ${MAIN_DIRECTORY}/lib )

set( SOURCES_DIRECTORY ${MAIN_DIRECTORY}/src )

set(GENERIC_COMPILER_FLAGS "-Wall -Wextra -Wno-unused-local-typedefs -Wno-unused-variable -Wno-sign-compare -Wfatal-errors")

set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" )
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${GENERIC_COMPILER_FLAGS}" )

include_directories(
        "${SOURCES_DIRECTORY}"
        "${MAIN_DIRECTORY}/include")

add_subdirectory( src )