#pragma once 
/**
 * 
 */

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"

#include "BugReports.hpp"

namespace pmfix {

/**
 * Subclasses of FixGenerator insert and/or perform fixes for the various types
 * of bugs.
 * 
 * This contains some helper functions as well, like getting the right function
 * definitions for flush and fence functions.
 */
class FixGenerator {
private:

protected:
    llvm::Module &module_;

    /** PURE UTILITY
     */

    llvm::Function *getClwbDefinition() const;

    llvm::Function *getSfenceDefinition() const; 

    /** PERF UTILITY
     * 
     */

    /**
     * Creates a new global variable, and inserts an unconditional reset before
     * resetBefore and an unconditional set at setAt.
     */
    llvm::GlobalVariable *createConditionVariable(
        llvm::Instruction *resetBefore, llvm::Instruction *setAt);

    llvm::Instruction *createConditionalBlock(
        llvm::Instruction *start, llvm::Instruction *end,
        const std::list<llvm::GlobalVariable*> &conditions);

public:
    FixGenerator(llvm::Module &m) : module_(m) {}

    /** CORRECTNESS
     * All these functions return the new instruction they created (or a pointer
     * to the last instruction they created), or nullptr if they were not 
     * successful.
     */

    virtual llvm::Instruction *insertFlush(llvm::Instruction *i) = 0;

    virtual llvm::Instruction *insertFence(llvm::Instruction *i) = 0;

    /**
     *
     */
    virtual llvm::Instruction *insertPersistentSubProgram(llvm::Instruction *i,
        const std::vector<LocationInfo> &callstack) = 0;

    /** PERFORMANCE
     * Removes an unnecessary flush
     */

    /**
     * Removes a single flush (at instruction i).
     */
    virtual bool removeFlush(llvm::Instruction *i) = 0;

    virtual bool removeFlushConditionally(
        llvm::Instruction *original, llvm::Instruction *redundant,
        std::list<llvm::Instruction*> pathPoints) = 0;
};

/**
 * This flavor of fix generator just has to insert the fix. No metadata or 
 * tracing needs to be added.
 */
class GenericFixGenerator : public FixGenerator {
private:

public:
    GenericFixGenerator(llvm::Module &m) : FixGenerator(m) {}

    virtual llvm::Instruction *insertFlush(llvm::Instruction *i) override;

    virtual llvm::Instruction *insertFence(llvm::Instruction *i) override;

    virtual llvm::Instruction *insertPersistentSubProgram(llvm::Instruction *i,
        const std::vector<LocationInfo> &callstack) override;

    virtual bool removeFlush(llvm::Instruction *i) override;

    virtual bool removeFlushConditionally(
        llvm::Instruction *original, llvm::Instruction *redundant,
        std::list<llvm::Instruction*> pathPoints) override;
};

/**
 * This flavor of fix generator has to insert both the appropriate fix and
 * insert PMTest trace events which validate the fix.
 */
class PMTestFixGenerator : public FixGenerator {
private:

    bool findFlushAndAssertion(llvm::Instruction *end, 
                               llvm::Instruction **flush, 
                               llvm::Instruction **assert);

public:
    PMTestFixGenerator(llvm::Module &m) : FixGenerator(m) {}

    virtual llvm::Instruction *insertFlush(llvm::Instruction *i) override;

    virtual llvm::Instruction *insertFence(llvm::Instruction *i) override;

    virtual llvm::Instruction *insertPersistentSubProgram(llvm::Instruction *i,
        const std::vector<LocationInfo> &callstack) override;

    virtual bool removeFlush(llvm::Instruction *i) override;

    virtual bool removeFlushConditionally(
        llvm::Instruction *original, llvm::Instruction *redundant,
        std::list<llvm::Instruction*> pathPoints) override;
};

}