cmake_minimum_required(VERSION 3.16)
project(my_http_server VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

# Find dependencies
find_package(ZLIB REQUIRED)

# Include directories
include_directories(
  ${PROJECT_SOURCE_DIR}/include
)

# Sources
set(SERVER_SOURCES
  src/main.cpp
  src/server.cpp
  src/request.cpp
  src/response.cpp
  src/router.cpp
  src/compression/registry.cpp
  src/compression/gzip.cpp
  src/utils/file_utils.cpp
  src/utils/path_validation.cpp
)

# Executable
add_executable(server ${SERVER_SOURCES})

target_link_libraries(server
  PRIVATE
    ZLIB::ZLIB
    pthread
)

# Tests (optional)
# enable_testing()
# add_subdirectory(tests)
