# cmake

cmake_minimum_required(VERSION 3.12)

project(json_validator)

include(cmake/build.cmake)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(third-party)

find_package(Lua 5.1 REQUIRED)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(${PROJECT_NAME} SHARED lua_json_validator.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)
target_include_directories(${PROJECT_NAME} PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE
  libluajit-5.1.so
  nlohmann_json::nlohmann_json
  nlohmann_json_schema_validator
  )
