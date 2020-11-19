#include "BugFixer.hpp"
#include "FlowAnalyzer.hpp"
#include "PassUtils.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace pmfix;
using namespace llvm;

cl::opt<bool> EnableHeuristicRaising("heuristic-raising", 
    cl::desc("Indicates whether or not enable heuristic raising."));

cl::opt<bool> ForceRaising("force[-raising", cl::init(false),
    cl::desc("Indicates whether or not to force heuristic raising (one level)."));

cl::opt<bool> DisableFixRaising("disable-raising", 
    cl::desc("Indicates whether or not to disable fix raising, which is what "
             "prevents flushes in memcpy/similar calls"));

cl::opt<bool> ExtraDumb("extra-dumb", cl::desc("Use dumb PM mem functions"));

cl::opt<std::string> SummaryFile("fix-summary-file", cl::init("fix_summary.txt"),
    cl::desc("Where to output the fix summary"));

cl::opt<bool> TraceAlias("trace-aa", cl::init(false),
    cl::desc("Use the trace based alias analysis instead of Andersen's"));

#pragma region BugFixer

bool BugFixer::addFixToMapping(const FixLoc &fl, FixDesc desc) {
    assert(fl.isValid() && "bad range!!");
    assert(desc.type > NO_FIX);

    if (!fixMap_.count(fl)) {
        fixMap_[fl] = desc;
        return true;
    } else if (fixMap_[fl] == desc) {
        return false;
    }

    if (fixMap_[fl].type == ADD_FLUSH_ONLY && desc.type == ADD_FENCE_ONLY) {
        fixMap_[fl].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[fl].type == ADD_FENCE_ONLY && desc.type == ADD_FLUSH_ONLY) {
        fixMap_[fl].type = ADD_FLUSH_AND_FENCE;
        return true;
    } else if (fixMap_[fl].type == ADD_FLUSH_AND_FENCE && 
               (desc.type == ADD_FLUSH_ONLY || desc.type == ADD_FENCE_ONLY)) {
        return false;
    } else if (desc.type == ADD_FLUSH_AND_FENCE && 
               (fixMap_[fl].type == ADD_FLUSH_ONLY || fixMap_[fl].type == ADD_FENCE_ONLY)) {
        fixMap_[fl].type = ADD_FLUSH_AND_FENCE;
        return true;
    }

    if ((fixMap_[fl].type == ADD_FLUSH_ONLY || fixMap_[fl].type == ADD_FLUSH_AND_FENCE) &&
        desc.type == REMOVE_FLUSH_ONLY) {
        assert(false && "conflicting solutions!");
    }

    if (fixMap_[fl].type == REMOVE_FLUSH_CONDITIONAL && 
        desc.type == REMOVE_FLUSH_CONDITIONAL) {

        fixMap_[fl].originals.insert(fixMap_[fl].originals.end(), 
            desc.originals.begin(), desc.originals.end());
        
        fixMap_[fl].points.insert(fixMap_[fl].points.end(),
            desc.points.begin(), desc.points.end());
        
        return true;
    }
 
    // errs() << "NEW: " << desc.type << " OLD: " << fixMap_[fl].type << "\n";

    assert(false && "what");

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
    AddressInfo addrInfo;
    std::list<AddressInfo> unadded;
    // errs() << "\t\tCHECK: " << te.addresses.front().str() << "\n";

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
                // if (!addrInfo.length)
                //     errs() << "STORE OVERLAP: " << addr.str() << "\n";
                if (addrInfo.canAdd(addr)) addrInfo += addr;
                else unadded.push_back(addr);

                while (!unadded.empty()) {
                    size_t sz = unadded.size();
                    bool added = false;
                    for (auto u = unadded.begin(); u != unadded.end(); ++u) {
                        if (addrInfo.canAdd(*u)) {
                            addrInfo += *u;
                            unadded.erase(u);
                            added = true;
                            break;
                        }
                    }

                    if (!added) break;
                    else {
                        assert(unadded.size() < sz && "INF");
                    }
                }

                opIndices.push_back(i);
                // This doesn't quite make sense to me, but I'll take it.
                // In theory, it should add up to be exact.
                if (addrInfo.contains(bugAddr)) {
                    // errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
                    // errs() << "\tCOMPLETE\n";
                    missingFlush = true;
                    break;
                } else {
                    // errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
                }
            } else if (event.type == TraceEvent::FLUSH &&
                        event.addresses.front().overlaps(te.addresses.front())) {
                assert(addr.isSingleCacheLine() && "don't know how to handle!");
                // errs() << "FLUSH: " << addr.str() << "\n";
                // errs() << "\tACCUMULATED: " << addrInfo.str() << "\n";
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

    // errs() << "\n\n";
    // errs() << "\t\tMissing Flush? : " << missingFlush << "\n";
    // errs() << "\t\tMissing Fence? : " << missingFence << "\n";
    // errs() << "\t\tNum Ops : " << opIndices.size() << "\n";

    assert(opIndices.size() && "Has to had been assigned at least!");

    bool added = false;
    // Foreach, if multiple stores need be fixed.
    for (int lastOpIndex : opIndices) {
        // Find where the last operation was.
        const TraceEvent &last = trace_[lastOpIndex];
        if (mapper_.contains(last.location)) {
            // errs() << "Fix direct!\n";
            // errs() << "\t\tLocation : " << last.location.str() << "\n";
            assert(mapper_[last.location].size() && "can't have no instructions!");
            for (const FixLoc &fLoc : mapper_[last.location]) {
                for (Instruction *i : fLoc.insts()) {
                    errs() << "\t\tInstruction : " << *i << "\n";
                    if (!isa<StoreInst>(i) && !isa<AtomicCmpXchgInst>(i)) {
                        // errs() << "\t\tNot a store instruction!\n";
                        continue;
                    }

                    FixLoc loc(i, i, fLoc.dbgLoc);
                    assert(loc.isValid());
                    errs() << "OG: " << fLoc.str() << "\n";
                    errs() << "CP: " << loc.str() << "\n";
                                
                    bool multiline = !last.addresses.front().isSingleCacheLine();
                    assert(!multiline && 
                            "Don't know how to handle multi-cache line operations!");
                    
                    bool res = false;
                    if (missingFlush && missingFence) {
                        res = addFixToMapping(loc, FixDesc(ADD_FLUSH_AND_FENCE, last.callstack));
                    } else if (missingFlush) {
                        res = addFixToMapping(loc, FixDesc(ADD_FLUSH_ONLY, last.callstack));
                    } else if (missingFence) {
                        res = addFixToMapping(loc, FixDesc(ADD_FENCE_ONLY, last.callstack));
                    }

                    // Have to do it this way, otherwise it short-circuits.
                    added = res || added;
                    // There MAY be multiple stores. So, don't break, run to end.
                    // break;
                }
                
            }
        } else {
            errs() << "Forced indirect fix!\n";
            for (const LocationInfo &li : last.callstack) {
                errs() << li.str() << " contains? " << mapper_.contains(li) << "\n";
            }
            // Here, we can take advantage of the persistent subprogram thing.
            auto desc = FixDesc(ADD_FLUSH_AND_FENCE, last.callstack);
            bool res = raiseFixLocation(FixLoc::NullLoc(), desc);
            // if (res) errs() << "\tAdded high-level fix!\n";
            // else errs() << "\tDid not add a fix!\n";
            added = res || added;
        }

    }

    // if (added) errs() << "-----------ADDED SOMEWHERE\n";
    // else errs() <<"------------NO FIX\n";
    
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

            /*
                Since we already check on the outside for multi-line flushes, we
                don't need to re-check here.

                Actually, we can likely be agnostic of size, since we will just
                wrap the operation in a conditional regardless.
            */
            // assert(event.addresses.front().isSingleCacheLine() && 
            //         "Don't know how to handle multi-cache line operations!");

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
                /**
                 * If the redundant store is not exactly equal, then we really
                 * can't do much, because we don't want to separate out the
                 * flush.
                 */
                if (redundantIdx == -1) {
                    errs() << "Only partially redundant--abort\n";
                    return false;
                }
                // Otherwise, we're good to go.
                originalIdx = i;
                break;
            }
        } 
    }

    errs() << "\tRedundant Index : " << redundantIdx << "\n";
    errs() << "\tOriginal Index : " << originalIdx << "\n";

    assert(redundantIdx >= 0 && "Has to have a redundant index!");

    /**
     * This can actually occur if there was no original store, IE we flushed
     * something that we never modified. I suppose we could pick the first event
     * in the program to condition on. For now, we can just skip.
     */
    if (originalIdx == -1) {
        errs() << "\t\tHard to condition on nothing, skip.\n";
        return false;
    }

    assert(originalIdx >= 0 && "Has to have a original index!");

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

    TraceEvent &orig = trace_[originalIdx];
    TraceEvent &redt = trace_[redundantIdx];

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
    if (!f.canAnalyze()) {
        errs() << "Cannot analyze, abort\n";
        return false;
    }

    errs() << "Always redundant? " << f.alwaysRedundant() << "\n";

    // Then we can just remove the redundant flush.
    bool res = false;
    for (auto &redtLoc : mapper_[redt.location]) {
        if (f.alwaysRedundant()) {
            res = addFixToMapping(redtLoc, FixDesc(REMOVE_FLUSH_ONLY, redt.callstack));
            errs() << "Always redundant! " << "\n";
        } else {
            std::list<Instruction*> redundantPaths = f.redundantPaths();

            if (redundantPaths.size()) {
                assert(mapper_[orig.location].size() > 0 && "can't handle!"); 
                
                for (const FixLoc &origLoc : mapper_[orig.location]) {
                    // Set dependent of the real fix
                    FixDesc remove(REMOVE_FLUSH_CONDITIONAL, redt.callstack, 
                        origLoc, redundantPaths);
                    bool ret = addFixToMapping(redtLoc, remove);
                    res = res || ret;
                }

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
            errs() << "Not doing perf fixes anymore!\n";
            return false;
            #if 0
            errs() << "\tPersistence Bug (Universal Performance)!\n";
            assert(te.addresses.size() > 0 &&
                "A redundant flush assertion needs an address!");
            assert(te.addresses.size() == 1 &&
                "A persist assertion should only have 1 address!");
            
            /**
             * If the flush is larger than a cache line, then it will likely 
             * be a black box operation that we can wrap our conditional around anyways.
             */

            // if (!te.addresses.front().isSingleCacheLine()) {
            //     errs() << "Skip this case (multi-cache line required flush), likely an artificial flush.\n";
            //     return false;
            // }
            // assert(te.addresses.front().isSingleCacheLine() &&
            //     "Don't know how to handle non-standard ranges which cross lines!");

            return handleRequiredFlush(te, bug_index);
            #endif
        }
        default: {
            errs() << "Not yet supported: " << te.typeString << "\n";
            return false;
        }
    }
}

