link_directories(${YAMLCPP_LIBS})

message("Exception: ${LLVM_ENABLE_EH}")
set(LLVM_ENABLE_EH ON)
add_llvm_library(PMFIXER MODULE  # Name of the generated shared library
    PmBugFixerPass.cpp           # Your pass
    PassUtils.cpp                # ... other stuff ...
    BugReports.cpp
    BugFixer.cpp
    PLUGIN_TOOL
    opt
)

target_include_directories(PMFIXER PUBLIC ${YAMLCPP_INCLUDE})
target_link_libraries(PMFIXER PUBLIC yaml-cpp -Wl,-rpath=${YAMLCPP_LIBS})
target_compile_options(PMFIXER PUBLIC "-fPIC")