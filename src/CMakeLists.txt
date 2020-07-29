link_directories(${YAMLCPP_LIBS})

# This is off by default but we need to enable it to use the yaml++ library.
set(LLVM_ENABLE_EH ON)
add_llvm_library(PMFIXER MODULE  # Name of the generated shared library
    PmBugFixerPass.cpp           # Your pass
    PassUtils.cpp                # ... other stuff ...
    BugReports.cpp
    BugFixer.cpp
    FixGenerator.cpp
    FlowAnalyzer.cpp
    PLUGIN_TOOL
    opt
)

set(LLVM_PASS_PATH ${CMAKE_CURRENT_BINARY_DIR}/PMFIXER.so CACHE INTERNAL 
    "Path to ${CMAKE_PROJECT_NAME} compiled pass.")

target_include_directories(PMFIXER PUBLIC ${YAMLCPP_INCLUDE} ${ANDERSEN_INCLUDE})
target_link_libraries(PMFIXER PUBLIC yaml-cpp -Wl,-rpath=${YAMLCPP_LIBS} 
                                     Andersen -Wl,-rpath=${ANDERSEN_LIB})
target_compile_options(PMFIXER PUBLIC "-fPIC")

add_subdirectory(intrinsic)