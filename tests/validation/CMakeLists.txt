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

# Issue: https://github.com/pmem/issues/issues/440
# Issue: https://github.com/pmem/issues/issues/441
# Issue: https://github.com/pmem/issues/issues/442
# Issue: https://github.com/pmem/issues/issues/444
# Issue: https://github.com/pmem/issues/issues/446
# Issue: https://github.com/pmem/issues/issues/447
# Issue: https://github.com/pmem/issues/issues/448
# Issue: https://github.com/pmem/issues/issues/449
# Issue: https://github.com/pmem/issues/issues/450
# Issue: https://github.com/pmem/issues/issues/452
# Issue: https://github.com/pmem/issues/issues/458
# Issue: https://github.com/pmem/issues/issues/459
# Issue: https://github.com/pmem/issues/issues/460
# Issue: https://github.com/pmem/issues/issues/461
# Issue: https://github.com/pmem/issues/issues/463
# Issue: https://github.com/pmem/issues/issues/465
# Issue: https://github.com/pmem/issues/issues/466
# Issue: https://github.com/pmem/issues/issues/535

# Issue: https://github.com/pmem/issues/issues/585
# add_pmdk_unit_test(TEST_CASE rpmemd_db
#                    TEST_FILE TEST0
#                    COMMIT_HASH 60e24d2 
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/940
add_pmdk_unit_test(TEST_CASE obj_first_next
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/942
add_pmdk_unit_test(TEST_CASE obj_mem
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/943
add_pmdk_unit_test(TEST_CASE obj_memops
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/945
add_pmdk_unit_test(TEST_CASE pmem_memset 
                   TEST_FILE TEST0
                   TEST_PATCH pmem_memset_TEST0_e71dfa41b.patch
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/949

# Issue: https://github.com/pmem/issues/issues/1103 
# add_pmdk_unit_test(TEST_CASE pmemobjcli 
#                    TEST_FILE TEST0
#                    COMMIT_HASH 2c2ba2e
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/1118
# add_pmdk_unit_test(TEST_CASE obj_pmalloc_mt
#                    TEST_FILE TEST1
#                    COMMIT_HASH 7ff604b
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)