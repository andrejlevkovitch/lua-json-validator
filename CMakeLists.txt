# cmake

cmake_minimum_required(VERSION 3.12)

project(json_validator)

include(cmake/build.cmake)
add_subdirectory(third-party)


list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(LuaJit REQUIRED)


set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(${PROJECT_NAME} SHARED lua_json_validator.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)
target_include_directories(${PROJECT_NAME} PRIVATE ${LUAJIT_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${LUAJIT_LIBRARIES}
  nlohmann_json::nlohmann_json
  nlohmann_json_schema_validator
  )
