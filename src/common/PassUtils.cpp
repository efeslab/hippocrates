#include "PassUtils.hpp"

#include <cxxabi.h>

using namespace pmfix;

#pragma region PMFix

std::string utils::demangle(const char *name) {
    int status;
    char *realname;
    std::string ret;

    realname = abi::__cxa_demangle(name, 0, 0, &status);
    if (!status) {
        ret = std::string(realname);
    } else {
        ret = std::string(name);
    }
    free(realname);

    return ret;
}

const Function *utils::getFlush(const CallBase *cb) {
    if (!cb) return nullptr;
    const Function *f = cb->getCalledFunction();
    if (!f) return nullptr;

    const auto iid = f->getIntrinsicID();
    if (iid == Intrinsic::x86_clwb || iid == Intrinsic::x86_clflushopt ||
        iid == Intrinsic::x86_sse2_clflush) return f;

    return nullptr;
}

std::list<Value*> utils::getConditionVariables(BasicBlock *bb) {
    std::list<BasicBlock*> frontier = {bb};
    std::unordered_set<BasicBlock*> traversed;

    std::list<Value*> conditionals;

    while (frontier.size()) {
        BasicBlock *block = frontier.front();
        frontier.pop_front();

        if (traversed.count(block)) continue;
        traversed.insert(block);

        for (BasicBlock *pred : predecessors(block)) {
            Instruction *inst = pred->getTerminator();
            if (auto *bi = dyn_cast<BranchInst>(inst)) {
                if (bi->isConditional()) {
                    conditionals.push_back(bi->getCondition());
                }
            } else if (auto *si = dyn_cast<SwitchInst>(inst)) {
                conditionals.push_back(si->getCondition());
            } else {
                assert(false && "wat");
            }

            frontier.push_back(pred);
        }
    }

    return conditionals;
}

#pragma endregion

bool utils::checkInlineAsmEq(const Instruction *iptr...) {
    va_list args;
    va_start(args, iptr);

    const InlineAsm *ia = nullptr;
    const Instruction &i = *iptr;
    if (i.getOpcode() == Instruction::Call) {
        const CallInst &ci = static_cast<const CallInst&>(i);
        if (ci.isInlineAsm()) {
            ia = static_cast<InlineAsm*>(ci.getCalledValue());
        }
    }

    char *asmStr;
    while(nullptr != ia &&
          nullptr != (asmStr = va_arg(args, char*))) {
        if (ia->getAsmString() == asmStr) {
            va_end(args);
            return true;
        }
    }

    va_end(args);
    return false;
}

bool utils::checkInstrinicInst(const Instruction *iptr...) {
    va_list args;
    va_start(args, iptr);
    assert(iptr && "wat");

    const Function *fn = nullptr;
    const Instruction &i = *iptr;
    if (i.getOpcode() == Instruction::Call) {
        const CallInst &ci = static_cast<const CallInst&>(i);
        if (ci.getIntrinsicID() != Intrinsic::not_intrinsic) {
            const IntrinsicInst &ii = static_cast<const IntrinsicInst&>(i);
            fn = ii.getCalledFunction();
        }
    }

    char *fnName;
    while(nullptr != fn &&
          nullptr != (fnName = va_arg(args, char*))) {
        if (fn->getName().contains(fnName)) {
            va_end(args);
            return true;
        }
    }

    va_end(args);
    return false;
}

bool utils::isFlush(const Instruction &i) {
    return checkInstrinicInst(&i,
                              "clflush",
                              nullptr) ||
           utils::checkInlineAsmEq(&i,
                   ".byte 0x66; clflush $0",
                   ".byte 0x66; xsaveopt $0", nullptr) ||
            (nullptr != utils::getFlush(dyn_cast<CallBase>(&i)));
}

bool utils::isFence(const Instruction &i) {
    return checkInstrinicInst(&i, "sfence", nullptr);
}

Value* utils::getPtrLoc(Value *v) {
    if (isa<User>(v)) {
        User *u = dyn_cast<User>(v);
        for (auto &op : u->operands()) {
            if (isa<AllocaInst>(op)) {
                return op;
            }
        }
    }

    return v;
}

unordered_set<const Value*> utils::getPtrsFromStoredLocs(const Value *ptr) {
    unordered_set<const Value*> ptrs;

    for (const auto *u : ptr->users()) {
        const StoreInst *si = dyn_cast<StoreInst>(u);
        if (nullptr != si
            && ptr == si->getValueOperand()) {
            errs() << "\t\t(store): " << *si << "\n";
            const Value *store_loc = si->getPointerOperand();

            for (const auto *su : store_loc->users()) {
                //errs() << "\t\t\t(store user): " << *su << "\n";
                const LoadInst *li = dyn_cast<LoadInst>(su);
                if (nullptr != li
                    && store_loc == li->getPointerOperand()) {
                    errs() << "\t\t\t(load): " << *li << "\n";

                    ptrs.insert(li);
                }
            }
        }
    }

    return ptrs;
}

void utils::getDerivativePtrs(unordered_set<const Value*> &s)
{
    if (s.empty()) return;
    unordered_set<const Value*> der;

    for (auto *v : s) {
        // We're not interested with dereferenced values from NVM.
        if (!v->getType()->isPointerTy()) continue;

        for (auto *u : v->users()) {
            if (u->getType()->isPointerTy()) {
                der.insert(u);
            }
        }

        auto fromStores = getPtrsFromStoredLocs(v);
        der.insert(fromStores.begin(), fromStores.end());
    }

    getDerivativePtrs(der);

    s.insert(der.begin(), der.end());
}

void utils::getModifiers(const Value* ptr, unordered_set<const Value*> &s) {
    for (auto *u : ptr->users()) {
        if (const StoreInst *si = dyn_cast<StoreInst>(u)) {
            if (si->getPointerOperand() == ptr) {
                s.insert(u);
            }
        }

        if (isFlush(*dyn_cast<Instruction>(u))) {
            s.insert(u);
        }
    }
}

list<const Function*> utils::getNestedFunctionCalls(const BasicBlock *bb) {
    list<const Function*> fns;

    for (const Instruction &i : *bb) {
        const CallInst *ci = dyn_cast<CallInst>(&i);
        if (ci && !ci->isInlineAsm()) {
            const Function *cfn = ci->getCalledFunction();
            if (cfn && !cfn->isIntrinsic()) {
                fns.push_back(cfn);
            }
        }
    }

    return fns;
}
