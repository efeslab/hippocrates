#include "BugFixer.hpp"
#include "FlowAnalyzer.hpp"
#include "PassUtils.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"

using namespace pmfix;
using namespace llvm;

#pragma region BugFixer

bool BugFixer::addFixToMapping(Instruction *i, FixDesc desc) {
    assert(desc.type > NO_FIX);

    if (!fixMap_.count(i)) {
        fixMap_[i] = desc;
        return true;
    } else if (fixMap_[i] == desc) {
        return false;
    }

    if (fixMap_[i].type == ADD_FLUSH_ONLY && desc.type == ADD_FENCE_ONLY) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[i].type == ADD_FENCE_ONLY && desc.type == ADD_FLUSH_ONLY) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[i].type == ADD_FLUSH_AND_FENCE && 
               (desc.type == ADD_FLUSH_ONLY || desc.type == ADD_FENCE_ONLY)) {
        return false;
    } else if (desc.type == ADD_FLUSH_AND_FENCE && 
               (fixMap_[i].type == ADD_FLUSH_ONLY || fixMap_[i].type == ADD_FENCE_ONLY)) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    }

    if ((fixMap_[i].type == ADD_FLUSH_ONLY || fixMap_[i].type == ADD_FLUSH_AND_FENCE) &&
        desc.type == REMOVE_FLUSH_ONLY) {
        assert(false && "conflicting solutions!");
    }

    // (iangneal): future work
    // if (fixMap_[i] == REMOVE_FLUSH_ONLY && t == REMOVE_FENCE_ONLY) {
    //     fixMap_[i] = REMOVE_FLUSH_AND_FENCE;
    //     return true;
    // } else if (fixMap_[i] == REMOVE_FENCE_ONLY && t == REMOVE_FLUSH_ONLY) {
    //     fixMap_[i] = REMOVE_FLUSH_AND_FENCE;
    //     return true;
    // }

 
    errs() << "NEW: " << desc.type << " OLD: " << fixMap_[i].type << "\n";

    assert(false && "what");
    return false;
}

