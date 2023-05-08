#include "FixGenerator.hpp"

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/CFG.h"

#include "llvm/IR/DIBuilder.h"

#include "PassUtils.hpp"

#include <unordered_set>

using namespace pmfix;
using namespace llvm;

#pragma region FixGenerator

cl::opt<bool> UseNT("use-nt", 
    cl::desc("Indicates whether or not to use NT stores for persistent subprograms."));

extern cl::opt<bool> TraceAlias;
extern cl::opt<bool> ReducedAlias;

llvm::Function *FixGenerator::getClwbDefinition() const {
    // Function *clwb = Intrinsic::getDeclaration(&module_, Intrinsic::x86_clwb, {ptrTy});
    // -- the above appends extra type specifiers that cause it not to generate.
    Function *clwb = Intrinsic::getDeclaration(&module_, Intrinsic::x86_clwb);
    assert(clwb && "could not find clwb!");
    return clwb;
}

llvm::Function *FixGenerator::getSfenceDefinition() const {
    // Function *sfence = module_.getFunction("llvm.x86.sse.sfence");
    Function *sfence = Intrinsic::getDeclaration(&module_, Intrinsic::x86_sse_sfence);
    assert(sfence && "could not find sfence!");
    return sfence;
}

llvm::Function *FixGenerator::getPersistentIntrinsic(const char *name) const {
    Function *fn = module_.getFunction(name);
    if (!fn) {
        errs() << __FUNCTION__ << ": PMFIXER(" << name << ") not found!\n";
    }
    assert(fn && "Could not find persistent intrinsic! Likely forgot to link with intrinsics library.");
    return fn;
}

llvm::Function *FixGenerator::getPersistentVersion(const char *name) const {
    static std::string prefix("PMFIXER_");
    auto fullName = prefix + std::string(name);
    return getPersistentIntrinsic(fullName.c_str());
}

llvm::Function *FixGenerator::getPersistentMemcpy() const {
    return getPersistentIntrinsic("PMFIXER_memcpy");
}

llvm::Function *FixGenerator::getPersistentMemset() const {
    return getPersistentIntrinsic("PMFIXER_memset");
}

llvm::Function *FixGenerator::getPersistentMemmove() const {
    return getPersistentIntrinsic("PMFIXER_memmove");
}

GlobalVariable *FixGenerator::createConditionVariable(
    const std::list<llvm::Instruction*> &resetBefore, 
    Instruction *setAt) {
    
    auto *boolType = Type::getInt1Ty(module_.getContext());

    GlobalVariable *gv = new GlobalVariable(
        /* Module */ module_, /* Type */ boolType, /* isConstant */ false,
        /* Linkage */ GlobalValue::ExternalLinkage, 
        /* Constant constructor */ Constant::getNullValue(boolType),
        /* Name */ "removeCondition", /* Insert before */ nullptr,
        /* Thread local? */ GlobalValue::LocalExecTLSModel,
        /* AddressSpace */ 0,
        /* Externally init? */ false);

    for (auto *resetB : resetBefore) {
        IRBuilder<> builder(resetB);
        // The reset
        auto *resetInst = builder.CreateStore(Constant::getNullValue(boolType), gv, "ResetAtStart");
        errs() << "reset:" << *resetInst << "\n";
    }

    // The set
    IRBuilder<> builder(setAt);
    auto *setInst = builder.CreateStore(Constant::getAllOnesValue(boolType), gv, "SetInPath");
    errs() << "set:" << *setInst << "\n";

    return gv;
}

