set(CMAKE_C_COMPILER wllvm)
set(CMAKE_CXX_COMPILER wllvm++)
check_wllvm()

add_library(PMINTRINSICS SHARED persistent_intrinsics.c)

# Inspired by: https://github.com/utsaslab/RECIPE/blob/1717dd567e3a4b6e5efde6a291e4450e1839a064/CMakeLists.txt#L15
execute_process(COMMAND cat /proc/cpuinfo COMMAND grep clflush OUTPUT_VARIABLE ENABLE_CLFLUSH)
execute_process(COMMAND cat /proc/cpuinfo COMMAND grep clflushopt OUTPUT_VARIABLE ENABLE_CLFLUSHOPT)
execute_process(COMMAND cat /proc/cpuinfo COMMAND grep clwb OUTPUT_VARIABLE ENABLE_CLWB)

if(ENABLE_CLWB)
    add_definitions(-DCLWB)
    message(STATUS "PMINTRINSICS: Looking for clwb instruction - found")
elseif(ENABLE_CLFLUSHOPT)
    add_definitions(-DCLFLUSH_OPT)
    message(STATUS "PMINTRINSICS: Looking for clwb instruction - not found")
    message(STATUS "PMINTRINSICS: Looking for clflushopt instruction - found")
elseif(ENABLE_CLFLUSH)
    add_definitions(-DCLFLUSH)
    message(STATUS "PMINTRINSICS: Looking for clwb instruction - not found")
    message(STATUS "PMINTRINSICS: Looking for clflushopt instruction - not found")
    message(STATUS "PMINTRINSICS: Looking for clflush instruction - found")
else()
    message(FATAL_ERROR "Cannot find any flush instructions (clflush, clflushopt, clwb)")
endif()

target_include_directories(PMINTRINSICS PUBLIC ${PMCHK_INCLUDE})
target_compile_options(PMINTRINSICS PUBLIC "-g;-O0;-march=native")
add_custom_command(TARGET PMINTRINSICS
                   POST_BUILD
                   COMMAND extract-bc $<TARGET_FILE:PMINTRINSICS>
                               -o $<TARGET_FILE:PMINTRINSICS>.bc
                   COMMENT "\textract-bc PMINTRINSICS")

set(PMINTRINSICS_BITCODE 
    ${CMAKE_CURRENT_BINARY_DIR}/libPMINTRINSICS.so.bc CACHE INTERNAL "")