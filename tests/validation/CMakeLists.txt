set(VALIDATE_PMDK_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk)

add_custom_command(OUTPUT ${VALIDATE_PMDK_ROOT_DIR}
                   COMMAND git clone https://github.com/pmem/pmdk ${VALIDATE_PMDK_ROOT_DIR}
                   COMMENT "Clone PMDK repo (${VALIDATE_PMDK_ROOT_DIR})")       