llvm::Instruction *FixGenerator::createConditionalBlock(
    llvm::Instruction *first, 
    llvm::Instruction *end,
    const std::list<llvm::GlobalVariable*> &conditions) {
    errs() << "first:" << *first << "\n";
   
   /**
    * We want to wrap instructions from [first, end] in a conditional block.
    * So, we need to create two new basic blocks.
    */

    BasicBlock *newRegion = first->getParent()->splitBasicBlock(first);
    newRegion->setName("TheFlushBlock");
    BasicBlock *endRegion = nullptr;
    if (Instruction *i = end->getNextNonDebugInstruction()) {
        endRegion = newRegion->splitBasicBlock(i);
    } else if (!succ_empty(end->getParent())) {
        assert(end->isTerminator());
        endRegion = end->getParent()->getSingleSuccessor();
        assert(endRegion);
    } else {
        errs() << "ERR:" << *end << "\n";
        assert(false && "don't know how to handle! is end a ret?");
    }

    endRegion->setName("TheEndBlock");
    /**
     * We want to get the last branch in the original basic block, because we
     * need to eventually replace it with a conditional branch.
     */
    BasicBlock *originalBB = newRegion->getUniquePredecessor();
    assert(originalBB);
    originalBB->setName("TheCommonPredecessor");

    /**
     * Okay, now we need to construct the actual conditional, then insert 
     * the conditional branch, and delete the unconditional branch to the newRegion.
     */

    // Set up the inserter.
    Instruction *oldTerm = originalBB->getTerminator();
    assert(isa<BranchInst>(oldTerm));
    IRBuilder<> builder(oldTerm);

    // We should go to the new region (with the flush) if all are 0.
    // -- That means it got reset and never set.
    // -- To check all zero, OR. If true, go to end.
    // Insert all the cascaded ORs
    auto *boolType = Type::getInt1Ty(module_.getContext());
    Value *prev = Constant::getNullValue(boolType);
    for (auto *gv : conditions) {
        // Load from gv, then and
        auto *loadRes = builder.CreateLoad(boolType, gv, /*isvolatile*/ true);
        prev = builder.CreateOr(prev, loadRes);
    }

    // Now, make a conditional branch.
    builder.CreateCondBr(prev, endRegion, newRegion);

    // Finally, erase old unconditional branch
    oldTerm->eraseFromParent();

    // Make sure to reset the variables after we skip once.
    builder.SetInsertPoint(endRegion->getFirstNonPHIOrDbgOrLifetime());
    for (auto *gv : conditions) {
        auto *resetInst = builder.CreateStore(Constant::getNullValue(boolType), gv, "PostSkipReset");
        errs() << "PostSkipReset:" << *resetInst << "\n";
        assert(resetInst);
    }

    return &newRegion->front();
}

Function *FixGenerator::duplicateFunction(
    Function *f, ValueToValueMapTy &vmap, std::string postFix) {

    Function *fNew = llvm::CloneFunction(f, vmap);
    fNew->setName(f->getName() + postFix);
    assert(fNew && "what");
    // for (auto &bb : *f) errs() << "Old " << &bb << "\n";
    // for (auto &bb : *fNew) errs() << "New " << &bb << "\n";
    // errs() << *f << "\n";
    // errs() << *fNew << "\n";
    return fNew;
}