bool BugFixer::fixBug(FixGenerator *fixer, const FixLoc &fl, const FixDesc &desc) {
    switch (desc.type) {
        case ADD_FLUSH_ONLY: {
            summary_ << summaryNum_ << ") ADD_FLUSH_ONLY:\n" << fl.str() << "\n";
            ++summaryNum_;

            Instruction *n = fixer->insertFlush(fl);
            assert(n && "could not ADD_FLUSH_ONLY");
            break;
        }
        case ADD_FENCE_ONLY: {
            summary_ << summaryNum_ << ") ADD_FENCE_ONLY:\n" << fl.str() << "\n";
            ++summaryNum_;

            Instruction *n = fixer->insertFence(fl);
            assert(n && "could not ADD_FENCE_ONLY");
            break;
        }
        case ADD_FLUSH_AND_FENCE: {
            summary_ << summaryNum_ << ") ADD_FLUSH_AND_FENCE:\n" << fl.str() << "\n";
            ++summaryNum_;

            Instruction *n = fixer->insertFlush(fl);
            assert(n && "could not add flush of FLUSH_AND_FENCE");
            n = fixer->insertFence(FixLoc(n, n));
            assert(n && "could not add fence of FLUSH_AND_FENCE");
            break;
        }
        case ADD_PERSIST_CALLSTACK_OPT: {
            summary_ << summaryNum_ << ") ADD_PERSISTENT_SUBPROGRAM:\n" << fl.str() << "\n";
            ++summaryNum_;

            Instruction *n = fixer->insertPersistentSubProgram(
                mapper_, fl, desc.dynStack, desc.stackIdx);
            if (!n) {
                errs() << "could not add persistent subprogram in ADD_PERSIST_CALLSTACK_OPT\n";
                return false;
            }
            
            // assert(n && "could not add persistent subprogram in ADD_PERSIST_CALLSTACK_OPT!");
            // n = fixer->insertFence(FixLoc(n, n));
            // assert(n && "could not add fence of ADD_PERSIST_CALLSTACK_OPT!");
            break;
        }
        case REMOVE_FLUSH_ONLY: {
            errs() << "Not doing perf fixes anymore!\n";
            return false;
            #if 0
            bool success = fixer->removeFlush(fl);
            assert(success && "could not remove flush of REMOVE_FLUSH_ONLY");
            break;
            #endif
        }
        case REMOVE_FLUSH_CONDITIONAL: {
            errs() << "Not doing perf fixes anymore!\n";
            return false;
            #if 0
            /**
             * We need to get all of the dependent fixes, add them, then
             * add the conditional wrapper. Fun.
             * 
             * We also need to reset the conditionals on the "original" store.
             */
            bool success = fixer->removeFlushConditionally(
                desc.originals, fl, desc.points);
            assert(success && 
                "could not conditionally remove flush of REMOVE_FLUSH_CONDITIONAL");
            break;
            #endif
        }
        default: {
            errs() << "UNSUPPORTED: " << desc.type << "\n";
            assert(false && "not handled!");
            break;
        }
    }

    return true;
}

