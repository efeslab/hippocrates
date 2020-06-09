include_directories((../common))
add_llvm_library(FLUSHREMOVER MODULE
    FlushRemover.cpp
    ../common/PassUtils.cpp
    PLUGIN_TOOL
    opt
)

set(FLUSH_REMOVER_PATH ${CMAKE_CURRENT_BINARY_DIR}/FLUSHREMOVER.so 
    CACHE INTERNAL "Path to FlushRemover pass")