bool FixGenerator::makeAllStoresPersistent(
    llvm::Function *oldF, llvm::Function *newF, const ValueToValueMapTy &vmap) {

    // errs() << "NEW FUNCTION PM: " << newF->getName() << "\n";
    // errs() << "OLD FUNCTION PM: " << oldF->getName() << "\n";
    std::list<Instruction*> flushPoints;
    // std::list<ReturnInst*> fencePoints;

    /**
     * Iterate through the old function, find PM aliases, then map to the new
     * function to add to the flush points.
     */
    for (BasicBlock &bb : *oldF) {
        for (Instruction &i : bb) {
            #if 0
            if (auto *cb = dyn_cast<CallBase>(&i)) {
                // Function *f = cb->getCalledFunction();
                // if (!f) continue;
                // if (f->getIntrinsicID() == Intrinsic::dbg_declare) continue;
                // if (f->getName().find("_NT") != StringRef::npos) continue;

                // recursePoints.push_back(cb);
                
                // errs() << "REC:" << *cb << "\n";
                // errs() << "FN:" << *f << "\n";
                // assert(false && "not supported yet!");
                // This should be okay. Just have to go down to the level that
                // the error occurs at.
            } else if (auto *ri = dyn_cast<ReturnInst>(&i)) {
                // Insert a sfence in front of the return.
                fencePoints.push_back(ri);
            } else 
            #endif

            Value *ptrOp = nullptr;
            if (auto *si = dyn_cast<StoreInst>(&i)) {
                ptrOp = si->getPointerOperand();
            } else if (auto *cx = dyn_cast<AtomicCmpXchgInst>(&i)) {
                ptrOp = cx->getPointerOperand();
            }

            if (TraceAlias || ReducedAlias) {
                ptrOp = (*traceAAMap_)[ptrOp];
            }
            
            if (ptrOp) {
                /**
                 * Figure out if the store is to a stack variable. If so, we
                 * really don't want to add it.
                 */
                if (isa<AllocaInst>(ptrOp)) continue;
                #if 1
                // Also figure out if the pointer operand points to PM or not.
                if (!pmDesc_->contains(ptrOp)) {
                    // errs() << "DOES NOT CONTAIN: " << *ptrOp << "\n";
                    // std::unordered_set<const llvm::Value *> ptsSet;
                    // bool res = pmDesc_->getPointsToSet(ptrOp, ptsSet);
                    // errs() << "??? " << res << " " << pmDesc_->getNumPmAliases(ptsSet) << "\n";
                } else if (pmDesc_->pointsToPm(ptrOp)) {
                    auto *ninst = dyn_cast<Instruction>(vmap.lookup(&i));
                    assert(ninst && "wat");
                    flushPoints.push_back(ninst);
                    // errs() << "POINTS: " << *ptrOp << "\n";
                } else {
                    // errs() << "DOES NOT POINT: " << *ptrOp << "\n";
                }
                #else 
                flushPoints.push_back(si);
                #endif
            }
        }
    }

    for (auto *i : flushPoints) {
        // Either insert a flush or make the store non-temporal.
        if (UseNT) {
            // Set non-temporal metadata
            DIExpression *die = DIBuilder(module_).createConstantValueExpression(1);
            errs() << "MD:" << *die << "\n";
            i->setMetadata(LLVMContext::MD_nontemporal, die);
            errs() << "NT:" << *i << "\n";
            /**
             * TODO: Extend this to PMTest as well.
             */
            Function *valFlush = getPersistentVersion("valgrind_flush");
            assert(valFlush && "can't mark NT stores as flushed!");
            IRBuilder<> builder(i->getNextNode());
            // Get the right pointer
            // -- dest
            Value *ptr = nullptr;
            Value *valOp = nullptr;
            if (auto *si = dyn_cast<StoreInst>(i)) {
                ptr = si->getPointerOperand();
                valOp = si->getValueOperand();
            } else if (auto *cx = dyn_cast<AtomicCmpXchgInst>(i)) {
                ptr = cx->getPointerOperand();
                valOp = cx->getNewValOperand();
            }

            Type *ptrDestTy = valFlush->arg_begin()->getType();
            Value *ptrOp = builder.CreatePointerCast(ptr, ptrDestTy);
            // Get the length argument as well.
            // -- dest
            Type *szDestTy = (valFlush->arg_begin() + 1)->getType();
            // -- Value
            size_t nbits = 0;
            Type *storedValTy = valOp->getType();
            if (storedValTy->isIntegerTy()) {
                nbits = storedValTy->getScalarSizeInBits();
            } else if (storedValTy->isPointerTy()) {
                nbits = module_.getDataLayout().getMaxPointerSize();
            }

            if (!nbits) {
                errs() << "TYPE: " << *storedValTy << "\n";
                errs() << storedValTy->isStructTy() << "\n";
            }

            assert(nbits > 0);
            APInt flushLen(szDestTy->getScalarSizeInBits(), nbits / 8);
            Value *lenOp = Constant::getIntegerValue(szDestTy, flushLen);
            
            builder.CreateCall(valFlush, {ptrOp, lenOp});
        } else {
            // Insert flush
            errs() << "Flushing" << *i << " in " << i->getFunction()->getName() << "\n";
            auto *ni = insertFlush(i);
            assert(ni && "unable to insert flush!");
        }
    }

    if (flushPoints.empty()) {
        errs() << "No flush points!\n";
        // This is not necessarily an error, it just means this function doesn't
        // have any direct stores.
        // return false;
    }

    #if 0
    for (auto *ri : fencePoints) {
        Instruction *insertAt = ri->getPrevNonDebugInstruction();
        if (!insertAt) {
            IRBuilder<> builder(ri);
            insertAt = builder.CreateCall(Intrinsic::getDeclaration(&module_, Intrinsic::donothing), {});
        }
        auto *fi = insertFence(insertAt);
        assert(fi && "unable to insert fence!");
    }
    #endif
  
    
    return true;
}

