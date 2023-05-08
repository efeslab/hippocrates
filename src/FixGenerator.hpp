#pragma once 
/**
 * 
 */

#include <stdint.h>
#include <functional>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "BugReports.hpp"
#include "FlowAnalyzer.hpp"

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

    /**
     * Direct lookup.
     */
    llvm::Function *getPersistentIntrinsic(const char *name) const;

protected:
    llvm::Module &module_;
    const PmDesc *pmDesc_;
    llvm::ValueToValueMapTy *traceAAMap_;

    /** PURE UTILITY
     */

    

    /** PERF UTILITY
     * 
     */

    /**
     * Creates a new global variable, and inserts an unconditional reset before
     * resetBefore and an unconditional set at setAt.
     */
    llvm::GlobalVariable *createConditionVariable(
        const std::list<llvm::Instruction*> &resetBefore, 
        llvm::Instruction *setAt);

    llvm::Instruction *createConditionalBlock(
        llvm::Instruction *start, 
        llvm::Instruction *end,
        const std::list<llvm::GlobalVariable*> &conditions);

    /**
     * Duplicates the function. Not recursive.
     */
    llvm::Function *duplicateFunction(
        llvm::Function *f, llvm::ValueToValueMapTy &vmap, std::string postFix="_NT");

    /**
     * Replaces all stores (recursively) with non-temporal hinted stores.
     * Edit: only replace stores which alias persistent memory.
     */
    bool makeAllStoresPersistent(
        llvm::Function *oldF, llvm::Function *newF, const llvm::ValueToValueMapTy &vmap);

public:
    FixGenerator(llvm::Module &m, const PmDesc *pm, llvm::ValueToValueMapTy *vmap) 
        : module_(m), pmDesc_(pm), traceAAMap_(vmap) {}

    /** CORRECTNESS
     * All these functions return the new instruction they created (or a pointer
     * to the last instruction they created), or nullptr if they were not 
     * successful.
     */

    virtual llvm::Instruction *insertFlush(const FixLoc &fl) = 0;

    virtual llvm::Instruction *insertFence(const FixLoc &fl) = 0;

    /**
     *
     */
    virtual llvm::Instruction *insertPersistentSubProgram(
        BugLocationMapper &mapper,
        const FixLoc &fl,
        const std::vector<LocationInfo> &callstack,
        int idx,
        bool insertFlushes,
        bool insertFence) = 0;

    /** PERFORMANCE
     * Removes an unnecessary flush
     */

    /**
     * Removes a single flush (at instruction i).
     */
    virtual bool removeFlush(const FixLoc &fl) = 0;

    virtual bool removeFlushConditionally(
        const std::list<FixLoc> &origs, 
        const FixLoc &redt,
        std::list<llvm::Instruction*> pathPoints) = 0;

    llvm::CallBase *modifyCall(llvm::CallBase *cb, llvm::Function *newFn);

    /**
     * Adds the name prefix.
     */
    llvm::Function *getPersistentVersion(const char *name) const;

    llvm::Function *getClwbDefinition() const;

    llvm::Function *getSfenceDefinition() const; 

    llvm::Function *getPersistentMemcpy() const;

    llvm::Function *getPersistentMemset() const;

    llvm::Function *getPersistentMemmove() const;
};

/**
 * This flavor of fix generator just has to insert the fix. No metadata or 
 * tracing needs to be added.
 */
class GenericFixGenerator : public FixGenerator {
private:

public:
    GenericFixGenerator(llvm::Module &m, const PmDesc *pm, llvm::ValueToValueMapTy *vmap) 
        : FixGenerator(m, pm, vmap) {}

    virtual llvm::Instruction *insertFlush(const FixLoc &fl) override;

    virtual llvm::Instruction *insertFence(const FixLoc &fl) override;

    virtual llvm::Instruction *insertPersistentSubProgram(
        BugLocationMapper &mapper,
        const FixLoc &fl,
        const std::vector<LocationInfo> &callstack, 
        int idx,
        bool insertFlushes,
        bool insertFence) override;

    virtual bool removeFlush(const FixLoc &fl) override;

    virtual bool removeFlushConditionally(
        const std::list<FixLoc> &origs, 
        const FixLoc &redt,
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
    PMTestFixGenerator(llvm::Module &m, const PmDesc *pm, llvm::ValueToValueMapTy *vmap) 
        : FixGenerator(m, pm, vmap) {}

    virtual llvm::Instruction *insertFlush(const FixLoc &fl) override;

    virtual llvm::Instruction *insertFence(const FixLoc &fl) override;

    virtual llvm::Instruction *insertPersistentSubProgram(
        BugLocationMapper &mapper,
        const FixLoc &fl,
        const std::vector<LocationInfo> &callstack, 
        int idx,
        bool insertFlush,
        bool insertFence) override;

    virtual bool removeFlush(const FixLoc &fl) override;

    virtual bool removeFlushConditionally(
        const std::list<FixLoc> &origs, 
        const FixLoc &redt,
        std::list<llvm::Instruction*> pathPoints) override;
};

}