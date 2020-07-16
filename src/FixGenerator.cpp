#include "FixGenerator.hpp"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IRBuilder.h"

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

GlobalVariable *FixGenerator::createConditionVariable(Instruction *resetBefore, 
                                                      Instruction *setAt) {
    
    auto *boolType = Type::getInt1Ty(module_.getContext());

    GlobalVariable *gv = new GlobalVariable(
        /* Module */ module_, /* Type */ boolType, /* isConstant */ false,
        /* Linkage */ GlobalValue::ExternalLinkage, 
        /* Constant constructor */ Constant::getNullValue(boolType),
        /* Name */ "test", /* Insert before */ nullptr,
        /* Thread local? */ GlobalValue::LocalExecTLSModel,
        /* AddressSpace */ 0,
        /* Externally init? */ false);

    return gv;
}

llvm::Instruction *FixGenerator::createConditionalBlock(
    llvm::Instruction *first, llvm::Instruction *end,
    std::list<llvm::GlobalVariable*> conditions) {
    return nullptr;
}

#pragma endregion

#pragma region FixGenerators

/**
 * The instruction given should be the store that we need to flush.
 * 
 * TODO: Metadata for inserted instructions?
 * TODO: What kind of flush? Determine on machine stuff I'm sure.
 */
Instruction *GenericFixGenerator::insertFlush(Instruction *i) {
    if (StoreInst *si = dyn_cast<StoreInst>(i)) {
        Value *addrExpr = si->getPointerOperand();

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
Instruction *GenericFixGenerator::insertFence(Instruction *i) {

    if (CallInst *ci = dyn_cast<CallInst>(i)) {
        Function *f = ci->getCalledFunction();
        
        // Validate that this is a flush.
        // TODO: could also be a non temporal store.
        // (iangneal): getting the intrinsic ID is more type-safe
        if (f->getIntrinsicID() != Intrinsic::x86_clwb) {
            assert(false && "must be a clwb!");
        }

        // 1) Set up the IR Builder.
        // -- want AFTER
        IRBuilder<> builder(i->getNextNode());

        // 2) Find and insert an sfence.
        CallInst *sfenceCall = builder.CreateCall(getSfenceDefinition(), {});

        return sfenceCall;
    }

    return nullptr;
}

/**
 * This should just remove the flush.
 */
bool GenericFixGenerator::removeFlush(Instruction *i) {

    if (CallInst *ci = dyn_cast<CallInst>(i)) {
        Function *f = ci->getCalledFunction();
        
        // Validate that this is a flush.
        // (iangneal): getting the intrinsic ID is more type-safe
        if (f->getIntrinsicID() != Intrinsic::x86_clwb) {
            assert(false && "must be a clwb!");
        }

        i->eraseFromParent();
        return true;
    }

    return false;
}

bool GenericFixGenerator::removeFlushConditionally(
        llvm::Instruction *original, llvm::Instruction *redundant,
        std::list<llvm::Instruction*> pathPoints) {
    assert(false && "Implement me!");
    return false;
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
Instruction *PMTestFixGenerator::insertFence(Instruction *i) {

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

/**
 * This has to remove the flush and the assertion, which needs to be in the same
 * basic block as the flush, in all likelihood.
 */
bool PMTestFixGenerator::removeFlush(Instruction *i) {
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

bool PMTestFixGenerator::removeFlushConditionally(Instruction *original, 
    Instruction *redundant, std::list<Instruction*> pathPoints) {

    for (Instruction *point : pathPoints) {
        GlobalVariable *gv = createConditionVariable(original, point);
        assert(gv && "could not create global variable!");
        errs() << "GV: " << *gv << "\n";
    }
    
    assert(false && "Implement me!");
    return false;
}

#pragma endregion