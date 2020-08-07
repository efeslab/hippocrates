#include "FixGenerator.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/CFG.h"

#include "llvm/IR/DIBuilder.h"

#include "PassUtils.hpp"

#include <unordered_set>

using namespace pmfix;
using namespace llvm;

#pragma region FixGenerator

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

Function *FixGenerator::duplicateFunction(Function *f, std::string postFix) {
    ValueToValueMapTy vmap;
    Function *fNew = llvm::CloneFunction(f, vmap);
    fNew->setName(f->getName() + postFix);
    assert(fNew && "what");
    for (auto &bb : *f) errs() << "Old " << &bb << "\n";
    for (auto &bb : *fNew) errs() << "New " << &bb << "\n";
    errs() << *f << "\n";
    errs() << *fNew << "\n";
    return fNew;
}

bool FixGenerator::makeAllStoresPersistent(llvm::Function *f, bool useNT) {
    errs() << "FUNCTION PM: " << f->getName() << "\n";
    std::list<StoreInst*> flushPoints;
    std::list<ReturnInst*> fencePoints;
    for (BasicBlock &bb : *f) {
        for (Instruction &i : bb) {
            if (auto *cb = dyn_cast<CallBase>(&i)) {
                Function *f = cb->getCalledFunction();
                if (f->getIntrinsicID() == Intrinsic::dbg_declare) continue;
                
                // errs() << "ERR:" << *cb << "\n";
                // errs() << "FN:" << *f << "\n";
                // assert(false && "not supported yet!");
                // This should be okay. Just have to go down to the level that
                // the error occurs at.
            } else if (auto *ri = dyn_cast<ReturnInst>(&i)) {
                // Insert a sfence in front of the return.
                fencePoints.push_back(ri);
            } else if (auto *si = dyn_cast<StoreInst>(&i)) {
                /**
                 * Figure out if the store is to a stack variable. If so, we
                 * really don't want to add it.
                 */
                if (isa<AllocaInst>(si->getPointerOperand())) continue;
                flushPoints.push_back(si);
            }
        }
    }

    for (auto *si : flushPoints) {
        // Either insert a flush or make the store non-temporal.
        if (useNT) {
            // Set non-temporal metadata
            DIExpression *die = DIBuilder(module_).createConstantValueExpression(1);
            errs() << "MD:" << *die << "\n";
            si->setMetadata(LLVMContext::MD_nontemporal, die);
            errs() << "NT:" << *si << "\n";
        } else {
            // Insert flush
            errs() << "Flushing" << *si << " in " << si->getFunction()->getName() << "\n";
            auto *ni = insertFlush(si);
            assert(ni && "unable to insert flush!");
        }
    }

    if (flushPoints.empty()) return false;

    for (auto *ri : fencePoints) {
        Instruction *insertAt = ri->getPrevNonDebugInstruction();
        if (!insertAt) {
            IRBuilder<> builder(ri);
            insertAt = builder.CreateCall(Intrinsic::getDeclaration(&module_, Intrinsic::donothing), {});
        }
        auto *fi = insertFence(insertAt);
        assert(fi && "unable to insert fence!");
    }

    return true;
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
    Instruction *i = fl.last;

    Value *addrExpr = nullptr;
    if (StoreInst *si = dyn_cast<StoreInst>(i)) {
        addrExpr = si->getPointerOperand();
    } else if (auto *cx = dyn_cast<AtomicCmpXchgInst>(i)) {
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
        CallInst *clwbCall = builder.CreateCall(getClwbDefinition(), {addrExpr});

        return clwbCall;
    }

    return nullptr;
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

static CallBase *modifyCall(CallBase *cb, Function *newFn) {
    // May need to do some casts.
    IRBuilder<> builder(cb);
    std::list<Value *> newArgs;
    for (unsigned i = 0; i < newFn->arg_size(); ++i) {
        // Get value
        Value *op = cb->getArgOperand(i);
        // Get new type
        Argument *arg = newFn->arg_begin() + i;
        // errs() << "\tARG:" << *arg << "\n";
        Type *newType = arg->getType();
        // Create the conversion
        // errs() << "\tOP:" << *op << "\n";
        // errs() << "\tTY:" << *newType << "\n";
        Value *newOp = builder.CreateBitOrPointerCast(op, newType);
        // Replace arg
        cb->setArgOperand(i, newOp);
    }

    // Now, replace the call.
    cb->setCalledFunction(newFn);
    errs() << "cb:" << *cb << "\n";
    return cb;
}

Instruction *GenericFixGenerator::insertPersistentSubProgram(
    BugLocationMapper &mapper,
    const FixLoc &fl,
    const std::vector<LocationInfo> &callstack, 
    int idx) {
    
    errs() << __PRETTY_FUNCTION__ << " BEGIN\n";
    Instruction *startInst = fl.first;

    if (!mapper.contains(callstack[0])) {
        // assert(1 == idx && "don't know how to handle nested unknowns!");
        if (idx != 1) {
            for (const auto &li : callstack) {
                errs() << li.str() << "\n";
            }
            errs() << "don't know how to handle nested unknowns, abort!\n";
            errs() << "idx=" << idx << ", contains=" << mapper.contains(callstack[0]) << "\n";
            return nullptr;
        }

        std::list<CallBase*> candidates;
        for (Instruction *i : fl.insts()) {
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

            errs() << *newFn << "\n";
            errs() << *cb << "\n";
            
            return modifyCall(cb, newFn);
        }

        if (f->isDeclaration()) {  
            std::string declName(utils::demangle(f->getName().data()));
            errs() << *cb << "\n";
            errs() << "DECL: " << declName << "\n";

            Function *pmVersion = getPersistentVersion(declName.c_str());
            CallBase *modCb = modifyCall(cb, pmVersion);
            errs() << *modCb << "\n";

            return modifyCall(cb, pmVersion);
        }
    }

    Instruction *retInst = nullptr;
    errs() << "GFIN " << *startInst << "\n";
    for (int i = 0; i < idx; ++i) {
        errs() << "GFLI " << callstack[i].str() << "\n";

        auto &fixLocList = mapper[callstack[i]];
        if (fixLocList.size() > 1) {
            // Make sure they're all in the same function, cuz then it's fine.
            std::unordered_set<Function*> fns;
            for (auto &f : fixLocList) fns.insert(f.last->getFunction());
            assert(fns.size() == 1 && "don't know how to handle this weird code!");
        }

        Instruction *currInst = fixLocList.front().last;
        errs() << "CI:" << *currInst << "\n";

        Function *fn = currInst->getFunction();
        Function *pmFn = duplicateFunction(fn);

        // Now, we need to 
        bool successful = makeAllStoresPersistent(pmFn, false);
        assert(successful && "failed to make persistent!");

        // Now we need to replace the call.
        auto &nextFixLoc = mapper[callstack[i+1]];
        // assert(nextInstLoc.size() == 1 && "next still too big");
        assert(!nextFixLoc.empty());

        for (const FixLoc &nFix : nextFixLoc) {
            Instruction *ni = nFix.last;
            errs() << *ni << " @ " << ni->getFunction()->getName() << "\n";
            if (auto *cb = dyn_cast<CallBase>(ni)) {
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
    int idx) {

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