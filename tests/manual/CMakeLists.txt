message("PMTEST: ${PMTEST_LIBS},${PMTEST_INCLUDE}")
link_directories(${PMTEST_LIBS})

# add_executable(000_SimpleBug 000_simple_bug.c)
# target_include_directories(000_SimpleBug PUBLIC ${PMTEST_INCLUDE})
# target_link_libraries(000_SimpleBug pmtest pthread)
add_test_executable(TARGET 000_SimpleBug SOURCES 000_simple_bug.c)