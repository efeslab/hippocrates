add_llvm_library(LLVMHEURISTIC MODULE  # Name of the generated shared library
    pass_utils.cpp
    nvm_function_info.cpp
    persistence_heuristic.cpp          # Your pass
    PLUGIN_TOOL
    opt
)
