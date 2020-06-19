add_subdirectory(andersen)
add_subdirectory(pmtest)

set(BUILD_SHARED_LIBS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_lib_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp)
set_property(GLOBAL PROPERTY yaml_cpp_include_dir ${CMAKE_CURRENT_BINARY_DIR}/yaml-cpp/include)