bool BugFixer::addFixToMapping(Instruction *f, Instruction *l, FixDesc desc) {
    #if 0
    assert(desc.type > NO_FIX);


    if (!fixMap_.count(i)) {
        fixMap_[i] = desc;
        return true;
    } else if (fixMap_[i] == desc) {
        return false;
    }

    if (fixMap_[i].type == ADD_FLUSH_ONLY && desc.type == ADD_FENCE_ONLY) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[i].type == ADD_FENCE_ONLY && desc.type == ADD_FLUSH_ONLY) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[i].type == ADD_FLUSH_AND_FENCE && 
               (desc.type == ADD_FLUSH_ONLY || desc.type == ADD_FENCE_ONLY)) {
        return false;
    } else if (desc.type == ADD_FLUSH_AND_FENCE && 
               (fixMap_[i].type == ADD_FLUSH_ONLY || fixMap_[i].type == ADD_FENCE_ONLY)) {
        fixMap_[i].type = ADD_FLUSH_AND_FENCE;
        return true;
    }

    if ((fixMap_[i].type == ADD_FLUSH_ONLY || fixMap_[i].type == ADD_FLUSH_AND_FENCE) &&
        desc.type == REMOVE_FLUSH_ONLY) {
        assert(false && "conflicting solutions!");
    }
 
    errs() << "NEW: " << desc.type << " OLD: " << fixMap_[i].type << "\n";

    assert(false && "what");
    #endif
    return false;
}

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
bool BugFixer::handleAssertPersisted(const TraceEvent &te, int bug_index) {
    bool missingFlush = false;
    bool missingFence = true;
    // Need this so we know where the eventual fixes will go.
    std::list<int> opIndices;
    // For cumulative stores.
    struct AddressInfo addrInfo;
    errs() << "\t\tCHECK: " << te.addresses.front().str() << "\n";

    /**
     * This can be larger than a cacheline, as it can be a bug report at the
     * end of a program.
     */
    auto &bugAddr = te.addresses.front();

    // First, determine which case we are in by going backwards.
    for (int i = bug_index - 1; i >= 0; i--) {
        const TraceEvent &event = trace_[i];
        if (!event.isOperation()) continue;

        assert(event.addresses.size() <= 1 && 
                "Don't know how to handle more addresses!");
        if (event.addresses.size()) {
            auto &addr = event.addresses.front();

            // if (event.type == TraceEvent::STORE && addr == bugAddr) {
            //     assert(addr.isSingleCacheLine() && "don't know how to handle!");
            //     /* This is the easy case, it's clear that none of the range was
            //     persisted. */
            //     errs() << "STORE: " << addr.str() << "\n";
            //     missingFlush = true;
            //     opIndices.push_back(i);
            //     break;
            // } else 
            if (event.type == TraceEvent::STORE && addr.overlaps(bugAddr)) {
                // assert(addr.isSingleCacheLine() && "don't know how to handle!");
                /* In this case, we need to validate that there are a bunch of stores that
                    when summed together */
                if (!addrInfo.length)
                    errs() << "STORE OVERLAP: " << addr.str() << "\n";
                addrInfo += addr;
                opIndices.push_back(i);
                // This doesn't quite make sense to me, but I'll take it.
                // In theory, it should add up to be exact.
                if (addrInfo.contains(bugAddr)) {
                    errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
                    errs() << "\tCOMPLETE\n";
                    missingFlush = true;
                    break;
                } else {
                    // errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
                }
            } else if (event.type == TraceEvent::FLUSH &&
                        event.addresses.front().overlaps(te.addresses.front())) {
                assert(addr.isSingleCacheLine() && "don't know how to handle!");
                errs() << "FLUSH: " << addr.str() << "\n";
                errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
                assert(missingFence == true &&
                        "Shouldn't be a bug in this case, has flush and fence");
                opIndices.push_back(i);
                break;
            }
        } else if (event.type == TraceEvent::FENCE) {
            // errs() << "FENCE\n";
            missingFence = false;
            missingFlush = true;
        }
    }

    errs() << "\t\tMissing Flush? : " << missingFlush << "\n";
    errs() << "\t\tMissing Fence? : " << missingFence << "\n";
    errs() << "\t\tNum Ops : " << opIndices.size() << "\n";

    assert(opIndices.size() && "Has to had been assigned at least!");

    bool added = false;
    // Foreach, if multiple stores need be fixed.
    for (int lastOpIndex : opIndices) {
        // Find where the last operation was.
        const TraceEvent &last = trace_[lastOpIndex];
        if (mapper_.contains(last.location)) {
            errs() << "Fix direct!\n";
            errs() << "\t\tLocation : " << last.location.str() << "\n";
            assert(mapper_[last.location].size() && "can't have no instructions!");
            for (Instruction *i : mapper_[last.location]) {
                assert(i && "can't be null!");
                errs() << "\t\tInstruction : " << *i << "\n";
                            
                bool multiline = !last.addresses.front().isSingleCacheLine();
                assert(!multiline && 
                        "Don't know how to handle multi-cache line operations!");
                
                bool res = false;
                if (missingFlush && missingFence) {
                    res = addFixToMapping(i, FixDesc(ADD_FLUSH_AND_FENCE, last.callstack));
                } else if (missingFlush) {
                    res = addFixToMapping(i, FixDesc(ADD_FLUSH_ONLY, last.callstack));
                } else if (missingFence) {
                    res = addFixToMapping(i, FixDesc(ADD_FENCE_ONLY, last.callstack));
                }

                // Have to do it this way, otherwise it short-circuits.
                added = res || added;
            }
        } else {
            errs() << "Forced indirect fix!\n";
            for (const LocationInfo &li : last.callstack) {
                errs() << li.str() << " contains? " << mapper_.contains(li) << "\n";
            }
            // Here, we can take advantage of the persistent subprogram thing.
            auto desc = FixDesc(ADD_FLUSH_AND_FENCE, last.callstack);
            bool res = raiseFixLocation(nullptr, desc);
            if (res) errs() << "\tAdded high-level fix!\n";
            else errs() << "\tDid not add a fix!\n";
            added = res || added;
        }

    }

    if (added) errs() << "-----------ADDED SOMEWHERE\n";
    else errs() <<"------------NO FIX\n";
    
    return added;      
}

bool BugFixer::handleAssertOrdered(const TraceEvent &te, int bug_index) {
    errs() << "\tTODO: implement " << __FUNCTION__ << "!\n";
    return false;
}

