link_directories(${YAMLCPP_LIBS})
message("Exception: ${LLVM_ENABLE_EH}")
set(LLVM_ENABLE_EH ON)
add_llvm_library(PMFIXER MODULE  # Name of the generated shared library
    PmBugFixerPass.cpp           # Your pass
    pass_utils.cpp               # ... other stuff ...
    BugReports.cpp
    PLUGIN_TOOL
    opt
)

target_include_directories(PMFIXER PUBLIC ${YAMLCPP_INCLUDE})
target_link_libraries(PMFIXER yaml-cpp)
target_compile_options(PMFIXER PUBLIC "-fPIC")