CallBase *FixGenerator::modifyCall(CallBase *cb, Function *newFn) {
    // May need to do some casts.
    // errs() << "\t" << __FUNCTION__ << " BEGIN\n";
    // if (StopSubprog) {
    //     errs() << "\t\tSTOP SUBPROG\n";
    //     return cb;
    // }

    assert(cb && "nonsense!");
    if (cb && cb->getFunction()) {
        // https://stackoverflow.com/questions/54524188/create-debug-location-for-function-calls-in-llvm-function-pass
        cb->getFunction()->addAttribute(AttributeList::FunctionIndex, Attribute::NoInline);
    }

    newFn->addAttribute(AttributeList::FunctionIndex, Attribute::NoInline);

    IRBuilder<> builder(cb->getNextNode());
    std::vector<Value *> newArgs;
    for (unsigned i = 0; i < newFn->arg_size(); ++i) {
        // Get new type
        Argument *arg = newFn->arg_begin() + i;
        Type *newType = arg->getType();

        // Get value
        Value *op = nullptr;
        if (i < cb->arg_size()) {
            op = cb->getArgOperand(i);
        } else {
            op = Constant::getNullValue(newType);
        }
        
        // Create the conversion
        // errs() << "\tOP:" << *op << "\n";
        // errs() << "\tTY:" << *newType << "\n";
        Value *newOp = nullptr;
        if (newType->isPointerTy()) {
            newOp = builder.CreateBitOrPointerCast(op, newType);
        } else {
            newOp = builder.CreateIntCast(op, newType, false /*is signed?*/);
        }

        newArgs.push_back(newOp);
    }

    // Now, replace the call.
    
    auto *newCb = builder.CreateCall(newFn, newArgs);
    auto *meta = cb->getMetadata("dbg");
    
    if (!meta && cb->getIntrinsicID() != Intrinsic::not_intrinsic) {
        Instruction *f = cb->getPrevNonDebugInstruction();
        Instruction *b = cb->getNextNonDebugInstruction();

        while (f || b) {
            if (f) {
                meta = f->getMetadata("dbg");
                if (meta) break;
                f = f->getPrevNonDebugInstruction();
            }
            if (b) {
                meta = b->getMetadata("dbg");
                if (meta) break;
                b = b->getNextNonDebugInstruction();
            }
        }
    }
    
    newCb->setMetadata("dbg", meta);
    if (!meta) {
        errs() << "cb:" << *cb << " in ";
        if (cb->getFunction()) errs() << cb->getFunction()->getName() << "\n";
        else errs() << "UNKNOWN\n";
        errs() << "newCb:" << *newCb << "\n";
    }
    
    // ReplaceInstWithInst(cb, newCb);
    // cb->eraseFromParent();
    
    return newCb;
}

#pragma endregion

#pragma region FixGenerators

/**
 * The instruction given should be the store that we need to flush.
 * 
 * TODO: Metadata for inserted instructions?
 * TODO: What kind of flush? Determine on machine stuff I'm sure.
 */
