################################################################################
#
# This function's purpose in life is to add a unit test from PMDK.
#
# - Need to create a dependency chain so that they don't have any races.
#
################################################################################

function(add_pmdk_unit_test)
    check_wllvm()

    set(options)                                                                  
    set(oneValueArgs TEST_CASE TEST_FILE PMDK_PATH PMDK_TARGET COMMIT_HASH)                                                       
    set(multiValueArgs SOURCES EXTRA_LIBS INCLUDE)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                        "${multiValueArgs}" ${ARGN})

    if (NOT TARGET ${FN_ARGS_PMDK_TARGET})
        message(FATAL_ERROR "PMDK_TARGET ${FN_ARGS_PMDK_TARGET} not a valid target!")
    endif()
    
    set(FN_ARGS_TARGET "${FN_ARGS_TEST_CASE}_${FN_ARGS_TEST_FILE}_${FN_ARGS_COMMIT_HASH}")
    if(TARGET ${FN_ARGS_TARGET})
        message(FATAL_ERROR "${FN_ARGS_TARGET} already a target!")
    endif()

    # # Checkout 
    # add_custom_target(TARGET)
    add_custom_target("${FN_ARGS_TARGET}_checkout"
                      COMMAND git checkout ${FN_ARGS_COMMIT_HASH}
                      WORKING_DIRECTORY ${FN_ARGS_PMDK_PATH}
                      DEPENDS ${FN_ARGS_PMDK_TARGET}
                      COMMENT "")
                      
    add_custom_target(${FN_ARGS_TARGET} ALL : # no-op
                      DEPENDS "${FN_ARGS_TARGET}_checkout"
                      COMMENT "${FN_ARGS_TARGET} complete.")               
    
    # add_executable(${FN_ARGS_TARGET} ${FN_ARGS_SOURCES})
    # target_include_directories(${FN_ARGS_TARGET} PUBLIC ${FN_ARGS_INCLUDE})
    # target_link_libraries(${FN_ARGS_TARGET} ${FN_ARGS_EXTRA_LIBS})
    # # Turning off optimizations is important to avoid line-combining.
    # target_compile_options(${FN_ARGS_TARGET} PUBLIC "-g;-march=native;-O0")
    
    # add_custom_command(TARGET ${FN_ARGS_TARGET}
    #                     POST_BUILD
    #                     COMMAND extract-bc $<TARGET_FILE:${FN_ARGS_TARGET}>
    #                             -o $<TARGET_FILE:${FN_ARGS_TARGET}>.bc
    #                     COMMENT "\textract-bc ${FN_ARGS_TARGET}")
     
    # append_tool_lists(TARGET ${FN_ARGS_TARGET} TOOL "PMDK_UNIT_TEST")

endfunction()