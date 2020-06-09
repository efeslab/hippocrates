cmake_minimum_required(VERSION 3.0)
# https://stackoverflow.com/questions/54864924/cmakelist-file-to-generate-llvm-bitcode-file-from-c-source-file
set(CMAKE_C_COMPILER "clang-8")
set(CMAKE_LINKER "ldd-8")
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-flto")
add_executable(full_copy full_copy.c)
add_executable(simple_cfg simple_cfg.c)
target_link_libraries(full_copy pmem pthread)
target_compile_options(full_copy PUBLIC ${CMAKE_C_FLAGS} -emit-llvm)

# Do something to statically compile the object with PMDK.
