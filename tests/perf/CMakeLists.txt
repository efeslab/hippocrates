link_directories(${PMDK_LIBS})

add_test_executable(TARGET 000_SimplePerf 
                    SOURCES 000_simple_perf.c 
                    INCLUDE ${PMDK_INCLUDE}
                    EXTRA_LIBS pmem pthread
                    TOOLS PMDK)