include(ProcessorCount)
ProcessorCount(NPROC)

set(VALIDATE_PMDK_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk)
  
add_custom_target(VALIDATION_PMDK 
                  COMMAND git clone https://github.com/pmem/pmdk ${VALIDATE_PMDK_ROOT_DIR}
                  COMMENT "PMDK validation repo cloned")           


# set(PMDK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pmdk CACHE INTERNAL "")
# set(PMDK_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk CACHE INTERNAL "")
# set(PMDK_INCLUDE_DIR ${PMDK_INSTALL_DIR}/include CACHE INTERNAL "")
# set(PMDK_INCLUDE ${PMDK_INCLUDE_DIR} CACHE INTERNAL "")
# set(PMDK_LIB_DIR ${PMDK_INSTALL_DIR}/lib CACHE INTERNAL "")
# set(PMDK_LIBS ${PMDK_LIB_DIR} CACHE INTERNAL "")

# add_custom_command(OUTPUT ${PMDK_ROOT_DIR}/src/debug
#                    COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC}
#                    COMMENT "Build PMDK dependency")
# add_custom_command(OUTPUT ${PMDK_INSTALL_DIR}
#                    COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC} 
#                            install prefix=${PMDK_INSTALL_DIR}
#                    DEPENDS ${PMDK_ROOT_DIR}/src/debug
#                    COMMENT "Install PMDK dependency")
# add_custom_target(PMDK ALL : # no-op command
#                   DEPENDS ${PMDK_INSTALL_DIR}
#                   COMMENT "PMDK dependencies complete")

add_pmdk_unit_test(TEST_CASE pmem_memset 
                   TEST_FILE TEST0
                   COMMIT_HASH e71dfa41b
                   PMDK_PATH ${VALIDATE_PMDK_ROOT_DIR}
                   PMDK_TARGET VALIDATION_PMDK)