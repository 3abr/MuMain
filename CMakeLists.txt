cmake_minimum_required(VERSION 3.25)

project(MuMain LANGUAGES CXX)

# Version compatibility options
option(ENABLE_V097_COMPAT "Enable MU Online v0.97 server compatibility" OFF)

if(ENABLE_V097_COMPAT)
  add_compile_definitions(ENABLE_V097_COMPAT=1)
  message(STATUS "Building with v0.97 compatibility enabled")
else()
  message(STATUS "Building with Season 6 compatibility (default)")
endif()

#Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("src")
