#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"

#include "llvm/Support/CommandLine.h"

#include <set>
#include <map>
#include <stack>
#include <vector>
#include <tuple>
#include <queue>

#include "yaml-cpp/yaml.h"

#include "pass_utils.hpp"

using namespace llvm;
using namespace std;

namespace pmfix {
    cl::opt<std::string> TraceFile("trace-file", cl::desc("<trace file>"));

    struct PmBugFixer : public ModulePass {
        static char ID;
        PmBugFixer() : ModulePass(ID) {
            errs() << "Hello!!!\n";
            YAML::Node trace_info = YAML::LoadFile(TraceFile);  
        }

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<PostDominatorTreeWrapperPass>();
        }

        bool runOnModule(Module &m) override {
            // We return false because we aren't modifying anything.
            return false;
        }
    };
}

char pmfix::PmBugFixer::ID = 0;
static RegisterPass<pmfix::PmBugFixer> X("pm-bug-fixer", "PM Bug Fixing Pass",
                                         false /* Only looks at CFG */,
                                         false /* Analysis Pass */);

#if 0
static RegisterStandardPasses Y(
        PassManagerBuilder::EP_EarlyAsPossible,
        [](const PassManagerBuilder &Builder,
           legacy::PassManagerBase &PM) { PM.add(new PmBugFixer()); });
#endif
