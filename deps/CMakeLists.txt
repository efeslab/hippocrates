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

# For some reason, you HAVE to do this via a variable or it thinks you're setting
# VALGRIND_ENABLED to "1 -Wno...."
set(PMDK_FLAGS "-DVALGRIND_ENABLED=1 -Wno-error=unused-function")
add_custom_command(OUTPUT ${PMDK_ROOT_DIR}/src/debug
                   COMMAND make CC=wllvm CXX=wllvm++ DEBUG=1
                            EXTRA_CFLAGS=${PMDK_FLAGS}
                             -C ${PMDK_ROOT_DIR} -j${NPROC}
                   COMMENT "Build PMDK dependency")
add_custom_command(OUTPUT ${PMDK_INSTALL_DIR}
                   COMMAND make CC=wllvm CXX=wllvm++ DEBUG=1
                            EXTRA_CFLAGS=${PMDK_FLAGS}
                            -C ${PMDK_ROOT_DIR} -j${NPROC} 
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

################################################################################
# Redis
################################################################################

#make USE_PMDK=yes STD=-std=gnu99
#OPTIMIZATION=-O0
set(REDIS_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/redis)
add_custom_target(REDIS 
                #   DEPENDS PMDK
                  COMMAND make 
                        CC=wllvm CXX=wllvm++
                        OPTIMIZATION=-O0 
                        INTERNAL_PMDK=no
                        PMDK_DIR=${PMDK_ROOT_DIR} 
                        USE_PMDK=yes
                        -C ${REDIS_ROOT_DIR}/src -j40
                  COMMENT "Building redis-pmem...")       

################################################################################
# memcached-pmem
################################################################################

set(MP_CFLAGS "-O0 -g -I${PMDK_INCLUDE_DIR}")
set(MP_LDFLAGS "-L${PMDK_LIB_DIR}")
# set(MP_LIBS "-lpthread -levent")
# set(MP_CFLAGS "-I${PMDK_INCLUDE_DIR}")
# set(MP_LDFLAGS "-L${PMDK_LIB_DIR}")
# set(MP_LIBS "")
set(MP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/memcached-pmem)
set(MP_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/memcached-pmem)
set(MP_BINARY ${MP_INSTALL_DIR}/bin/memcached)
add_custom_target(MEMCACHED_PMEM
                   COMMAND CC=wllvm 
                           CFLAGS=${MP_CFLAGS} LDFLAGS=${MP_LDFLAGS} #LIBS=${MP_LIBS}
                           ./configure --enable-pslab --prefix=${MP_INSTALL_DIR}
                   WORKING_DIRECTORY ${MP_ROOT_DIR}
                   COMMAND make CC=wllvm 
                           CFLAGS=${MP_CFLAGS} LDFLAGS=${MP_LDFLAGS} #LIBS=${MP_LIBS}
                           -C ${MP_ROOT_DIR} clean
                   COMMAND make CC=wllvm 
                           CFLAGS=${MP_CFLAGS} LDFLAGS=${MP_LDFLAGS} #LIBS=${MP_LIBS}
                           -C ${MP_ROOT_DIR} -j40
                   COMMAND make CC=wllvm 
                           CFLAGS=${MP_CFLAGS} LDFLAGS=${MP_LDFLAGS} #LIBS=${MP_LIBS}
                           -C ${MP_ROOT_DIR} install
                  #  COMMAND patchelf --set-rpath=${PMDK_LIB_DIR}/pmdk_debug ${MP_BINARY}
                   COMMAND extract-bc -o ${MP_BINARY}.bc ${MP_BINARY}
                   COMMENT "Building memcached-pmem...")     