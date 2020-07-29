set(CMAKE_C_COMPILER wllvm)
set(CMAKE_CXX_COMPILER wllvm++)
check_wllvm()

add_library(PMINTRINSICS SHARED persistent_intrinsics.c)
target_include_directories(PMINTRINSICS PUBLIC ${PMCHK_INCLUDE})
target_compile_options(PMINTRINSICS PUBLIC "-g;-O0")
add_custom_command(TARGET PMINTRINSICS
                   POST_BUILD
                   COMMAND extract-bc $<TARGET_FILE:PMINTRINSICS>
                               -o $<TARGET_FILE:PMINTRINSICS>.bc
                   COMMENT "\textract-bc PMINTRINSICS")

set(PMINTRINSICS_BITCODE 
    ${CMAKE_CURRENT_BINARY_DIR}/libPMINTRINSICS.so.bc CACHE INTERNAL "")