bool BugFixer::raiseFixLocation(const FixLoc &fl, const FixDesc &desc) {
    bool raised = false;
    Instruction *startInst = fl.last;

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
     * 
     * Update:
     * - We need to correlate the fix location with the input arguments
     */
    const std::vector<LocationInfo> &stack = desc.dynStack;
    assert(!stack.empty() && "doesn't make sense!");
    
    const FixLoc *curr = nullptr;

    int heuristicIdx = 0;
    /**
     * For this, we want to find the point with the minimal number of volatile
     * aliases and maximum PM aliases.
     */
    if (EnableHeuristicRaising) {
        errs() << "\n\nHeuristic time!!!\n\n";
        // Now, we need to find the point of minimal aliasing
        // Score = pmAlias - volAlias
        std::vector<int64_t> scores;
        scores.resize(stack.size());

        const int64_t NO_ALIASES = INT64_MIN + 1;

        for (int l = 0; l < stack.size(); ++l) {
            auto &loc = stack[l];
            if (!mapper_.contains(loc)) {
                scores[l] = INT64_MIN;
                continue;
            }
            
            // int64_t volAlias = 0;
            // int64_t pmAlias = 0;

            // iangneal: We want unique aliases
            std::unordered_set<const llvm::Value *> volAlias, pmAlias;

            if (heuristicCache_.count(loc)) {
                // errs() << "H-Cache hit!\n";
                volAlias = heuristicCache_[loc].first;
                pmAlias = heuristicCache_[loc].second;

            } else {

                for (auto &fl : mapper_[loc]) {
                    for (Instruction *inst : fl.insts()) {

                        Instruction *i = inst;
                        if (TraceAlias) {
                            Value *v = vMap_[inst];
                            // if (!v) errs() << *i << "\n";
                            assert(v);
                            i = dyn_cast<Instruction>(v);
                            assert(i);
                        }

                        /** Skip conditions **/
                        if (auto *cb = dyn_cast<CallBase>(i)) {
                            Function *f = cb->getCalledFunction();

                            // Only skip if its a non-memcpy intrinsic
                            // if (f && !f->isDeclaration() && 
                            //     f->getIntrinsicID() == Intrinsic::not_intrinsic &&
                            //     ) {
                                // errs() << "CB SKIP" << *cb << "\n";
                            //     continue;
                            // }
                            if (f && f->getIntrinsicID() != Intrinsic::not_intrinsic
                                && f->getIntrinsicID() != Intrinsic::memset 
                                && f->getIntrinsicID() != Intrinsic::memcpy
                                && f->getIntrinsicID() != Intrinsic::memmove) {
                                continue;
                            }

                            // Function pointers are a bit evil to me at the 
                            // moment.
                            if (!f) {
                                errs() << "\t" << *i << "\n";
                                errs() << "Function pointer! ABORT ALL!\n";
                                volAlias.clear();
                                pmAlias.clear();
                                goto end;
                            }

                        } else if (!isa<StoreInst>(i) && 
                            !isa<AtomicCmpXchgInst>(i)) {
                            // errs() << "SKIP" << *i << "\n";
                            // assert(false);
                            continue;
                        }

                        errs() << "EXAMINE: " << *i << "\n";

                        for (auto iter = i->value_op_begin(); 
                            iter != i->value_op_end();
                            ++iter) {
                            Value *v = *iter;
                            assert(v);  
                            if (!v->getType()->isPointerTy() || isa<Function>(v)) {
                                // errs() << "NO POINTERS:" << *v << "\n";
                                continue;
                            }
                            errs() << "\tCheck " << *v << "\n";
                            
                            // Now, we need to figure out all the aliases.
                        
                            // errs() << "Made it!\n";
                            std::unordered_set<const llvm::Value *> ptsSet;
                            bool res = pmDesc_->getPointsToSet(v, ptsSet);
                            // assert(res);
                            if (ptsSet.empty() && !isa<Function>(v)) {
                                errs() << "\t\tNO PTS TO\n";
                                // errs() << "\t\tPoints? " << pmDesc_->pointsToPm(v) << "\n";
                                if (pmDesc_->pointsToPm(v)) pmAlias.insert(v);
                                else volAlias.insert(v);

                                errs() << loc.str() << "\t\t\t[" << l << "] VOL: " << volAlias.size() << " PM: " << pmAlias.size() << "\n";
                                continue;
                            } else if (ptsSet.empty()) {
                                errs() << loc.str() << " wut " << isa<Function>(v) << "\n";
                            }
                            // assert(!ptsSet.empty() && "can't make progress!");

                            size_t numPm = pmDesc_->getNumPmAliases(ptsSet);
                            size_t numVol = ptsSet.size() - numPm;

                            // // If it's all volatile, then it's irrelevant
                            // if (!numPm) {
                            //     // errs() << "ALL VOLATILE: " << *v << numVol << "\n";
                            //     continue;
                            // }  

                            errs() << "\tResult: " << *v << "; <" << numVol << ", " << numPm << ">\n";

                            // volAlias += numVol;
                            // pmAlias += numPm;

                            for (const auto val : ptsSet) {
                                if (pmDesc_->pointsToPm(const_cast<Value*>(val))) pmAlias.insert(val);
                                else volAlias.insert(val);
                            }
                            
                        }
                           
                    }
                }            
            }

            end:

            heuristicCache_[loc].first = volAlias;
            heuristicCache_[loc].second = pmAlias; 
            
            errs() << loc.str() << "\n[" << l << "] VOL: " << volAlias.size() << " PM: " << pmAlias.size() << "\n";
            // errs() << loc.str() << "\t[" << minIdx << "] VOL: " << minVolAlias << " PM: " << maxPmAlias << "\n";

            // Rebuttal: do we need this?
            //if (volAlias < minVolAlias && pmAlias >= maxPmAlias) {
            // if (volAlias < minVolAlias) {
            //     errs() << "\tUpdated!\n";
            //     minIdx = l;
            //     minVolAlias = volAlias;
            //     maxPmAlias = pmAlias;
            // }
            int64_t score = pmAlias.size() - volAlias.size();
            if (!pmAlias.size() && !volAlias.size()) scores[l] = NO_ALIASES;
            else scores[l] = score;
        }

        // Now, find the minIdx
        int64_t maxIdx = -1;
        int64_t maxScore = INT64_MIN;
        for (int l = 0; l < scores.size(); ++l) {
            if (scores[l] == NO_ALIASES) {
                errs() << "[" << l << "] Score: NO ALIAS, abort!\n";
                break;
            }

            errs() << "[" << l << "] Score: " << scores[l] << "\n";
            if (scores[l] > maxScore) {
                maxScore = scores[l];
                maxIdx = l;
            }
        } 
        
        assert(maxIdx >= 0 && "didn't do anything!");

        if (ForceRaising) {
            if (maxIdx == 0) {
                errs() << "ForceRaising: forced!\n";
            } else {
                errs() << "ForceRaising: not necessary!\n";
            }
        }

        heuristicIdx = maxIdx;
        if (heuristicIdx > 0) raised = true;
    }


    int idx = heuristicIdx;
    assert(idx >= 0 && "doesn't work otherwise!");

    // errs() << "idx=" << idx << " stacksz=" << stack.size() << "\n";

    while (idx < stack.size()) {
        if (!startInst && !mapper_.contains(stack[idx])) {
            errs() << "LI: " << stack[idx].str() << " NOT CONTAINED\n";
            raised = true;
            idx++;
            continue;
        }

        auto &fixLocList = mapper_[stack[idx]];
        if (fixLocList.size() > 1) {
            // Make sure they're all in the same function, cuz then it's fine.
            std::unordered_set<Function*> fns;
            for (auto &f : fixLocList) fns.insert(f.last->getFunction());
            assert(fns.size() == 1 && "don't know how to handle this weird code!");
        }
        
        curr = &fixLocList.front();
   
        Function *f = curr->last->getFunction();
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
        assert(curr && "cannot be null!");
        success = addFixToMapping(*curr, desc);
    }

    if (idx > heuristicIdx) {
        errs() << "Heuristic discrepancy!\n";
        for (const auto &li : desc.dynStack) {
            errs() << li.str() << "\n";
        }
        errs() << "H: " << heuristicIdx << "; N: " << idx << "\n";
    } else {
        errs() << "Heuristic consistent!\n";
        for (const auto &li : desc.dynStack) {
            errs() << li.str() << "\n";
        }
        errs() << "H: " << heuristicIdx << "; N: " << idx << "\n";
    }

    return success;
}