bool BugFixer::handleRequiredFlush(const TraceEvent &te, int bug_index) {
    /**
     * Step 1: find the redundant flush and the original flush.
     */

    int redundantIdx = -1;
    int originalIdx = -1;

    for (int i = bug_index - 1; i >= 0; i--) {
        if (redundantIdx != -1 && originalIdx != -1) break;
        const TraceEvent &event = trace_[i];
        if (!event.isOperation()) continue;

        assert(event.addresses.size() <= 1 && 
                "Don't know how to handle more addresses!");
        if (event.addresses.size()) {
            errs() << "IDX: " << i << "\n";
            errs() << "EVENT: " << event.typeString << "\n";
            errs() << "Address: " << event.addresses.front().address << "\n";
            errs() << "Length:  " << event.addresses.front().length << "\n";
            assert(event.addresses.front().isSingleCacheLine() && 
                    "Don't know how to handle multi-cache line operations!");

            if (event.type == TraceEvent::FLUSH &&
                event.addresses.front() == te.addresses.front()) {
                if (redundantIdx == -1) {
                    errs() << "\tfilled redt!\n";
                    redundantIdx = i;
                } else {
                    errs() << "\tfilled orig!\n";
                    originalIdx = i;
                    break;
                }
            } else if (event.type == TraceEvent::FLUSH &&
                        event.addresses.front().overlaps(te.addresses.front())) {
                assert(false && "wat");
                break;
            }
        } 
    }

    errs() << "\tRedundant Index : " << redundantIdx << "\n";
    errs() << "\tOriginal Index : " << originalIdx << "\n";

    assert(redundantIdx >= 0 && "Has to have a redundant index!");
    assert(originalIdx >= 0 && "Has to have a redundant index!");

    /**
     * Step 2: Figure out how we can fix this bug.
     * 
     * Options:
     *  - If the two flushes are in the same function context...
     *      - If the redundant is dominated by the original, delete the redundant.
     *      - If the redundant post-dominates the original, delete the original.
     *      - If there is a common post-dominator, delete both and insert in the
     *        common post-dominator.
     * 
     *  - If the two flushes are NOT in the same function context, then we have
     *  to do some complicated crap.
     *      - TODO
     * 
     * Otherwise, abort.
     */

    const TraceEvent &orig = trace_[originalIdx];
    const TraceEvent &redt = trace_[redundantIdx];

    errs() << "Original: " << orig.str() << "\n";
    errs() << "Redundant: " << redt.str() << "\n";

    // Are they in the same context?
    if (TraceEvent::callStacksEqual(orig, redt)) {
        errs() << "\teq stack!\n";
    } else {
        errs() << "\tneq!!\n";
    }

    // ContextGraph<bool> graph(mapper_, orig, redt);
    FlowAnalyzer f(module_, mapper_, orig, redt);
    errs() << "Always redundant? " << f.alwaysRedundant() << "\n";

    // Then we can just remove the redundant flush.
    bool res = false;
    for (Instruction *i : mapper_[redt.location]) {
        if (f.alwaysRedundant()) {
            res = addFixToMapping(i, FixDesc(REMOVE_FLUSH_ONLY, redt.callstack));
            errs() << "Always redundant! " << "\n";
        } else {
            std::list<Instruction*> redundantPaths = f.redundantPaths();

            if (redundantPaths.size()) {
                assert(mapper_[orig.location].size() == 1 && "can't handle!");
                Instruction *originalInst = mapper_[orig.location].front();
                // Set dependent of the real fix
                FixDesc remove(REMOVE_FLUSH_CONDITIONAL, redt.callstack, 
                    originalInst, redundantPaths);
                bool ret = addFixToMapping(i, remove);
                res = res || ret;
            } else {
                errs() << "No paths on which to fix!!!" << "\n";
            }
        }
    }

    return res;
}

bool BugFixer::computeAndAddFix(const TraceEvent &te, int bug_index) {
    assert(te.isBug && "Can't fix a not-a-bug!");

    switch(te.type) {
        case TraceEvent::ASSERT_PERSISTED: {
            errs() << "\tPersistence Bug (Universal Correctness)!\n";
            assert(te.addresses.size() == 1 &&
                "A persist assertion should only have 1 address!");
            return handleAssertPersisted(te, bug_index);
        }
        case TraceEvent::REQUIRED_FLUSH: {
            errs() << "\tPersistence Bug (Universal Performance)!\n";
            assert(te.addresses.size() > 0 &&
                "A redundant flush assertion needs an address!");
            assert(te.addresses.size() == 1 &&
                "A persist assertion should only have 1 address!");
            assert(te.addresses.front().isSingleCacheLine() &&
                "Don't know how to handle non-standard ranges which cross lines!");
            return handleRequiredFlush(te, bug_index);
        }
        default: {
            errs() << "Not yet supported: " << te.typeString << "\n";
            return false;
        }
    }
}

