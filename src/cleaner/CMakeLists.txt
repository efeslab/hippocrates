include_directories((../common))
add_llvm_library(FUNCTIONCLEANER MODULE
    FunctionCleaner.cpp
    PLUGIN_TOOL
    opt
)

set(FUNCTION_CLEANER_PATH ${CMAKE_CURRENT_BINARY_DIR}/FUNCTIONCLEANER.so 
    CACHE INTERNAL "Path to FunctionCleaner pass")