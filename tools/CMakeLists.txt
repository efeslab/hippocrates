install(PROGRAMS apply-fixer DESTINATION bin)
configure_file(apply-fixer "${CMAKE_BINARY_DIR}/apply-fixer")

install(PROGRAMS clean-functions DESTINATION bin)
configure_file(clean-functions "${CMAKE_BINARY_DIR}/clean-functions")

install(PROGRAMS remove-flushes DESTINATION bin)
configure_file(remove-flushes "${CMAKE_BINARY_DIR}/remove-flushes")

install(PROGRAMS run-redis DESTINATION bin)
configure_file(run-redis "${CMAKE_BINARY_DIR}/run-redis")

install(PROGRAMS parse-trace DESTINATION bin)
configure_file(Reports.py "${CMAKE_BINARY_DIR}/Reports.py" COPYONLY)

install(PROGRAMS parse-trace DESTINATION bin)
configure_file(parse-trace "${CMAKE_BINARY_DIR}/parse-trace")

install(PROGRAMS verify DESTINATION bin)
configure_file(verify "${CMAKE_BINARY_DIR}/verify")

install(PROGRAMS verify-memcached DESTINATION bin)
configure_file(verify "${CMAKE_BINARY_DIR}/verify-memcached")

install(PROGRAMS verify-recipe DESTINATION bin)
configure_file(verify "${CMAKE_BINARY_DIR}/verify-recipe")