Instruction *GenericFixGenerator::insertFlush(const FixLoc &fl) {
    CallInst *clwbCall = nullptr;

    errs() << "insertFlush:\n" << fl.str() << "\n";

    for (Instruction *i : fl.insts()) {

        Value *addrExpr = nullptr;
        if (auto *si = dyn_cast<StoreInst>(i)) {
            errs() << "STORE:" << *si << "\n";
            addrExpr = si->getPointerOperand();
        } else if (auto *cx = dyn_cast<AtomicCmpXchgInst>(i)) {
            errs() << "CMPXCHG:" << *cx << "\n";
            addrExpr = cx->getPointerOperand();
        }

        if (addrExpr) {
            errs() << "Inserting flush in " << i->getFunction()->getName() << "\n";
            errs() << "Address of assign: " << *addrExpr << "\n";

            // I think we've made the assumption up to this point that len <= 64
            // and that [addr, addr + len) is within a cacheline. We'll continue
            // on with that assumption.
            // TODO: validate size of store (non-temporals) w/ alignment

            // 1) Set up the IR Builder.
            // -- want AFTER
            errs() << "After: " << *i->getNextNode() << "\n";
            IRBuilder<> builder(i->getNextNode());

            // 2) Check the type of addrExpr. If it is not an Int8PtrTy, we need to
            // insert a bitcast instruction so the function does not become broken
            // according to LLVM type safety.
            auto *ptrTy = Type::getInt8PtrTy(module_.getContext());
            if (ptrTy != addrExpr->getType()) {
                addrExpr = builder.CreateBitCast(addrExpr, ptrTy);
                errs() << "\t====>" << *addrExpr << "\n";
            }

            // 3) Find and insert a clwb.
            // This magically recreates an ArrayRef<Value*>.
            clwbCall = builder.CreateCall(getClwbDefinition(), {addrExpr});
            assert(clwbCall);
        }
    }

    return clwbCall;
}

/**
 * This should insert the fence right after the clwb instruction/fence 
 * instruction.
 * 
 * TODO: Metadata for inserted instructions?
 */
Instruction *GenericFixGenerator::insertFence(const FixLoc &fl) {
    Instruction *i = fl.last;
    // 1) Set up the IR Builder.
    // -- want AFTER
    IRBuilder<> builder(i->getNextNode());

    // 2) Find and insert an sfence.
    CallInst *sfenceCall = builder.CreateCall(getSfenceDefinition(), {});

    return sfenceCall;
}

