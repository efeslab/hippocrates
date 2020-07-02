include(ProcessorCount)
ProcessorCount(NPROC)

set(VALIDATE_PMDK_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk)

add_custom_command(OUTPUT ${VALIDATE_PMDK_ROOT_DIR}
                   COMMAND git clone https://github.com/pmem/pmdk 
                            ${VALIDATE_PMDK_ROOT_DIR}
                   COMMENT "PMDK validation repo cloned")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/RUNTESTS
                   COMMAND cp ${VALIDATE_PMDK_ROOT_DIR}/src/test/RUNTESTS 
                              ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS ${VALIDATE_PMDK_ROOT_DIR}
                   COMMENT "PMDK unit test scripts")

add_custom_target(VALIDATION_PMDK : # no-op, only if a dependency
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/RUNTESTS
                  COMMENT "PMDK validation repo set up.")           
configure_file(testconfig.sh ${CMAKE_CURRENT_BINARY_DIR})

add_pmdk_unit_test(TEST_CASE pmem_memset 
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)