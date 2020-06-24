#pragma once 
/**
 * 
 */

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"

#include "BugReports.hpp"

namespace pmfix {

/**
 * Subclasses of FixGenerator insert and/or perform fixes for the various types
 * of bugs.
 */
class FixGenerator {
private:

protected:
    llvm::Module &module_;

public:
    FixGenerator(llvm::Module &m) : module_(m) {}

    /** CORRECTNESS
     * All these functions return the new instruction they created (or a pointer
     * to the last instruction they created), or nullptr if they were not 
     * successful.
     */

    virtual llvm::Instruction *insertFlush(llvm::Instruction *i) = 0;

    virtual llvm::Instruction *insertFence(llvm::Instruction *i) = 0;
};

/**
 * This flavor of fix generator has to insert both the appropriate fix and
 * insert PMTest trace events which validate the fix.
 */
class PMTestFixGenerator : public FixGenerator {
private:

public:
    PMTestFixGenerator(llvm::Module &m) : FixGenerator(m) {}

    virtual llvm::Instruction *insertFlush(llvm::Instruction *i) override;

    virtual llvm::Instruction *insertFence(llvm::Instruction *i) override;
};

/**
 * Runs all the fixing algorithms.
 */
class BugFixer {
private:
    llvm::Module &module_;
    TraceInfo &trace_;
    BugLocationMapper mapper_;

    bool fixBug(FixGenerator *fixer, const TraceEvent &te, int bug_index);

public:
    BugFixer(llvm::Module &m, TraceInfo &ti) : module_(m), trace_(ti), mapper_(m) {}

    /**
     * Returns true if modifications were made to the program.
     */
    bool doRepair(void);
};

}