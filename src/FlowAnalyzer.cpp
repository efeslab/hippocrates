#include <algorithm>
#include <vector>
#include <iomanip>
#include <sstream>

#include "FlowAnalyzer.hpp"

using namespace llvm;
using namespace pmfix;
using namespace std;

#pragma region PMDesc

bool PMDesc::getPointsToSet(const llvm::Value *v,                                  
                            std::unordered_set<const llvm::Value *> &ptsSet) {
    /**                                                                            
     * Using a cache for this dramatically reduces the amount of time spent here,  
     * as the call to "getPointsToSet" has to re-traverse a bunch of internal      
     * data structures to construct the set.                                       
     */                                                                            
    bool ret = true;                                                               
    if (!cache_.count(v)) {                                                
        std::vector<const Value*> rawSet;                                            
        ret = anders_.getResult().getPointsToSet(v, rawSet);                      
        if (ret) { 
            ptsSet.insert(rawSet.begin(), rawSet.end());                                                                         
            cache_[v] = ptsSet;                                              
        }                                                                            
    } else {                                                                       
        ptsSet = cache_[v];                                                
    }                                                                              
                                                                                   
    return ret;
}

PMDesc::PMDesc(Module &m) {
    assert(!anders_.runOnModule(m) && "failed!");
}

void PMDesc::addKnownPMValue(llvm::Value *pmv) {
    std::unordered_set<const llvm::Value *> ptsSet;
    assert(getPointsToSet(pmv, ptsSet) && "could not get!");
    pm_values_.insert(ptsSet.begin(), ptsSet.end());
}

bool PMDesc::pointsToPM(llvm::Value *pmv) {
    std::unordered_set<const llvm::Value *> ptsSet;
    assert(getPointsToSet(pmv, ptsSet) && "could not get!");

    std::vector<const Value*> inter;

    (void)std::set_intersection(ptsSet.begin(), ptsSet.end(),
                                pm_values_.begin(), pm_values_.end(), inter.begin());
    return !inter.empty();
}

#pragma endregion

#pragma region FnContext 

FnContext::FnContext(FnContext *p, BasicBlock *bb) : current(bb) {
    if (p) {
        callStack.insert(callStack.end(), p->callStack.begin(), p->callStack.end());
        callStack.push_back(p);
    }
}

FnContext* FnContext::create(const BugLocationMapper &mapper, const TraceEvent &te) {
    // Start from the top down.
    FnContext *parent = nullptr;
    for (int i = te.callstack.size() - 1; i >= 0; --i) {
        const LocationInfo &li = te.callstack[i];
        if (!li.valid()) {
            if (!parent) continue;
            else assert(false && "wat");
        }

        std::unordered_set<BasicBlock*> possibleBBs;
        for (auto *inst : mapper[li]) {
            possibleBBs.insert(inst->getParent());
        }

        assert(possibleBBs.size() > 0 && "don't know how to handle!");
        assert(possibleBBs.size() == 1 && "don't know how to handle!");

        BasicBlock *callsite = possibleBBs.front()->getParent();
        FnContext *curr = new FnContext(parent, callSite);
        parent = curr;
    }

    return parent;
}

std::string FnContext::str() const {
    std::stringstream buffer;
    int n = 1;
    buffer << "<FnContext>\n";
    buffer << "[0] " << current << "\n";
    for (FnContext *fn : callStack) {
        buffer << "[" << n << "] " << current << "\n";
        n++;
    }
    buffer << "</FnContext>";

    return buffer.str();
}

#pragma endregion