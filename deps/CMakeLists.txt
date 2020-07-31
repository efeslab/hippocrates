add_subdirectory(andersen)
set(ANDERSEN_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/andersen/include CACHE INTERNAL "")
set(ANDERSEN_LIB ${CMAKE_CURRENT_BINARY_DIR}/andersen/lib CACHE INTERNAL "")

add_subdirectory(pmtest)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_lib_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_include_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp/include)

include(ProcessorCount)
ProcessorCount(NPROC)

set(PMDK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pmdk CACHE INTERNAL "")
set(PMDK_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk CACHE INTERNAL "")
set(PMDK_INCLUDE_DIR ${PMDK_INSTALL_DIR}/include CACHE INTERNAL "")
set(PMDK_INCLUDE ${PMDK_INCLUDE_DIR} CACHE INTERNAL "")
set(PMDK_LIB_DIR ${PMDK_INSTALL_DIR}/lib CACHE INTERNAL "")
set(PMDK_LIBS ${PMDK_LIB_DIR} CACHE INTERNAL "")

add_custom_command(OUTPUT ${PMDK_ROOT_DIR}/src/debug
                   COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC}
                   COMMENT "Build PMDK dependency")
add_custom_command(OUTPUT ${PMDK_INSTALL_DIR}
                   COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC} 
                           install prefix=${PMDK_INSTALL_DIR}
                   DEPENDS ${PMDK_ROOT_DIR}/src/debug
                   COMMENT "Install PMDK dependency")
add_custom_target(PMDK ALL : # no-op command
                  DEPENDS ${PMDK_INSTALL_DIR}
                  COMMENT "PMDK dependencies complete")       


set(PMCHK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/valgrind-pmem CACHE INTERNAL "")
set(PMCHK_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/valgrind-pmem CACHE INTERNAL "")
set(PMCHK_INCLUDE_DIR ${PMCHK_INSTALL_DIR}/include CACHE INTERNAL "")
set(PMCHK_INCLUDE ${PMCHK_INCLUDE_DIR} CACHE INTERNAL "")
set(PMCHK_LIB_DIR ${PMCHK_INSTALL_DIR}/lib CACHE INTERNAL "")
set(PMCHK_BIN_DIR ${PMCHK_INSTALL_DIR}/bin CACHE INTERNAL 
    "Location of pmemcheck binary.")

add_custom_command(OUTPUT ${PMCHK_ROOT_DIR}/configure
                   COMMAND ./autogen.sh 
                   WORKING_DIRECTORY ${PMCHK_ROOT_DIR}
                   COMMENT "PMEMCheck autogen.sh")
add_custom_command(OUTPUT ${PMCHK_ROOT_DIR}/Makefile
                   DEPENDS ${PMCHK_ROOT_DIR}/configure
                   COMMAND ./configure --prefix=${PMCHK_INSTALL_DIR} 
                   WORKING_DIRECTORY ${PMCHK_ROOT_DIR}
                   COMMENT "PMEMCheck configure")
add_custom_command(OUTPUT ${PMCHK_ROOT_DIR}/pmemcheck/pmemcheck-amd64-linux
                   DEPENDS ${PMCHK_ROOT_DIR}/Makefile
                   COMMAND make -C ${PMCHK_ROOT_DIR} -j${NPROC}
                   COMMENT "PMEMCheck build")
add_custom_command(OUTPUT ${PMCHK_INSTALL_DIR}/bin/valgrind
                   DEPENDS ${PMCHK_ROOT_DIR}/pmemcheck/pmemcheck-amd64-linux
                   COMMAND make -C ${PMCHK_ROOT_DIR} -j${NPROC} install
                   COMMENT "PMEMCheck install")
add_custom_target(PMEMCHECK ALL : # no-op command
                  DEPENDS ${PMCHK_INSTALL_DIR}/bin/valgrind
                  COMMENT "PMEMCheck dependencies complete")

################################################################################
# RECIPE
################################################################################

add_subdirectory(RECIPE/P-CLHT)