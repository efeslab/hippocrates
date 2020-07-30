################################################################################
#
# This includes all the appropriate function definitions and sets up some 
# shared functionality used by all the functions.
#
################################################################################

# For the verifier later.
set(TEST_TARGET_LIST "" CACHE INTERNAL "List of generated test target names")
set(TEST_EXE_LIST "" CACHE INTERNAL "List of all generated tests (executables)")
set(TEST_BC_LIST "" CACHE INTERNAL "List of all generated tests (bitcodes)")
set(TEST_TOOL_LIST "" CACHE INTERNAL "List of tools to use for each test")
set(TEST_SUITE_LIST "" CACHE INTERNAL "List of suites that each test belongs to")

function(append_tool_lists)
    set(options)                                                                   
    set(oneValueArgs TARGET TOOL SUITE EXECUTABLE)                                                       
    set(multiValueArgs)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                         "${multiValueArgs}" ${ARGN})

    if (NOT FN_ARGS_EXECUTABLE) 
        set(FN_ARGS_EXECUTABLE "${FN_ARGS_TARGET}")
    endif()

    if (FN_ARGS_TOOL STREQUAL "NONE")
        message(WARNING "${FN_ARGS_TARGET} tool set to NONE, not adding to validation script.")
    else()
        list(APPEND TEST_TARGET_LIST "${FN_ARGS_TARGET}")
        set(TEST_TARGET_LIST ${TEST_TARGET_LIST} CACHE INTERNAL "")

        list(APPEND TEST_EXE_LIST "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_EXECUTABLE}")
        set(TEST_EXE_LIST ${TEST_EXE_LIST} CACHE INTERNAL "")
        
        list(APPEND TEST_BC_LIST "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_EXECUTABLE}.bc")
        set(TEST_BC_LIST ${TEST_BC_LIST} CACHE INTERNAL "")
        
        list(APPEND TEST_TOOL_LIST "${FN_ARGS_TOOL}")
        set(TEST_TOOL_LIST ${TEST_TOOL_LIST} CACHE INTERNAL "")
        
        list(APPEND TEST_SUITE_LIST "${FN_ARGS_SUITE}")
        set(TEST_SUITE_LIST ${TEST_SUITE_LIST} CACHE INTERNAL "")
    endif()
endfunction()

function(check_wllvm)
    # Need to ensure that we're running with WLLVM as our compiler.
    # --https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#how-do-i-use-a-different-compiler
    string(FIND ${CMAKE_C_COMPILER} "wllvm" C_WLLVM)
    string(FIND ${CMAKE_CXX_COMPILER} "wllvm++" CXX_WLLVM)

    if (${C_WLLVM} EQUAL -1 OR ${CXX_WLLVM} EQUAL -1)
        message(FATAL_ERROR "Must set wllvm and wllvm++ as C and CXX compiler.\nCurrently ${C_WLLVM} and ${CXX_WLLVM}")
    endif()

    # Ensure environment variables are set.
    if (NOT DEFINED ENV{LLVM_COMPILER} OR NOT DEFINED ENV{LLVM_COMPILER_PATH})
        message(FATAL_ERROR "Must set LLVM_COMPILER and LLVM_COMPILER_PATH environment variables")
    endif()
endfunction()

set(FUNCTION_FILES "${CMAKE_SOURCE_DIR}/cmake/add_test_executable.cmake" 
                   "${CMAKE_SOURCE_DIR}/cmake/add_pmdk_unit_test.cmake")

foreach(F IN LISTS FUNCTION_FILES)
    if (NOT EXISTS ${F})
        message(FATAL_ERROR "CMake cannot find function file ${F}!")
    endif()
    include(${F})
endforeach()
