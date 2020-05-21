# cmake for third-party

option(JSON_BuildTests OFF)
add_subdirectory(json)

set(BUILD_SHARED_LIBS OFF)
option(BUILD_TESTS    OFF)
option(BUILD_EXAMPLES OFF)
add_subdirectory(json-schema-validator)
