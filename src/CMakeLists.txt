add_llvm_library(PMFIXER MODULE  # Name of the generated shared library
    pm_bug_fixer_pass.cpp        # Your pass
    pass_utils.cpp               # ... other stuff ...
    PLUGIN_TOOL
    opt
)
