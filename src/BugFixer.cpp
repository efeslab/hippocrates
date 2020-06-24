#include "BugFixer.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"

using namespace pmfix;
using namespace llvm;

#pragma region FixGenerators

/**
 * So, with the source code mapping, the instruction we have should be a call
 * to the C_createMetadata_Assign function. Arguments 2 and 3 are address and 
 * width, so we can steal that for making the flush and the call to 
 * "C_createMetadata_Flush"
 * 
 * TODO: Metadata for inserted instructions?
 * TODO: What kind of flush? Determine on machine stuff I'm sure.
 */
Instruction *PMTestFixGenerator::insertFlush(Instruction *i) {

    if (CallInst *ci = dyn_cast<CallInst>(i)) {
        Function *f = ci->getCalledFunction();
        assert(f && f->getName() == "C_createMetadata_Assign" && "IDK!");

        Value *addrExpr = ci->getArgOperand(1); // 0-indexed
        Value *lenExpr  = ci->getArgOperand(2); 

        errs() << "Address of assign: " << *addrExpr << "\n";
        errs() << "Length of assign:  " << *lenExpr << "\n";

        // I think we've made the assumption up to this point that len <= 64
        // and that [addr, addr + len) is within a cacheline. We'll continue
        // on with that assumption.

        // 1) Set up the IR Builder.
        // -- want AFTER
        IRBuilder<> builder(i->getNextNode());

        // 2) Find and insert a clwb.
        Function *clwb = module_.getFunction("llvm.x86.clwb");
        assert(clwb && "could not find clwb!");
        // -- Need to assemble an ArrayRef<Value>
        // Value* clwbArgs[1] = {addrExpr};
        // ArrayRef<Value*> clwbArgRef(clwbArgs);
        CallInst *clwbCall = builder.CreateCall(clwb, {addrExpr});

        // 3) Find and insert a trace instruction.
        Function *trace = module_.getFunction("C_createMetadata_Flush");
        assert(trace && "could not find PMTest flush trace!");

        // Should be the same arguments as to the assign call...
        CallInst *traceCall = builder.CreateCall(trace, {ci->getArgOperand(0),
            addrExpr, lenExpr, ci->getArgOperand(3), ci->getArgOperand(4)});

        // return clwbCall;
        return traceCall;
    }

    return nullptr;
}

#pragma endregion

#pragma region BugFixer

bool BugFixer::fixBug(FixGenerator *fixer, const TraceEvent &te, int bug_index) {
    assert(te.isBug && "Can't fix a not-a-bug!");

    if (te.type == TraceEvent::ASSERT_PERSISTED) {
        errs() << "\tPersistence Bug!\n";
        assert(te.addresses.size() == 1 &&
               "A persist assertion should only have 1 address!");
        assert(te.addresses.front().isSingleCacheLine() &&
               "Don't know how to handle non-standard ranges which cross lines!");
        /**
         * If something is not persisted, that means one of three things:
         * 1. It is missing a flush.
         * - In this case, we need to insert a flush between the ASSIGN and 
         * it's nearest FENCE.
         * - Needs to be some check-up operation.
         * 
         * 2. It is missing a fence.
         * - In this case, we need to insert a fence after the ASSIGN and it's 
         * FLUSH.
         * 
         * 3. It is missing a flush AND a fence.
         */
        bool missingFlush = false;
        bool missingFence = true;
        // Need this so we know where the eventual fix will go.
        int lastOpIndex = -1;

        // First, determine which case we are in by going backwards.
        for (int i = bug_index - 1; i >= 0; i--) {
            const TraceEvent &event = trace_[i];
            if (!event.isOperation()) continue;

            assert(event.addresses.size() <= 1 && 
                    "Don't know how to handle more addresses!");
            if (event.addresses.size()) {
                assert(event.addresses.front().isSingleCacheLine() && 
                       "Don't know how to handle multi-cache line operations!");

                if (event.type == TraceEvent::STORE) {
                    missingFlush = true;
                    lastOpIndex = i;
                    break;
                } else if (event.type == TraceEvent::FLUSH) {
                    assert(missingFence == true &&
                           "Shouldn't be a bug in this case, has flush and fence");
                    lastOpIndex = i;
                    break;
                }
            } else if (event.type == TraceEvent::FENCE) {
                missingFence = false;
                missingFlush = true;
            }
        }

        errs() << "\t\tMissing Flush? : " << missingFlush << "\n";
        errs() << "\t\tMissing Fence? : " << missingFence << "\n";
        errs() << "\t\tLast Operation : " << lastOpIndex << "\n";

        assert(lastOpIndex >= 0 && "Has to had been assigned at least!");

        // Find where the last operation was.
        const TraceEvent &last = trace_[lastOpIndex];
        errs() << "\t\tLocation : " << last.location.file << ":" << last.location.line << "\n";
        Instruction *i = mapper_[last.location];
        assert(i && "can't be null!");
        errs() << "\t\tInstruction : " << *i << "\n";

        if (missingFlush) {
            i = fixer->insertFlush(i);
        }

        if (missingFence) {
            i = fixer->insertFence(i);
        } 

        return nullptr != i;            
    } else {
        errs() << "Not yet supported: " << te.typeString << "\n";
        return false;
    }

    errs() << "Fallthrough!!!\n";
    return false;
}

bool BugFixer::doRepair(void) {
    bool modified = false;

    // TODO: see below
    errs() << "TODO: pick the right bug fixer!\n";
    PMTestFixGenerator pmtestFixer(module_);
    FixGenerator *fixer = &pmtestFixer;

    for (int bug_index : trace_.bugs()) {
        errs() << "Bug Index: " << bug_index << "\n";
        bool res = fixBug(fixer, trace_[bug_index], bug_index);
        if (!res) {
            errs() << "\tFailed to fix!\n";
        } else {
            errs() << "\tFixed!\n";
        }

        modified = modified || res;
    }

    return modified;
}

#pragma endregion