Instruction *GenericFixGenerator::insertPersistentSubProgram(
    BugLocationMapper &mapper,
    const FixLoc &fl,
    const std::vector<LocationInfo> &callstack, 
    int idx,
    bool addFlushes,
    bool addFence) {
    
    errs() << __PRETTY_FUNCTION__ << " BEGIN\n";
    // Instruction *startInst = fl.first;

    Instruction *retInst = nullptr;
    // errs() << "GFIN " << *startInst << "\n";
    for (int i = 0; i < idx; ++i) {
        errs() << "GFLI IDX " << i << ": " << callstack[i].str() << "\n";

        if (!mapper.contains(callstack[i])) {
            // assert(0 == i && "don't know how to handle nested unknowns!");
            if (i > 0) {
                for (const auto &li : callstack) {
                    errs() << li.str() << "---contains? " << mapper.contains(li) << "\n";
                }
                errs() << "don't know how to handle nested unknowns, abort!\n";
                errs() << "idx=" << idx << ", contains=" << mapper.contains(callstack[0]) << "\n";
                return nullptr;
            }

            std::list<CallBase*> candidates;
            auto &nextFixLoc = mapper[callstack[i+1]];
            assert(!nextFixLoc.empty());
            assert(nextFixLoc.size() == 1);
            for (Instruction *i : nextFixLoc.front().insts()) {
                if (auto *c = dyn_cast<CallBase>(i)) candidates.push_back(c);
            }
            assert(!candidates.empty() && "has to be calling something!");
            assert(candidates.size() == 1 && "don't know how to handle multiple calls yet!");

            auto *cb = candidates.front();
            Function *f = cb->getCalledFunction();
            assert(f && "don't know what to do!");    

            if (f->getIntrinsicID() != Intrinsic::not_intrinsic) {
                Function *newFn = nullptr;
                switch (f->getIntrinsicID()) {
                    case Intrinsic::memcpy: {
                        newFn = getPersistentMemcpy();
                        break;
                    }
                    case Intrinsic::memset: {
                        newFn = getPersistentMemset();
                        break;
                    }
                    case Intrinsic::memmove: {
                        newFn = getPersistentMemmove();
                        break;
                    }
                    default: {
                        assert(false && "unhandled!");
                        break;
                    }
                }

                assert(cb && "wut");
                assert(newFn && "wut");

                // errs() << "SUBPROGRAM: " << *newFn << "\n";
                // errs() << "SUBPROGRAM: " << *cb << "\n";
                
                // return modifyCall(cb, newFn);

                // --- This screws up later pointers
                // CallBase *modCb = modifyCall(cb, newFn);
                // cb->eraseFromParent();

                cb->setCalledFunction(newFn);

                // errs() << "NOW: " << *modCb->getFunction() << "\n";
                errs() << "NOW: " << *cb->getFunction() << "\n";

                // retInst = modCb;
                retInst = cb;
            } else if (f->isDeclaration()) {  
                std::string declName(utils::demangle(f->getName().data()));
                errs() << *cb << "\n";
                errs() << "DECL: " << declName << "\n";

                Function *pmVersion = getPersistentVersion(declName.c_str());

                // -- This screws up stuff
                // CallBase *modCb = modifyCall(cb, pmVersion);
                // errs() << *modCb << "\n";
                // cb->eraseFromParent();

                // retInst = modCb;

                cb->setCalledFunction(pmVersion);
                retInst = cb;
            }

            continue;
        }

        auto &fixLocList = mapper[callstack[i]];
        if (fixLocList.size() > 1) {
            // Make sure they're all in the same function, cuz then it's fine.
            std::unordered_set<Function*> fns;
            for (auto &f : fixLocList) fns.insert(f.last->getFunction());
            assert(fns.size() == 1 && "don't know how to handle this weird code!");
        }

        Instruction *currInst = fixLocList.front().last;
        assert(currInst && "current can't be nullptr!");
        errs() << "CI ptr:" << currInst << "\n";
        errs() << "CI:" << *currInst << "\n";
        errs() << "cool\n";

        Function *fn = currInst->getFunction();
        Function *pmFn = fn;
        ValueToValueMapTy vmap;

        if (addFlushes) {
            pmFn = duplicateFunction(fn, vmap);
            // Now, we need to make all of the stores flushed
            bool successful = makeAllStoresPersistent(fn, pmFn, vmap);
            assert(successful && "failed to make persistent!");
        }
        
        // Now we need to replace the call.
        auto &nextFixLoc = mapper[callstack[i+1]];
        // assert(nextInstLoc.size() == 1 && "next still too big");
        assert(!nextFixLoc.empty());

        for (const FixLoc &nFix : nextFixLoc) {
            for (Instruction *ni : nFix.insts()) {
                errs() << *ni << " @ " << ni->getFunction()->getName() << "\n";
                if (auto *cb = dyn_cast<CallBase>(ni)) {
                    errs() << "\tCALL\n";
                    Function *cbFn = cb->getCalledFunction();
                    // Replace this value with a call to the new function.
                    if (cbFn == fn) {
                        cb->setCalledFunction(pmFn);
                        retInst = cb;
                    } else if (cbFn) {
                        continue;
                    } else {
                        /**
                         * For function pointers, we need a conditional mapping, a-la
                         * if (f == old_fn) new_fn(...)
                         */
                        errs() << "FUNCTION POINTER: " << *cb << "\n";
                        assert(false && "function pointer unhandled!");
                    }
                } 
            }  
        }
    }
    
    // Now, we add the fence after the call instruction to make sure everything
    // was persisted.
    // We only need to do this for bugs which require it.
    if (addFence) {
        errs() << "\t\tAdding fence!\n";
        auto *fi = insertFence(retInst);
        assert(fi && "unable to insert fence!");
    } else {
        errs() << "\t\tNOT adding fence!\n";
    }
    
    return retInst;
}

