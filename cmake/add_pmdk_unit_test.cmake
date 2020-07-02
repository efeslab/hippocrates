################################################################################
#
# This function's purpose in life is to add a unit test from PMDK.
#
# - Need to create a dependency chain so that they don't have any races.
#
################################################################################
include(ProcessorCount)
ProcessorCount(NPROC)

function(add_pmdk_unit_test)
    check_wllvm()

    set(options)                                                                  
    set(oneValueArgs TEST_CASE TEST_FILE PMDK_PATH PMDK_TARGET COMMIT_HASH)                                                       
    set(multiValueArgs SOURCES EXTRA_LIBS INCLUDE)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                        "${multiValueArgs}" ${ARGN})

    if (NOT TARGET ${FN_ARGS_PMDK_TARGET})
        message(FATAL_ERROR "PMDK_TARGET ${FN_ARGS_PMDK_TARGET} not a valid target!")
    endif()
    
    set(FN_ARGS_TARGET "${FN_ARGS_TEST_CASE}_${FN_ARGS_TEST_FILE}_${FN_ARGS_COMMIT_HASH}")
    if(TARGET ${FN_ARGS_TARGET})
        message(FATAL_ERROR "${FN_ARGS_TARGET} already a target!")
    endif()

    # 1. Checkout
    set(EXTRA_FLAGS "-g -O0")
    add_custom_target("${FN_ARGS_TARGET}_checkout"
                      COMMAND git checkout ${FN_ARGS_COMMIT_HASH}
                      COMMAND make CC=wllvm CXX=wllvm++
                              EXTRA_CFLAGS=${EXTRA_FLAGS}
                              EXTRA_CXXFLAGS=${EXTRA_FLAGS}
                              -j${NPROC} 
                      WORKING_DIRECTORY ${FN_ARGS_PMDK_PATH}
                      DEPENDS ${FN_ARGS_PMDK_TARGET}
                      COMMENT "")

    # 2. Build test
    set(TEST_ROOT "${FN_ARGS_PMDK_PATH}/src/test")
    set(TEST_PATH "${TEST_ROOT}/${FN_ARGS_TEST_CASE}")
    set(TOOL_PATH "${TEST_ROOT}/tools")
    # if(NOT EXISTS ${TEST_PATH} OR NOT EXISTS ${TOOL_PATH})
    #     message(FATAL_ERROR "${TEST_PATH} does not exist!")
    # endif()

    add_custom_target("${FN_ARGS_TARGET}_build"
                      COMMAND make CC=wllvm CXX=wllvm++
                              EXTRA_CFLAGS=${EXTRA_FLAGS}
                              EXTRA_CXXFLAGS=${EXTRA_FLAGS}
                              -j${NPROC} ${FN_ARGS_TEST_CASE} tools
                      WORKING_DIRECTORY ${TEST_ROOT}
                      DEPENDS "${FN_ARGS_TARGET}_checkout"
                      COMMENT "Building ${FN_ARGS_TEST_CASE}...")

    # 3. Copy test directory
    add_custom_target("${FN_ARGS_TARGET}_copy"
                      COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_TARGET}"
                      COMMAND cp -ruv ${TEST_PATH}/* 
                                "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_TARGET}"
                      DEPENDS "${FN_ARGS_TARGET}_build"
                      COMMENT "Copying files...")

    add_custom_target("${FN_ARGS_TARGET}_tooling"
                      COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/tools"
                      COMMAND cp -rnv ${TOOL_PATH}/* "${CMAKE_CURRENT_BINARY_DIR}/tools"
                      COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/unittest"
                      COMMAND cp -rnv ${TEST_ROOT}/unittest/* "${CMAKE_CURRENT_BINARY_DIR}/unittest"
                      DEPENDS "${FN_ARGS_TARGET}_build"
                      COMMENT "Copying tooling...")               
                      
    add_custom_target(${FN_ARGS_TARGET} ALL : # no-op
                      DEPENDS "${FN_ARGS_TARGET}_copy" "${FN_ARGS_TARGET}_tooling"
                      COMMENT "${FN_ARGS_TARGET} complete.")               
     
    append_tool_lists(TARGET ${FN_ARGS_TARGET} TOOL "PMDK_UNIT_TEST")

endfunction()