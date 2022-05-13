cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(3dstocode)

set(LOADER_NAME  3dstocode)
set(LOADER_SOURCES src.cpp)
set(LOADER_OUTPUT_NAME       3dstocode-output)

include($ENV{IDASDK}/ida-cmake/addons.cmake)

set_source_files_properties(${DISABLED_SOURCES} PROPERTIES LANGUAGE "")