/**
 * This should just remove the flush.
 */
bool GenericFixGenerator::removeFlush(const FixLoc &fl) {
    errs() << "We be in remove flush\n";

    for (auto *i : fl.insts()) {
        errs() << "\t" << *i << "\n";
    }
    Instruction *i = fl.last;

    for (Instruction *i : fl.insts()) {
        if (CallInst *ci = dyn_cast<CallInst>(i)) {
            bool remove = false;

            if (const Function *f = utils::getFlush(ci)) {
                // Validate that this is a flush.
                // (iangneal): getting the intrinsic ID is more type-safe
                remove = true;
            } else if (ci->isInlineAsm()) {
                bool isFlushAsm = false;
                // Get the inline ASM string
                auto *ia = dyn_cast<InlineAsm>(ci->getCalledValue());
                assert(ia);
                auto str = ia->getAsmString();
                errs() << str << "\n";
                if (str == ".byte 0x66; xsaveopt $0") {
                    isFlushAsm = true;
                } else {
                    errs() << "'" << str << "' != " << ".byte 0x66; xsaveopt $0" << "\n";
                }
                // Check if it is
                if (isFlushAsm) {
                    remove = true;
                }
            }

            if (remove) {
                i->eraseFromParent();
                return true;
            }
            
        } else if (auto *si = dyn_cast<StoreInst>(i)) {
            assert(false && "TODO!");
        }
    }
    
    return false;
}

bool GenericFixGenerator::removeFlushConditionally(
        const std::list<FixLoc> &origs, 
        const FixLoc &redt,
        std::list<llvm::Instruction*> pathPoints) {

    errs() << "\n\n" << __FUNCTION__ << "\n\n";

    std::list<Instruction*> resetPoints;
    for (auto &fl : origs) {
        assert(fl.isValid());
        resetPoints.push_back(fl.first);
    }

    /**
     * Step 1. Create the conditionals.
     */
    std::list<GlobalVariable*> conditions;
    for (Instruction *setPoint : pathPoints) {
        GlobalVariable *gv = createConditionVariable(resetPoints, setPoint);
        assert(gv && "could not create global variable!");
        errs() << "GV: " << *gv << "\n";
        conditions.push_back(gv);
    }

    /**
     * Step 2. Get the bounds of the block we need to make conditional.
     */

    Instruction *start = redt.first, *end = redt.last;

    assert(start && end);

    /**
     * Step 3. Now, we actually create the conditional block to wrap around the
     * redundant store.
     */

    auto *i = createConditionalBlock(start, end, conditions);
    assert(i && "wat");

    return true;
}

/**
 * So, with the source code mapping, the instruction we have should be a call
 * to the C_createMetadata_Assign function. Arguments 2 and 3 are address and 
 * width, so we can steal that for making the flush and the call to 
 * "C_createMetadata_Flush"
 * 
 * TODO: Metadata for inserted instructions?
 * TODO: What kind of flush? Determine on machine stuff I'm sure.
 */
