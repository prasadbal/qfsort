cmake_minimum_required(VERSION 3.16)
project(symqfilesort
		VERSION 1.0
		LANGUAGES CXX)
		
#include(${CMAKE_TOOLCHAIN_FILE})

set(CMAKE_CXX_STANDARD "20")
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)
SET(CMAKE_C_FLAGS "-fPIC -Wno-deprecated -Wall")
set(CXXFLAGS "${CMAKE_CXX_FLAGS} -g -O2")

message(STATUS "CXXFLAGS:" ${CXXFLAGS})

set(PROJ_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJ_INCLUDE_DIR})
add_subdirectory(src)
