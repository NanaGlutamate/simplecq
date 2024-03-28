cmake_minimum_required(VERSION 3.12)
set(PROJ_NAME SIMPLE_CQ)
project(${PROJ_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(AFTER ${PROJECT_SOURCE_DIR}/src)
include_directories(AFTER ${PROJECT_SOURCE_DIR}/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

add_definitions(-D__PROJECT_ROOT_PATH="${PROJECT_SOURCE_DIR}")
# add_definitions(-D__ENABLE_PROFILE)

if(MSVC)
  string(APPEND CMAKE_CXX_FLAGS " /permissive- /Zc:__cplusplus /utf-8 ")
  add_definitions(-D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
endif()

# if(MSVC)
#   string(APPEND CMAKE_CXX_FLAGS " /MT ")
# else()
#   message(FATAL_ERROR "Only MSVC is supported")
# endif()

add_subdirectory(src)