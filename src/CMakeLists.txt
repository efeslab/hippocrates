cmake_minimum_required(VERSION 3.4.3)
# This will find the shared LLVM build.
find_package(LLVM 8.0 REQUIRED CONFIG)
# You don't need to change ${LLVM_CMAKE_DIR} since it is already defined.
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# You don't need to change ${LLVM_DEFINITIONS} since it is already defined.
add_definitions(${LLVM_DEFINITIONS})
# You don't need to change ${LLVM_INCLUDE_DIRS} since it is already defined.
include_directories(${LLVM_INCLUDE_DIRS})
# Add the directory which your pass lives.
add_subdirectory(heuristic_pass)
add_subdirectory(000_test_pass)

# Add the directory which your pass lives.
add_subdirectory(tests)
