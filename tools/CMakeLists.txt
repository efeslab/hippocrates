install(PROGRAMS apply-fixer DESTINATION bin)
get_property(LLVM_PASS_PATH GLOBAL PROPERTY pass_library) 
configure_file(apply-fixer "${CMAKE_BINARY_DIR}/apply-fixer")

install(PROGRAMS parse-trace DESTINATION bin)
configure_file(parse-trace "${CMAKE_BINARY_DIR}/parse-trace" COPYONLY)