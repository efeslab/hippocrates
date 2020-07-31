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

################################################################################
# ALL ISSUES AND TESTS
################################################################################

# Issue: https://github.com/pmem/issues/issues/440
# obj_list_recovery/TEST0, TEST1, TEST2
# Status: Need to copy tool.
#add_pmdk_unit_test(TEST_CASE obj_list_recovery
#                   TEST_FILE TEST0
#                   COMMIT_HASH 8bbb0af9c
#                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                   PMDK_TARGET VALIDATION_PMDK)

#add_pmdk_unit_test(TEST_CASE obj_list_recovery
#                   TEST_FILE TEST1
#                   COMMIT_HASH 8bbb0af9c
#                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                   PMDK_TARGET VALIDATION_PMDK)

#add_pmdk_unit_test(TEST_CASE obj_list_recovery
#                   TEST_FILE TEST2
#                   COMMIT_HASH 8bbb0af9c
#                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                   PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/441
# obj_locks/TEST0
# Status: Passing? NOT REPRO
add_pmdk_unit_test(TEST_CASE obj_locks
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE NO_REPRO)

# Issue: https://github.com/pmem/issues/issues/442
# obj_persist_count/TEST0, TEST1
# Status: NOT REPRO (wants non-pmem?)
add_pmdk_unit_test(TEST_CASE obj_persist_count
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE NO_REPRO)

add_pmdk_unit_test(TEST_CASE obj_persist_count
                   TEST_FILE TEST1
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE NO_REPRO)

# Issue: https://github.com/pmem/issues/issues/444
# obj_pvector/TEST0
# Status: 
add_pmdk_unit_test(TEST_CASE obj_pvector
                   TEST_FILE TEST0
                   COMMIT_HASpH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/446
# obj_redo_log/TEST0, TEST1
add_pmdk_unit_test(TEST_CASE obj_redo_log
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

add_pmdk_unit_test(TEST_CASE obj_redo_log
                   TEST_FILE TEST1
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/447
# obj_toid/TEST0
# Status: NOT REPRO
# orig: 0fd509d73
# 8bbb0af9c6fc96f1aa9fc17321d0896a74e15d34
add_pmdk_unit_test(TEST_CASE obj_toid
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/448
# obj_tx_alloc/TEST0
add_pmdk_unit_test(TEST_CASE obj_tx_alloc
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/449
# obj_tx_add_range/TEST0
add_pmdk_unit_test(TEST_CASE obj_tx_add_range
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/450
# obj_tx_add_range_direct/TEST0
add_pmdk_unit_test(TEST_CASE obj_tx_add_range_direct
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/452
# obj_constructor/TEST0, TEST2
add_pmdk_unit_test(TEST_CASE obj_constructor
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

add_pmdk_unit_test(TEST_CASE obj_constructor
                   TEST_FILE TEST2
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/458
# pmemspoil/TEST0
add_pmdk_unit_test(TEST_CASE pmemspoil
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/459
# pmem_memcpy/TEST0, TEST1, TEST2, TEST3 
# Status: PASSING
add_pmdk_unit_test(TEST_CASE pmem_memcpy
                   TEST_FILE TEST0
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memcpy
                   TEST_FILE TEST1
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memcpy
                   TEST_FILE TEST2
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memcpy
                   TEST_FILE TEST3
                   COMMIT_HASH 8bbb0af9c
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)           

# Issue: https://github.com/pmem/issues/issues/460
# pmem_memmove/TEST0, TEST1, TEST2, TEST3, TEST4, TEST5, 
#              TEST6, TEST7, TEST8, TEST9, TEST10, TEST11
# Status: REPRO'D
add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST0
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST1
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST2
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST3
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST4
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST5
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST6
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST7
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST8
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST9
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST10
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

add_pmdk_unit_test(TEST_CASE pmem_memmove
                   TEST_FILE TEST11
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

# Issue: https://github.com/pmem/issues/issues/461
# pmem_memset/TEST1
# Status: PASSING
add_pmdk_unit_test(TEST_CASE pmem_memset
                   TEST_FILE TEST1
                   COMMIT_HASH 0fd509d73
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

# Issue: https://github.com/pmem/issues/issues/463
# Status: needs pmempool
# add_pmdk_unit_test(TEST_CASE pmempool_sync
#                    TEST_FILE TEST0
#                    COMMIT_HASH 0fd509d73
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)


# Issue: https://github.com/pmem/issues/issues/465
# libpmempool_api/TEST1, TEST10, TEST11, TEST13
# add_pmdk_unit_test(TEST_CASE libpmempool_sync
#                    TEST_FILE TEST1
#                    COMMIT_HASH 0fd509d73
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# add_pmdk_unit_test(TEST_CASE libpmempool_sync
#                    TEST_FILE TEST10
#                    COMMIT_HASH 0fd509d73
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# add_pmdk_unit_test(TEST_CASE libpmempool_sync
#                    TEST_FILE TEST11
#                    COMMIT_HASH 0fd509d73
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# add_pmdk_unit_test(TEST_CASE libpmempool_sync
#                    TEST_FILE TEST13
#                    COMMIT_HASH 0fd509d73
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/466
# Status: 
add_pmdk_unit_test(TEST_CASE libpmempool_sync
                   TEST_FILE TEST0
                #    COMMIT_HASH 0fd509d73
                   COMMIT_HASH 9a1651b11
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/535
# pmemobjcli/TEST0
# add_pmdk_unit_test(TEST_CASE pmemobjcli
#                    TEST_FILE TEST0
#                    COMMIT_HASH 60e24d2 
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/585
# Status: requires pmempool
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
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/942
# Status: REPRO'D, cannot fix?
add_pmdk_unit_test(TEST_CASE obj_mem
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/943
# Status: REPRO'D (10m timeout)
add_pmdk_unit_test(TEST_CASE obj_memops
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/945
# pmem_memset/TEST0
# Status: FIX PASSES
add_pmdk_unit_test(TEST_CASE pmem_memset 
                   TEST_FILE TEST0
                   TEST_PATCH pmem_memset_TEST0_e71dfa41b.patch
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE PASSING)

# Issue: https://github.com/pmem/issues/issues/949
# Tests: obj_pmalloc_basic/TEST0
# Status: TIMEOUT
add_pmdk_unit_test(TEST_CASE obj_pmalloc_basic
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)

# Issue: https://github.com/pmem/issues/issues/1103 
# Status: TOOL TEST
# add_pmdk_unit_test(TEST_CASE pmemobjcli 
#                    TEST_FILE TEST0
#                    COMMIT_HASH 2c2ba2e
#                    PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
#                    PMDK_TARGET VALIDATION_PMDK)

# Issue: https://github.com/pmem/issues/issues/1118
# Status: 
add_pmdk_unit_test(TEST_CASE obj_pmalloc_mt
                   TEST_FILE TEST1
                   COMMIT_HASH 7ff604b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK
                   SUITE UNKNOWN)
