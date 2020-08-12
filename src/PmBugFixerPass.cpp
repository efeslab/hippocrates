#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Support/CommandLine.h"

#include <set>
#include <map>
#include <stack>
#include <vector>
#include <tuple>
#include <queue>

#include "yaml-cpp/yaml.h"

#include "BugReports.hpp"
#include "BugFixer.hpp"

using namespace llvm;
using namespace std;

namespace pmfix {

cl::opt<std::string> TraceFile("trace-file", cl::desc("<trace file>"));

cl::list<std::string> Immutables("immutable-fns", cl::desc("Something"), 
                                 cl::ZeroOrMore, cl::CommaSeparated);

struct PmBugFixerPass : public ModulePass {
    static char ID; // For LLVM purposes.

    PmBugFixerPass() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.setPreservesCFG();
        AU.addRequired<DominatorTreeWrapperPass>();
        AU.addRequired<PostDominatorTreeWrapperPass>();
    }

    bool runOnModule(Module &m) override {
        // {
        //     auto &mapper = BugLocationMapper::getInstance(m);
        // }

        YAML::Node trace_info_doc = YAML::LoadFile(TraceFile);
        TraceInfo ti = TraceInfoBuilder(m, trace_info_doc).build();
        // errs() << "TraceInfo string:\n" << ti.str() << '\n';
        if (ti.empty()) {
            errs() << "Err: trace is empty!!!\n";;
            return false;
        }
        
        // Construct bug fixer
        BugFixer fixer(m, ti);
        errs() << "fixer built!\n";
        for (const std::string &fnName : Immutables) {
            fixer.addImmutableFunction(fnName);
        } 

        bool modified = fixer.doRepair();

        if (modified)
            errs() << "Modified!\n";
        else
            errs() << "Not modified!\n";   

        return modified;
    }
};

}

char pmfix::PmBugFixerPass::ID = 0;
static RegisterPass<pmfix::PmBugFixerPass> X("pm-bug-fixer", "PM Bug Fixing Pass",
                                             false /* Only looks at CFG */,
                                             false /* Analysis Pass */);
