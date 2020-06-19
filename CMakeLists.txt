cmake_minimum_required(VERSION 3.4.3)

include("${CMAKE_SOURCE_DIR}/cmake/add_test_executable.cmake")

# This will find the shared LLVM build.
find_package(LLVM 8.0 REQUIRED CONFIG)
# You don't need to change ${LLVM_CMAKE_DIR} since it is already defined.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# You don't need to change ${LLVM_DEFINITIONS} since it is already defined.
add_definitions(${LLVM_DEFINITIONS})
# You don't need to change ${LLVM_INCLUDE_DIRS} since it is already defined.
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(deps)

# -- This sets these variables so that they're available in the lower scope.
get_property(PMTEST_LIBS GLOBAL PROPERTY pmtest_lib_dir)
get_property(PMTEST_INCLUDE GLOBAL PROPERTY pmtest_include_dir)

get_property(YAMLCPP_LIBS GLOBAL PROPERTY yaml_cpp_lib_dir)
get_property(YAMLCPP_INCLUDE GLOBAL PROPERTY yaml_cpp_include_dir)

message("YAMLCPP: ${YAMLCPP_LIBS},${YAMLCPP_INCLUDE}")
add_subdirectory(src)

add_subdirectory(tests)
add_subdirectory(tools)