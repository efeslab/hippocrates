install(PROGRAMS apply-fixer DESTINATION bin)
configure_file(apply-fixer "${CMAKE_BINARY_DIR}/apply-fixer")

install(PROGRAMS parse-trace DESTINATION bin)
configure_file(parse-trace "${CMAKE_BINARY_DIR}/parse-trace" COPYONLY)

install(PROGRAMS verify DESTINATION bin)
configure_file(verify "${CMAKE_BINARY_DIR}/verify")