Instruction *PMTestFixGenerator::insertFlush(const FixLoc &fl) {
    Instruction *i = fl.last;

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
        CallInst *clwbCall = builder.CreateCall(getClwbDefinition(), {addrExpr});

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

/**
 * So, with the source code mapping, the instruction we have should be a call
 * to the C_createMetadata_Flush function, although it really shouldn't matter
 * too much---our theorem states that it shouldn't matter where we insert the 
 * fence. We just want it to steal the other arguments
 * 
 * "C_createMetadata_Fence" is the function we want to find.
 * 
 * TODO: Metadata for inserted instructions?
 */
Instruction *PMTestFixGenerator::insertFence(const FixLoc &fl) {
    Instruction *i = fl.last;

    if (CallInst *ci = dyn_cast<CallInst>(i)) {
        Function *f = ci->getCalledFunction();
        assert(f && f->getName() == "C_createMetadata_Flush" && "IDK!");

        // 1) Set up the IR Builder.
        // -- want AFTER
        IRBuilder<> builder(i->getNextNode());

        // 2) Find and insert an sfence.
        CallInst *sfenceCall = builder.CreateCall(getSfenceDefinition(), {});

        // 3) Find and insert a trace instruction.
        Function *trace = module_.getFunction("C_createMetadata_Fence");
        assert(trace && "could not find PMTest flush trace!");

        // Should be the same arguments as to the flush call...
        CallInst *traceCall = builder.CreateCall(trace, {ci->getArgOperand(0),
                                                         ci->getArgOperand(3), 
                                                         ci->getArgOperand(4)});

        return traceCall;
    }

    return nullptr;
}

Instruction *PMTestFixGenerator::insertPersistentSubProgram(
    BugLocationMapper &mapper,
    const FixLoc &fl,
    const std::vector<LocationInfo> &callstack,
    int idx,
    bool addFlush,
    bool addFence) {

    assert(false && "IMPLEMENT ME");
    return nullptr;
}

/**
 * This has to remove the flush and the assertion, which needs to be in the same
 * basic block as the flush, in all likelihood.
 */
bool PMTestFixGenerator::removeFlush(const FixLoc &fl) {
    Instruction *i = fl.last;
    assert(i && "must be non-null!");
    // We need to first find the flush and assertion.

    CallBase *flushCb=nullptr, *assertCb=nullptr;

    Function *assertFn = module_.getFunction("C_createMetadata_Flush");
    assert(assertFn && "could not find!");
    
    Instruction *tmp = i;
    do {
        if (auto *cb = dyn_cast<CallBase>(tmp)) {
            Function *f = cb->getCalledFunction();
            if (!assertCb && f && f == assertFn) {
                assertCb = cb;
            } else if (!flushCb && f && f->getIntrinsicID() == Intrinsic::x86_clwb) {
                flushCb = cb;
            }
        }
        if (assertCb && flushCb) break;
    } while ((tmp = tmp->getPrevNonDebugInstruction()));

    assert(flushCb && assertCb && "can't find both the flush and assertion!");

    // errs() << __FUNCTION__ << "  flushCb:" << *flushCb << "\n";
    // errs() << __FUNCTION__ << " assertCb:" << *assertCb << "\n";

    // This unlinks and deletes. Remove just unlinks.
    flushCb->eraseFromParent();
    assertCb->eraseFromParent();

    return true;
}

bool PMTestFixGenerator::removeFlushConditionally(
        const std::list<FixLoc> &origs, 
        const FixLoc &redt,
        std::list<Instruction*> pathPoints)
{
    assert(false && "update me!");
    return false;
#if 0
    Instruction *original = origs.front().last;
    Instruction *redundant = redt.last;
    std::list<GlobalVariable*> conditions;
    for (Instruction *point : pathPoints) {
        GlobalVariable *gv = createConditionVariable(original, point);
        assert(gv && "could not create global variable!");
        errs() << "GV: " << *gv << "\n";
        conditions.push_back(gv);
    }
    
    /**
     * Step 2. Get the bounds of the block we need to make conditional.
     */

    Instruction *start = nullptr, *end = redundant, *tmp = redundant;

    errs() << *end << "\n";
    // end should be the flush assertion, then we scroll up to find the flush.
    while(!start) {
        tmp = tmp->getPrevNonDebugInstruction();
        if (!tmp) break;
        if (auto *cb = dyn_cast<CallBase>(tmp)) {
            if (cb->getCalledFunction() == getClwbDefinition()){
                start = tmp;
            }
        }
    }
    assert(start && end);

    errs() << "HEY START" << *start << "\n";
    errs() << "HEY END" << *end << "\n";

    /**
     * Step 3. Now, we actually create the conditional block to wrap around the
     * redundant store.
     */

    auto *i = createConditionalBlock(start, end, conditions);
    assert(i && "wat");

    return true;
#endif
}

#pragma endregion