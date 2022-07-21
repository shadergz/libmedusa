cmake_minimum_required(VERSION 3.13)

project(medusa LANGUAGES C VERSION 0.1.1)

set(ENABLE_TEST ON)

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_FLAGS "-Wall -Werror -fsanitize=address")

add_library(${CMAKE_PROJECT_NAME} medusalog.c)
add_library(${CMAKE_PROJECT_NAME}::${CMAKE_PROJECT_NAME} ALIAS ${CMAKE_PROJECT_NAME})

find_package(Threads)

target_link_libraries(${CMAKE_PROJECT_NAME} Threads::Threads)

set(NAMESPACE "${CMAKE_PROJECT_NAME}::")

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:>
)

if (ENABLE_TEST)
    add_subdirectory(test)
endif()