bool BugFixer::runFixMapOptimization(void) {
    std::list<FixLoc> moved;
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

    for (auto &fl : moved) {
        assert(fixMap_.erase(fl) && "couldn't remove!");
    }

    return res;
}

bool BugFixer::patchMemoryPrimitives(FixGenerator *fixer) {
    std::unordered_map<CallBase*, Function*> replace_map;

    for (Function &f : module_) {
        for (BasicBlock &b : f) {
            for (Instruction &i : b) {

                auto *cb = dyn_cast<CallBase>(&i);
                if (!cb) continue;

                Function *f = cb->getCalledFunction();
                if (!f) continue;

                switch (f->getIntrinsicID()) {
                    case Intrinsic::memcpy: {
                        Function *f = fixer->getPersistentVersion("memcpy");
                        if (ExtraDumb) f = fixer->getPersistentVersion("memcpy_dumb");
                        replace_map[cb] = f;
                        continue;
                    }
                    case Intrinsic::memset: {
                        Function *f = fixer->getPersistentVersion("memset");
                        if (ExtraDumb) f = fixer->getPersistentVersion("memset_dumb");
                        replace_map[cb] = f;
                        continue;
                    }
                    case Intrinsic::memmove: {
                        Function *f = fixer->getPersistentVersion("memmove");
                        if (ExtraDumb) f = fixer->getPersistentVersion("memmove_dumb");
                        replace_map[cb] = f;
                        continue;
                    }
                    default: {
                        break;
                    }
                }

                std::string fname(f->getName().data());

                if (std::string::npos != fname.find("movnt") || 
                    std::string::npos != fname.find("clflush") ||
                    std::string::npos != fname.find("clwb") ||
                    std::string::npos != fname.find("use_") || 
                    std::string::npos != fname.find("pmemops_")) {
                    continue;
                }

                if (cb->arg_size() < 3) continue;
                if (cb->getArgOperand(1)->getType()->isVectorTy()) continue;
                if (cb->getArgOperand(1)->getType()->isPointerTy()) continue;

                if (std::string::npos != fname.find("memcpy")) {
                    Function *f = fixer->getPersistentVersion("memcpy");
                    if (ExtraDumb) f = fixer->getPersistentVersion("memcpy_dumb");
                    replace_map[cb] = f;
                }

                if (std::string::npos != fname.find("memset")) {
                    Function *f = fixer->getPersistentVersion("memset");
                    if (ExtraDumb) f = fixer->getPersistentVersion("memset_dumb");
                    replace_map[cb] = f;
                }

                if (std::string::npos != fname.find("memmove")) {
                    Function *f = fixer->getPersistentVersion("memmove");
                    if (ExtraDumb) f = fixer->getPersistentVersion("memmove_dumb");
                    replace_map[cb] = f;
                }

                if (std::string::npos != fname.find("strncpy")) {
                    Function *f = fixer->getPersistentVersion("strncpy");
                    if (ExtraDumb) f = fixer->getPersistentVersion("strncpy_dumb");
                    replace_map[cb] = f;
                }
            }
        }
    }

    for (auto &p : replace_map) {
        (void)fixer->modifyCall(p.first, p.second);
        p.first->eraseFromParent();
    }

    errs() << "Changed " << replace_map.size() << " calls!\n";
    return !replace_map.empty();
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
            PMTestFixGenerator pmtestFixer(module_, pmDesc_.get(), &vMap_);
            fixer = &pmtestFixer;
            break;
        }
        case TraceEvent::GENERIC: {
            GenericFixGenerator genericFixer(module_, pmDesc_.get(), &vMap_);
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
     * Raise fixes if enabled.
     */
    if (!DisableFixRaising) {
        bool couldOpt = runFixMapOptimization();
        if (couldOpt) {
            errs() << "Was able to optimize!\n";
        } else {
            errs() << "Was not able to perform fix map optimizations!\n";
        }
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

    // errs() << *module_.getFunction("ulog_entry_val_create") << "\n";
    // errs() << *module_.getFunction("memset_mov_avx512f_empty")->

    /**
     * Step 2.
     * 
     * Patch primitives if we raising was not enabled.
     */
    if (DisableFixRaising) {
        bool patched = patchMemoryPrimitives(fixer);
        if (patched) {
            errs() << "Was able to patch primitives!\n";
        } else {
            errs() << "Was NOT able to patch primitives!\n";
        }
    }

    errs() << "Fixed " << nfixes << " of " << nbugs << " identified! (" 
        << trace_.bugs().size() << " in trace)\n";

    return modified;
}

const std::string BugFixer::immutableFnNames_[] = { "memset_mov_sse2_empty" };
// const std::string BugFixer::immutableFnNames_[] = {};
const std::string BugFixer::immutableLibNames_[] = {"libc.so"};

BugFixer::BugFixer(llvm::Module &m, TraceInfo &ti) 
    : module_(m), trace_(ti), mapper_(BugLocationMapper::getInstance(m)), 
      pmDesc_(nullptr), dupMod_(nullptr), summary_(SummaryFile.c_str()) {
    for (const std::string &fnName : immutableFnNames_) {
        addImmutableFunction(fnName);
    }

    for (const std::string &libName : immutableLibNames_) {
        addImmutableModule(libName);
    }

    if (EnableHeuristicRaising) {

        if (TraceAlias) {

            dupMod_ = llvm::CloneModule(module_, vMap_);

            // std::unordered_set<Value*> oVal, dVal, over;

            // for (Function &f : module_) {
            //     for (BasicBlock &b : f) {
            //         for (Instruction &i : b) {
            //             for (auto iter = i.value_op_begin(); 
            //                 iter != i.value_op_end();
            //                 ++iter) {
            //                 oVal.insert(*iter);
            //                 over.insert(*iter);
            //             }
            //         }
            //     }
            // }

            // for (Function &f : *dupMod_) {
            //     for (BasicBlock &b : f) {
            //         for (Instruction &i : b) {
            //             for (auto iter = i.value_op_begin(); 
            //                 iter != i.value_op_end();
            //                 ++iter) {
            //                 dVal.insert(*iter);
            //                 over.insert(*iter);
            //             }
            //         }
            //     }
            // }

            
            // errs() << "ORIG: " << oVal.size() << " DUP: " << dVal.size() << 
            //     " OVER: " << over.size() << " MAP:" << vMap_.size() <<"\n";

            // assert(false && "dumb");
            // errs() << "ORIG: " << &module_ << " DUP: " << dupMod_.get() << "\n";
            // for (auto iter = vMap_.begin(); iter != vMap_.end(); ++iter) {
            //     auto *f = dyn_cast<Instruction>(iter->first);
            //     auto *s = dyn_cast<Instruction>(iter->second);
            //     if (f && s) {
            //         errs() << "FIRST:" << f->getModule() << "\n";
            //         errs() << "SECOND:" << s->getModule() << "\n";
            //         break;
            //     }
            // }

            for (Function &f : module_) {
                for (BasicBlock &b : f) {
                    for (Instruction &i : b) {
                        assert(vMap_[&i] && "check");
                    }
                }
            }
            // assert(false && "dumb dumb");

            // Get all the functions used in the trace.
            unordered_set<Value*> used;
            for (const TraceEvent &te : trace_.events()) {
                for (const LocationInfo &li : te.callstack) {
                    if (!mapper_.contains(li)) continue;

                    for (const FixLoc &fl : mapper_[li]) {
                        if (fl.insts().empty()) continue;

                        Function *usedFn = fl.insts().front()->getFunction();
                        Value *remap = vMap_[usedFn];
                        assert(remap);
                        used.insert(remap);

                    }                    
                }
            }

            while (true) {
                errs() << "Currently " << used.size() << "\n";
                unordered_set<Value*> next;
                // Also add the users of the function as needed
                for (auto *v : used) {
                    for (auto *u : v->users()) {
                        auto *i = dyn_cast<Instruction>(u);
                        if (!i) continue;
                        Function *f = i->getFunction();
                        if (!f) continue;
                        if (used.count(f)) continue;
                        next.insert(f);
                    }

                    Instruction *i = dyn_cast<Instruction>(v);
                    if (!i) continue;

                    for (auto iter = i->value_op_begin(); iter != i->value_op_end(); ++iter) {
                        Value *v = *iter;
                        if (!v) continue;
                        Function *f = dyn_cast<Function>(v);
                        if (!f) continue;
                        if (used.count(f)) continue;
                        next.insert(f);
                        assert(false && "needed");
                    }
                }
                if (next.empty()) break;
                errs() << "\tAdding " << next.size() << '\n';

                used.insert(next.begin(), next.end());
            }

            list<Function*> toRemove;
            
            size_t nfn = 0;
            for (Function &f : *dupMod_) {
                nfn++;
                if (!used.count(&f)) toRemove.push_back(&f);
            }

            // for (Function &f : module_) {
            //     for (BasicBlock &b : f) {
            //         for (Instruction &i : b) {
            //             for (auto iter = i.value_op_begin(); 
            //                 iter != i.value_op_end();
            //                 ++iter) {
            //                 assert(vMap_.count(*iter) && "wut wut");
            //             }
            //         }
            //     }
            // }

            errs() << nfn << ", remove " << toRemove.size() << "!\n";

            for (Function *f : toRemove) {
                // Rather than actually removing the functions, we strip their
                // contents.
                f->deleteBody();
            }

            pmDesc_.reset(new PmDesc(*dupMod_));

            errs() << "analysis done!\n";

            // Set values
            for (auto &te : trace_.events()) {
                for (auto *val : te.pmValues(mapper_)) {
                    Value *v = vMap_[val];
                    // errs() << "PMV: " << *v << "\n";
                    assert(v && "can't be nullptr!");
                    pmDesc_->addKnownPmValue(v);

                    // assert(pmDesc_->pointsToPm(v));
                }    
            }

            errs() << "added pmv values!\n";
            errs() << pmDesc_->str() << "\n";

            // assert(false && "cool");

        } else {
            pmDesc_.reset(new PmDesc(module_));

            // Set values
            for (auto &te : trace_.events()) {
                for (auto *val : te.pmValues(mapper_)) {
                    pmDesc_->addKnownPmValue(val);
                }    
            }
        }
        // errs() << "scoping\n";
    }

    // errs() << "here?\n";
    // assert(false);

    // if (TraceAlias) {
    //     for (Function &f : module_) {
    //         for (BasicBlock &b : f) {
    //             for (Instruction &i : b) {
    //                 assert(vMap_[&i] && "check 2");
    //             }
    //         }
    //     }
    // }

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
