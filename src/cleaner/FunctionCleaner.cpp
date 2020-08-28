#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/CommandLine.h"

#include <list>
#include <stdint.h>

#include "PassUtils.hpp"

using namespace llvm;
using namespace std;

namespace pmremove {

struct FunctionCleaner : public ModulePass {
    static char ID; // For LLVM purposes.

    FunctionCleaner() : ModulePass(ID) {}

    bool runOnModule(Module &m) override {
        size_t nfunctions = 0;
        size_t nremoved = 0;

        for (Function &f : m) {
            nfunctions++;
        }

        bool changed = false;       
        do {
            changed = false;

            // errs() << f.getName() << " " << f.getNumUses() << "\n";
            // bool isUsed = f.getNumUses() > 0 || f.hasAddressTaken();
            // if (isUsed) continue;

            // // errs() << f.getName() << " " << f.getNumUses() << "\n";
            
            // nzero++;
            // // f.eraseFrom

            list<Function*> toRemove;

            for (Function &f : m) {
                if (!f.isDefTriviallyDead()) continue;
                toRemove.push_back(&f);
            }

            changed = toRemove.size() > 0;

            for (auto *f : toRemove) {
                f->eraseFromParent();
                nremoved++;
            }

            errs() << "\tnremoved: " << nremoved << "\n";

        } while (changed);

        errs() << "\nNumber of functions at start: " << nfunctions;
        errs() << "\nNumber of functions removed: " << nremoved << "\n";

        return nremoved > 0;
    }
};

}

char pmremove::FunctionCleaner::ID = 0;
static RegisterPass<pmremove::FunctionCleaner> X("function-cleaner", "PM Function Cleaner",
                                             false /* Only looks at CFG */,
                                             false /* Analysis Pass */);
