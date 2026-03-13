cmake_minimum_required(VERSION 3.22)
project(TPF_ecs
        VERSION "1.0.0"
        LANGUAGES C
        DESCRIPTION "TPF Entity-Component-System")

# CMake configuration
option(TPF_ENABLE_TESTS "Enable tests for TPF modules" ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(PROJECT_IS_TOP_LEVEL)
  # CMake internals
  include(vendor/TPF_cmake/testing.cmake)
  include(vendor/TPF_cmake/memcheck.cmake)
  if(TPF_ENABLE_TESTS)
    enable_testing()
  endif()

  # System packages
  find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)

  # Vendor dependencies
  add_subdirectory(vendor)
endif()

# This module
add_library(TPF_sparse_array OBJECT)
target_sources(TPF_sparse_array PRIVATE sparse_array.c)
target_link_libraries(TPF_sparse_array PRIVATE SDL3)

add_library(TPF_dynamic_array OBJECT)
target_sources(TPF_dynamic_array PRIVATE dynamic_array.c)
target_link_libraries(TPF_dynamic_array PRIVATE SDL3)

# add_library(TPF_component_table OBJECT)
# target_sources(TPF_component_table PRIVATE component_table.c)
# target_link_libraries(TPF_component_table PRIVATE SDL3)
# target_include_directories(TPF_component_table PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# add_library(TPF_ecs STATIC)
# target_sources(TPF_ecs PRIVATE ecs.c)
# target_link_libraries(TPF_ecs PRIVATE SDL3 TPF_arena TPF_sparse_array TPF_dynamic_array)
# target_include_directories(TPF_ecs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# target_include_directories(TPF_ecs INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Unit testing
if(TPF_ENABLE_TESTS)
  setup_test(NAME sparse_array LIBS SDL3::SDL3 TPF_sparse_array)
  add_memcheck_target(sparse_array_test)

  setup_test(NAME dynamic_array LIBS SDL3::SDL3 TPF_dynamic_array)
  add_memcheck_target(dynamic_array_test)

  setup_test(NAME component_table LIBS SDL3::SDL3 TPF_component_table)
  add_memcheck_target(component_table_test)

  # setup_test(NAME ecs LIBS SDL3::SDL3 TPF_ecs)
  # add_memcheck_target(ecs_test)
endif()
