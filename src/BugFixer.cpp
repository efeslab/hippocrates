#include "BugFixer.hpp"

using namespace pmfix;
using namespace llvm;

#pragma region FixGenerators

bool PMTestFixGenerator::insertFlush(llvm::Instruction *i) {

    return false;
}

#pragma endregion

#pragma region BugFixer

bool BugFixer::fixBug(const TraceEvent &te, int bug_index) {
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

    } else {
        errs() << "Not yet supported: " << te.typeString << "\n";
        return false;
    }

    errs() << "Fallthrough!!!\n";
    return false;
}

bool BugFixer::doRepair(void) {
    bool modified = false;

    for (int bug_index : trace_.bugs()) {
        errs() << "Bug Index: " << bug_index << "\n";
        bool res = fixBug(trace_[bug_index], bug_index);
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