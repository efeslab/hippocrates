
function(add_test_executable)
    # Need to ensure that we're running with WLLVM as our compiler.
    # --https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#how-do-i-use-a-different-compiler
    string(FIND ${CMAKE_C_COMPILER} "wllvm" C_WLLVM)
    string(FIND ${CMAKE_CXX_COMPILER} "wllvm++" CXX_WLLVM)

    if (${C_WLLVM} EQUAL -1 OR ${CXX_WLLVM} EQUAL -1)
        message(FATAL_ERROR "Must set wllvm and wllvm++ as C and CXX compiler.")
    endif()

    # # Ensure environment variables are set.
    if (NOT DEFINED ENV{LLVM_COMPILER} OR NOT DEFINED ENV{LLVM_COMPILER_PATH})
        message(FATAL_ERROR "Must set LLVM_COMPILER and LLVM_COMPILER_PATH environment variables")
    endif()

    # Now that we know that our compiler is in order, we can parse args.
    set(options)                                                                   
    set(oneValueArgs TARGET)                                                       
    set(multiValueArgs SOURCES EXTRA_LIBS INCLUDE)                                         
    cmake_parse_arguments(FN_ARGS "${options}" "${oneValueArgs}"                   
                        "${multiValueArgs}" ${ARGN})
    
    add_executable(${FN_ARGS_TARGET} ${FN_ARGS_SOURCES})
    target_include_directories(${FN_ARGS_TARGET} PUBLIC ${FN_ARGS_INCLUDE})
    target_link_libraries(${FN_ARGS_TARGET} ${FN_ARGS_EXTRA_LIBS})
    target_compile_options(${FN_ARGS_TARGET} PUBLIC "-g;-march=native")
    
    add_custom_command(TARGET ${FN_ARGS_TARGET}
                       POST_BUILD
                       COMMAND extract-bc $<TARGET_FILE:${FN_ARGS_TARGET}>
                               -o $<TARGET_FILE:${FN_ARGS_TARGET}>.bc
                       COMMENT "\textract-bc ${FN_ARGS_TARGET}")
    
endfunction()

