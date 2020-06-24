link_directories(${PMTEST_LIBS})

add_test_executable(TARGET 000_MissingFlush_PMTest
                    SOURCES 000_missing_flush_pmtest.c
                    INCLUDE ${PMTEST_INCLUDE}
                    EXTRA_LIBS pmtest pthread)

add_test_executable(TARGET 001_MissingFence_PMTest
                    SOURCES 001_missing_fence_pmtest.c
                    INCLUDE ${PMTEST_INCLUDE}
                    EXTRA_LIBS pmtest pthread)