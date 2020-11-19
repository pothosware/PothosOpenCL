########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosOpenCL CXX)

find_package(Pothos "0.6.0" CONFIG REQUIRED)

########################################################################
## Find OpenCl
########################################################################
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
find_package(OpenCL)

message(STATUS "OPENCL_FOUND: ${OPENCL_FOUND}")
message(STATUS "OPENCL_INCLUDE_DIRS: ${OPENCL_INCLUDE_DIRS}")
message(STATUS "OPENCL_LIBRARIES: ${OPENCL_LIBRARIES}")

########################################################################
# json.hpp header
########################################################################
find_path(JSON_HPP_INCLUDE_DIR NAMES json.hpp PATH_SUFFIXES nlohmann)

if (NOT JSON_HPP_INCLUDE_DIR)
    message(WARNING "Pothos OpenCL toolkit requires json.hpp, skipping...")
endif (NOT JSON_HPP_INCLUDE_DIR)

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_OPENCL "Enable Pothos OpenCL component" ON "Pothos_FOUND;OPENCL_FOUND;JSON_HPP_INCLUDE_DIR" OFF)
add_feature_info(OpenCL ENABLE_OPENCL "Support for OpenCL-based processing blocks")
if (NOT ENABLE_OPENCL)
    return()
endif()

########################################################################
## Build the module
########################################################################
include_directories(${OPENCL_INCLUDE_DIRS})
include_directories(${JSON_HPP_INCLUDE_DIR})

set(SOURCES
    OpenClInfo.cpp
    OpenClErrToStr.cpp
    OpenClContextCache.cpp
    OpenClKernel.cpp
    OpenClBufferManager.cpp
    OpenClConfLoader.cpp
    TestOpenClBlocks.cpp
)
set(DOC_SOURCES OpenClKernel.cpp)

POTHOS_MODULE_UTIL(
    TARGET OpenClBlocks
    SOURCES ${SOURCES}
    ENABLE_DOCS ON
    DOC_SOURCES ${DOC_SOURCES}
    LIBRARIES ${OPENCL_LIBRARIES}
    DESTINATION blocks/opencl
    ENABLE_DOCS
)