bool BugFixer::fixBug(FixGenerator *fixer, Instruction *i, FixDesc desc) {
    switch (desc.type) {
        case ADD_FLUSH_ONLY: {
            Instruction *n = fixer->insertFlush(i);
            assert(n && "could not ADD_FLUSH_ONLY");
            break;
        }
        case ADD_FENCE_ONLY: {
            Instruction *n = fixer->insertFence(i);
            assert(n && "could not ADD_FENCE_ONLY");
            break;
        }
        case ADD_FLUSH_AND_FENCE: {
            Instruction *n = fixer->insertFlush(i);
            assert(n && "could not add flush of FLUSH_AND_FENCE");
            n = fixer->insertFence(n);
            assert(n && "could not add fence of FLUSH_AND_FENCE");
            break;
        }
        case ADD_PERSIST_CALLSTACK_OPT: {
            Instruction *n = fixer->insertPersistentSubProgram(
                mapper_, i, *desc.dynStack, desc.stackIdx);
            assert(n && "could not add persistent subprogram in ADD_PERSIST_CALLSTACK_OPT!");
            n = fixer->insertFence(n);
            assert(n && "could not add fence of ADD_PERSIST_CALLSTACK_OPT!");
            break;
        }
        case REMOVE_FLUSH_ONLY: {
            bool success = fixer->removeFlush(i);
            assert(success && "could not remove flush of REMOVE_FLUSH_ONLY");
            break;
        }
        case REMOVE_FLUSH_CONDITIONAL: {
            /**
             * We need to get all of the dependent fixes, add them, then
             * add the conditional wrapper. Fun.
             * 
             * We also need to reset the conditionals on the "original" store.
             */
            bool success = fixer->removeFlushConditionally(
                desc.original, i, desc.points);
            assert(success && 
                "could not conditionally remove flush of REMOVE_FLUSH_CONDITIONAL");
            break;
        }
        default: {
            errs() << "UNSUPPORTED: " << desc.type << "\n";
            assert(false && "not handled!");
            break;
        }
    }

    return true;
}

bool BugFixer::raiseFixLocation(llvm::Instruction *startInst, const FixDesc &desc) {
    bool raised = false;

    /**
     * We raise in two circumstances: if the instruction is in an "immutable"
     * function, or if it's heuristically good to raise the fix. 
     * 
     * Optimization 1: avoid "immutable" functions. This is both explicit and
     * implicit (i.e. function not linked, like libc).
     * 
     * Optimization 2: For all the fixes, see if we should raise any 
     * heuristically. 
     * 
     * For now, we'll see what "min-aliases" gives us. Likely, 
     * too high.
     * - We'll need all the PM values in the program. We want to know the number
     * of overall aliases and the number which can be PM values.
     */
    const std::vector<LocationInfo> &stack = *desc.dynStack;
    int idx = 0;
    Instruction *curr = nullptr;

    while (idx < stack.size()) {
        if (!startInst && !mapper_.contains(stack[idx])) {
            errs() << "LI: " << stack[idx].str() << " NOT CONTAINED\n";
            raised = true;
            idx++;
            continue;
        }

        auto &instList = mapper_[stack[idx]];
        if (instList.size() > 1) {
            // Make sure they're all in the same function, cuz then it's fine.
            std::unordered_set<Function*> fns;
            for (auto *i : instList) fns.insert(i->getFunction());
            assert(fns.size() == 1 && "don't know how to handle this weird code!");
        }
        
        curr = instList.front();

        
        Function *f = curr->getFunction();
        if (immutableFns_.count(f)) {
            // Optimization 1: If it is immutable.
            errs() << "LI: " << stack[idx].str() << " RAISING ABOVE IMMUTABLE\n";
            raised = true;
            idx++;
        } else {
            errs() << "LI: " << stack[idx].str() << " NOW HAS THE FIX\n";
            break;
        }
    }

    bool success = false;
    if (raised) {
        auto desc = FixDesc(ADD_PERSIST_CALLSTACK_OPT, stack, idx);
        success = addFixToMapping(curr, desc);
    }

    return success;
}

