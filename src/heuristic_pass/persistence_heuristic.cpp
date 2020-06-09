#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"

#include <set>
#include <map>
#include <stack>
#include <vector>
#include <tuple>
#include <queue>

#include "pass_utils.hpp"
#include "nvm_function_info.hpp"

using namespace llvm;
using namespace std;

namespace {
    struct HeuristicPass : public ModulePass {
        static char ID;
        HeuristicPass() : ModulePass(ID) {}

        void getAnalysisUsage(AnalysisUsage &AU) const {
            AU.setPreservesCFG();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<PostDominatorTreeWrapperPass>();
        }

        bool runOnModule(Module &m) override {
            {
                Function *main = m.getFunction("main");
                //Function *main = m.getFunction("do_copy_to_non_pmem");
                //Function *main = m.getFunction("util_range_register");
                //Function *main = m.getFunction("pmem_map_register");
                //Function *main = m.getFunction("out_err");
                auto x = utils::FunctionInfo(*this, m);

                for (const auto &f : m) {
                    bool manip = x.manipulatesNVM(&f);
                    errs() << f.getName() << " has "
                        << x.totalPathsInFunction(&f) << " total paths\n";
                    x.dumpManip(&f);
                }
                errs() << "\n\n--------------------------------------\n\n";
                x.computeImportantSuccessors(main);
                x.dumpImportantSuccessors();

                errs() << format("Total paths in main: %lu\n",
                        x.totalPathsInFunction(main));
                (void)x.totalPathsThroughFunction(main);
                x.dumpPathsThrough();
                errs() << format("Total NVM paths in main: %lu\n",
                        x.totalImportantPaths(main));
                x.dumpUnique();
#if 0
                size_t npaths = x.computeNumPaths(*this, main);
                errs() << format("Number of total paths: %lu %lx\n", npaths, npaths);
                x.dump();
#endif
            }

            // We return false because we aren't modifying anything.
            return false;
        }
    };
}

char HeuristicPass::ID = 0;
static RegisterPass<HeuristicPass> X("heuristicpass", "Heuristic Computation Pass",
                                     true /* Only looks at CFG */,
                                     true /* Analysis Pass */);

#if 0
static RegisterStandardPasses Y(
        PassManagerBuilder::EP_EarlyAsPossible,
        [](const PassManagerBuilder &Builder,
           legacy::PassManagerBase &PM) { PM.add(new HeuristicPass()); });
#endif
