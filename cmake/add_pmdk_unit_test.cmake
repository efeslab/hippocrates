################################################################################
#
# This function's purpose in life is to add a unit test from PMDK
#
################################################################################

function(add_pmdk_unit_test)
    check_wllvm()

    set(options)                                                                   
    set(oneValueArgs TARGET)                                                       
    set(multiValueArgs SOURCES EXTRA_LIBS INCLUDE)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                        "${multiValueArgs}" ${ARGN})
    
    # -- We want at least to know what tool and target, etc
    if (NOT DEFINED FN_ARGS_TOOL)
        message(FATAL_ERROR "Must provide TOOL argument!")
    endif()
    
    add_executable(${FN_ARGS_TARGET} ${FN_ARGS_SOURCES})
    target_include_directories(${FN_ARGS_TARGET} PUBLIC ${FN_ARGS_INCLUDE})
    target_link_libraries(${FN_ARGS_TARGET} ${FN_ARGS_EXTRA_LIBS})
    # Turning off optimizations is important to avoid line-combining.
    target_compile_options(${FN_ARGS_TARGET} PUBLIC "-g;-march=native;-O0")
    
    add_custom_command(TARGET ${FN_ARGS_TARGET}
                        POST_BUILD
                        COMMAND extract-bc $<TARGET_FILE:${FN_ARGS_TARGET}>
                                -o $<TARGET_FILE:${FN_ARGS_TARGET}>.bc
                        COMMENT "\textract-bc ${FN_ARGS_TARGET}")
     
    append_tool_lists(TARGET ${FN_ARGS_TARGET} TOOL "PMDK_UNIT_TEST")

endfunction()