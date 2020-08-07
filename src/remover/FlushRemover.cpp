#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/CommandLine.h"

#include <list>

#include "PassUtils.hpp"

using namespace llvm;
using namespace std;

namespace pmremove {

struct FlushRemover : public ModulePass {
    static char ID; // For LLVM purposes.

    FlushRemover() : ModulePass(ID) {}

    void addIfIsFlush(list<Instruction*> &l, Instruction &i) {
        if (pmfix::utils::isFlush(i)) {
            l.push_back(&i);
        }
    }

    bool runOnModule(Module &m) override {
        list<Instruction*> flushes;
        for (Function &f : m) {
            for (BasicBlock &b : f) {
                for (Instruction &i : b) {
                    addIfIsFlush(flushes, i);
                }
            }
        }

        outs() << "Removing " << flushes.size() << " flushes!\n";
        bool mod = !flushes.empty();

        for (Instruction *i : flushes) i->eraseFromParent();
        return mod;
    }
};

}

char pmremove::FlushRemover::ID = 0;
static RegisterPass<pmremove::FlushRemover> X("flush-remover", "PM Flush Remover",
                                             false /* Only looks at CFG */,
                                             false /* Analysis Pass */);