bool BugFixer::runFixMapOptimization(void) {
    std::list<Instruction*> moved;
    bool res = false;
    
    for (auto &p : fixMap_) {
        bool applies = (p.second.type == ADD_FLUSH_ONLY || 
                        p.second.type == ADD_FENCE_ONLY || 
                        p.second.type == ADD_FLUSH_AND_FENCE);
        if (!applies) continue;

        bool success = raiseFixLocation(p.first, p.second);
        res = res || success;
        // Erase original fix if successful
        if (success) moved.push_back(p.first);
    }

    for (auto *i : moved) {
        assert(fixMap_.erase(i) && "couldn't remove!");
    }

    return res;
}

bool BugFixer::doRepair(void) {
    bool modified = false;

    /**
     * Select the bug fixer based on the source of the bug report. Mostly 
     * differentiates between tools which require assertions (PMTEST) and 
     * everything else.
     */
    FixGenerator *fixer = nullptr;
    switch (trace_.getSource()) {
        case TraceEvent::PMTEST: {
            PMTestFixGenerator pmtestFixer(module_);
            fixer = &pmtestFixer;
            break;
        }
        case TraceEvent::GENERIC: {
            GenericFixGenerator genericFixer(module_);
            fixer = &genericFixer;
            break;
        }
        default: {
            errs() << "unsupported!\n";
            exit(-1);
        }
    }

    /**
     * Step 1.
     * 
     * Now, we find all the fixes.
     */
    for (int bug_index : trace_.bugs()) {
        errs() << "Bug Index: " << bug_index << "\n";
        bool addedFix = computeAndAddFix(trace_[bug_index], bug_index);
        if (addedFix) {
            errs() << "\tAdded a fix!\n";
        } else {
            errs() << "\tDid not add a fix!\n";
        }
    }

    /**
     * Step 2.
     * 
     * TODO: Raise fixes.
     */
    bool couldOpt = runFixMapOptimization();
    if (couldOpt) {
        errs() << "Was able to optimize!\n";
    } else {
        errs() << "Was not able to perform fix map optimizations!\n";
    }

    /**
     * Step 3.
     * 
     * The final step. Now, we actually do the fixing.
     */
    size_t nbugs = 0;
    size_t nfixes = 0;
    for (auto &p : fixMap_) {
        bool res = fixBug(fixer, p.first, p.second);
        modified = modified || res;
        nbugs += 1;
        nfixes += (res ? 1 : 0);
    }

    errs() << "Fixed " << nfixes << " of " << nbugs << " identified! (" 
        << trace_.bugs().size() << " in trace)\n";

    return modified;
}

const std::string BugFixer::immutableFnNames_[] = { "memset_mov_sse2_empty" };
// const std::string BugFixer::immutableFnNames_[] = {};
const std::string BugFixer::immutableLibNames_[] = {"libc.so"};

BugFixer::BugFixer(llvm::Module &m, TraceInfo &ti) 
    : module_(m), trace_(ti), mapper_(m) {
    for (const std::string &fnName : immutableFnNames_) {
        addImmutableFunction(fnName);
    }

    for (const std::string &libName : immutableLibNames_) {
        addImmutableModule(libName);
    }
}

void BugFixer::addImmutableFunction(const std::string &fnName) {
    Function *f = module_.getFunction(fnName);

    if (f) {
        immutableFns_.insert(f);
    } else {
        errs() << "Could not find function " << fnName << ", skipping\n";
    }
}

void BugFixer::addImmutableModule(const std::string &modName) {
    for (Function &f : module_) {
        auto *metadata = f.getMetadata("dbg");
        // if (metadata) errs() << "Metadata: " << *metadata << "\n";
        // else {
        //     errs() << "Metadata: NULLPTR\n";
        //     SmallVector<StringRef,100> Result;
        //     module_.getMDKindNames(Result);
        //     for (auto &name : Result) errs() << "KIND " << name << "\n";
        //     assert(false);
        // }
        if (!metadata) {
            // If we don't have debug info, then don't modify anyways.
            immutableFns_.insert(&f);
            continue;
        }

        if (auto *ds = dyn_cast<DISubprogram>(metadata)) {
            DIFile *df = ds->getFile();
            std::string fileName = df->getFilename().data();
            if (fileName.find(modName) != std::string::npos) {
                assert(false && "implement!");
            }
        }
    }
}

#pragma endregion
