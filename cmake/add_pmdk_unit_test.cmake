
################################################################################
#
# This function's purpose in life is to add a unit test from PMDK.
#
# - Need to create a dependency chain so that they don't have any races.
#
################################################################################
include(ProcessorCount)
ProcessorCount(NPROC)

set(PMDK_UNIT_TEST_TARGETS "" CACHE INTERNAL "")

function(append_unit_test_list)
    set(options)                                                                   
    set(oneValueArgs TARGET)                                                       
    set(multiValueArgs)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                         "${multiValueArgs}" ${ARGN})

    list(APPEND PMDK_UNIT_TEST_TARGETS "${FN_ARGS_TARGET}")
    set(PMDK_UNIT_TEST_TARGETS ${PMDK_UNIT_TEST_TARGETS} CACHE INTERNAL "")
endfunction()

function(add_pmdk_unit_test)
    check_wllvm()

    # This is for some tests that need the pmempool exe or whatnot
    set(options)                                                                  
    set(oneValueArgs TEST_CASE TEST_FILE TEST_PATCH 
                     PMDK_PATH PMDK_TARGET COMMIT_HASH SUITE)                                                       
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
    set(EXTRA_FLAGS "-g -O0 -Wno-error -DUSE_VALGRIND=1 -I${PMCHK_INCLUDE_DIR}")
    add_custom_target("${FN_ARGS_TARGET}_checkout"
                      COMMAND git checkout ${FN_ARGS_COMMIT_HASH}
                      COMMAND git clean -fxd
                      COMMAND make CC=wllvm CXX=wllvm++ clean
                      COMMAND make CC=wllvm CXX=wllvm++
                              EXTRA_CFLAGS=${EXTRA_FLAGS}
                              EXTRA_CXXFLAGS=${EXTRA_FLAGS}
                              -j${NPROC} 
                      WORKING_DIRECTORY ${FN_ARGS_PMDK_PATH}
                      DEPENDS ${FN_ARGS_PMDK_TARGET} #${PMDK_UNIT_TEST_TARGETS}
                    #   DEPENDS ${FN_ARGS_PMDK_TARGET} ${PMDK_UNIT_TEST_TARGETS}
                      COMMENT "Checking out PMDK repo for test generation...")

    # 2. Build test
    set(TEST_ROOT "${FN_ARGS_PMDK_PATH}/src/test")
    set(LIB_ROOT "${FN_ARGS_PMDK_PATH}/src/debug")
    set(TEST_PATH "${TEST_ROOT}/${FN_ARGS_TEST_CASE}")
    set(TOOL_PATH "${TEST_ROOT}/tools")
    set(SRC_TOOLS "${FN_ARGS_PMDK_PATH}/src/tools")
    set(TARGET_BIN "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_TARGET}")
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

    # 3. Copy test directory and libs
    set(DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/${FN_ARGS_TARGET}")

    add_custom_target("${FN_ARGS_TARGET}_copy"
                    COMMAND mkdir -p "${DEST_DIR}"
                    COMMAND cp -ruv ${TEST_PATH}/* "${DEST_DIR}"
                    # Copy tool files first.
                    COMMAND cp -uv "${SRC_TOOLS}/pmempool/pmempool" "${DEST_DIR}"
                    COMMAND cp -uv "${TOOL_PATH}/pmemobjcli/pmemobjcli" "${DEST_DIR}"
                    COMMAND extract-bc "${DEST_DIR}/${FN_ARGS_TEST_CASE}"
                                -o "${DEST_DIR}/${FN_ARGS_TEST_CASE}.bc"
                    COMMAND cp -uv "${LIB_ROOT}/*.so" "${DEST_DIR}"
                    COMMAND cp -v "${TEST_ROOT}/match" "${CMAKE_CURRENT_BINARY_DIR}"
                    COMMAND ln -vf "${TARGET_BIN}/libpmem.so" "${TARGET_BIN}/libpmem.so.1"
                    COMMAND ln -vf "${TARGET_BIN}/libpmemobj.so" "${TARGET_BIN}/libpmemobj.so.1"
                    COMMAND ln -vf "${TARGET_BIN}/libpmempool.so" "${TARGET_BIN}/libpmempool.so.1"
                    COMMAND ln -vf "${TARGET_BIN}/libpmemblk.so" "${TARGET_BIN}/libpmemblk.so.1"
                    COMMAND ln -vf "${TARGET_BIN}/libpmemlog.so" "${TARGET_BIN}/libpmemlog.so.1"
                    COMMAND extract-bc -o ${TARGET_BIN}/libpmem.so.1.bc ${TARGET_BIN}/libpmem.so.1
                    COMMAND extract-bc -o ${TARGET_BIN}/libpmemobj.so.1.bc ${TARGET_BIN}/libpmemobj.so.1
                    COMMAND extract-bc -o ${TARGET_BIN}/libpmempool.so.1.bc ${TARGET_BIN}/libpmempool.so.1
                    COMMAND extract-bc -o ${TARGET_BIN}/libpmemblk.so.1.bc ${TARGET_BIN}/libpmemblk.so.1
                    COMMAND extract-bc -o ${TARGET_BIN}/libpmemlog.so.1.bc ${TARGET_BIN}/libpmemlog.so.1
                    COMMAND patchelf --set-rpath "${DEST_DIR}" "${DEST_DIR}/pmempool"
                    COMMAND patchelf --set-rpath "${DEST_DIR}" "${DEST_DIR}/${FN_ARGS_TEST_CASE}"
                    DEPENDS "${FN_ARGS_TARGET}_build"
                    COMMENT "Copying and extracting files...")
    
    # execute_process(COMMAND echo for f in ${TARGET_BIN}/*.so*; do extract-bc $f -o $f.bc; done)
    # message(FATAL_ERROR "argh")

    add_custom_target("${FN_ARGS_TARGET}_tooling"
                      COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/tools"
                      COMMAND cp -rnv ${TOOL_PATH}/* "${CMAKE_CURRENT_BINARY_DIR}/tools"
                      COMMAND mkdir -p "${CMAKE_CURRENT_BINARY_DIR}/unittest"
                      COMMAND cp -rnv ${TEST_ROOT}/unittest/* "${CMAKE_CURRENT_BINARY_DIR}/unittest"
                      DEPENDS "${FN_ARGS_TARGET}_build"
                      COMMENT "Copying tooling...")
    
    set(DEP_LIST "${FN_ARGS_TARGET}_copy" "${FN_ARGS_TARGET}_tooling")
    
    if (FN_ARGS_TEST_PATCH)
        # message(FATAL_ERROR "PATCH FILE: ${FN_ARGS_TEST_PATCH}")
        add_custom_target("${FN_ARGS_TARGET}_patching"
                          COMMAND patch -f -o "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}.patched" 
                                    "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/${FN_ARGS_TEST_PATCH}"
                          COMMAND cp "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}" 
                                      "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}.original"
                          COMMAND cp "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}.patched" 
                                     "${FN_ARGS_TARGET}/${FN_ARGS_TEST_FILE}"
                          DEPENDS "${FN_ARGS_TARGET}_tooling"
                          COMMENT "Patching with patch ${FN_ARGS_TEST_PATCH}...")
        list(APPEND DEP_LIST "${FN_ARGS_TARGET}_patching")
    endif()
                      
    add_custom_target(${FN_ARGS_TARGET} ALL : # no-op
                      DEPENDS ${DEP_LIST}
                      COMMENT "${FN_ARGS_TARGET} complete.")               
     
    append_tool_lists(TARGET "${FN_ARGS_TARGET}"
                      EXECUTABLE "${FN_ARGS_TARGET}/${FN_ARGS_TEST_CASE}" 
                      TOOL "PMDK_UNIT_TEST" 
                      SUITE "${FN_ARGS_SUITE}")
    append_unit_test_list(TARGET "${FN_ARGS_TARGET}")

endfunction()
