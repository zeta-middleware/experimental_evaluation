cmake_minimum_required(VERSION 3.13.1)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(project_name)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND HEADERS
    "${PROJECT_SOURCE_DIR}/include")

list(APPEND SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/src/core.c"
	"${CMAKE_CURRENT_LIST_DIR}/src/board.c"
	"${CMAKE_CURRENT_LIST_DIR}/src/net.c"
	"${CMAKE_CURRENT_LIST_DIR}/src/storage.c")

include_directories(${HEADERS})
target_sources(app PRIVATE ${SOURCES})
