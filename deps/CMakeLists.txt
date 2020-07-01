add_subdirectory(andersen)
add_subdirectory(pmtest)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_lib_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_include_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp/include)

include(ProcessorCount)
ProcessorCount(NPROC)

set(PMDK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pmdk CACHE INTERNAL)
set(PMDK_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/pmdk CACHE INTERNAL)
set(PMDK_INCLUDE_DIR ${PMDK_INSTALL_DIR}/include CACHE INTERNAL)
set(PMDK_INCLUDE ${PMDK_INCLUDE_DIR} CACHE INTERNAL)
set(PMDK_LIB_DIR ${PMDK_INSTALL_DIR}/lib CACHE INTERNAL)
set(PMDK_LIBS ${PMDK_LIB_DIR} CACHE INTERNAL)
add_custom_command(OUTPUT ${PMDK_ROOT_DIR}/src/debug
                   COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC}
                   COMMENT "Build PMDK dependency")
add_custom_command(OUTPUT ${PMDK_INSTALL_DIR}
                   COMMAND make -C ${PMDK_ROOT_DIR} -j${NPROC} 
                           install prefix=${PMDK_INSTALL_DIR}
                   COMMENT "Install PMDK dependency")


set(PMCHK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/valgrind-pmem)
set(PMCHK_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/valgrind-pmem)
set(PMCHK_INCLUDE_DIR ${PMCHK_INSTALL_DIR}/include)
set(PMCHK_LIB_DIR ${PMCHK_INSTALL_DIR}/lib)
if(NOT EXISTS ${PMCHK_ROOT_DIR}/Makefile)
    execute_process(COMMAND ./autogen.sh WORKING_DIRECTORY ${PMCHK_ROOT_DIR})
    execute_process(COMMAND ./configure --prefix=${PMCHK_INSTALL_DIR} WORKING_DIRECTORY ${PMCHK_ROOT_DIR})
endif()
execute_process(COMMAND make -C ${PMCHK_ROOT_DIR} -j${NPROC})
execute_process(COMMAND make -C ${PMCHK_ROOT_DIR} -j${NPROC} install)
set_property(GLOBAL PROPERTY pmemcheck_include_dir ${PMCHK_INCLUDE_DIR})

set(PMCHK_BIN_DIR ${PMCHK_INSTALL_DIR}/bin CACHE INTERNAL "Location of pmemcheck binary.")