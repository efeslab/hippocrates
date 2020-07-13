#include <algorithm>
#include <vector>
#include <iomanip>
#include <sstream>
#include <deque>

#include "llvm/IR/CFG.h"

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

FnContext::FnContext(FnContext *p, BasicBlock *c) : current(c) {
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
        if (!li.valid() || !mapper.contains(li)) {
            errs() << "Abort " << li.str() << "\n";
            if (!parent) continue;
            else assert(false && "wat");
        }

        std::unordered_set<BasicBlock*> possibleBBs;
        for (auto *inst : mapper[li]) {
            possibleBBs.insert(inst->getParent());
        }

        assert(possibleBBs.size() > 0 && "don't know how to handle!");
        assert(possibleBBs.size() == 1 && "don't know how to handle!");

        BasicBlock *callsite = *possibleBBs.begin();
        FnContext *curr = new FnContext(parent, callsite);
        parent = curr;
        errs() << "FC " << li.str() << " @ BB " << callsite << "\n";
    }

    return parent;
}

std::list<FnContext*> FnContext::nextSuccessors(void) const {
    std::list<FnContext*> children;

    for (Instruction &i : *current) {
        if (ReturnInst *ri = dyn_cast<ReturnInst>(&i)) {
            if (callStack.back()) {
                return callStack.back()->next();
            } else {
                return children;
            }
        }
    } 
    
    /**
     * Now, we get the successor basic block.
     */
    for (BasicBlock *succ : successors(current)) {
        FnContext *sctx = new FnContext(*this);
        sctx->current = succ;
        children.push_back(sctx);
    }

    return children;
}

std::list<FnContext*> FnContext::next(void) {
    /**
     * Normally, a basic block has it's own branching successors. However,
     * we also want to capture function calls. So, we will split basic blocks
     * at calls.
     * 
     * We also want to capture return instructions.
     */
    llvm::CallBase *cb = nullptr;
    for (Instruction &i : *current) {
        if ((cb = dyn_cast<CallBase>(&i))) {
            Function *fn = cb->getCalledFunction();
            if (fn && fn->isIntrinsic()) continue;
            else if (fn && fn->isDeclaration()) continue;

            Instruction *split_inst = cb->getNextNonDebugInstruction();
            // If null, then we don't need to split
            if (split_inst) {
                BasicBlock *nb = current->splitBasicBlock(split_inst, "split_for_call");
                assert(nb && "failed to split!");
            }
            break;
        }
    }

    std::list<FnContext*> children = nextSuccessors();

    if (cb) {
        // TODO: recursion protection
        Function *calledFn = cb->getCalledFunction();
        if (!calledFn) {
            // Must be a function pointer.
            assert(false && "TODO!!!");
        }
        if (calledFn == current->getParent()) {
            assert(false && "recursion!");
        }

        FnContext *cctx = new FnContext(this, &calledFn->getEntryBlock());
        errs() << "Hmm curr=" << cctx->current << "\n";
        errs() << "CallBase: " << calledFn->getName() << "\n";
        children.push_back(cctx);
    }
    

    return children;
}

// TODO: recursion
bool FnContext::isTerminator(void) const {
    for (BasicBlock *bb : successors(current)) return false;
    errs() << "IS TERM: " << current << "\n";
    for (Instruction &i : *current) {
        if (isa<ReturnInst>(&i)) return false;
    }

    return true;
}

std::string FnContext::str() const {
    std::stringstream buffer;
    int n = 1;
    buffer << "<FnContext curr=" << current << ">\n";
    buffer << "\t[0] " << current->getParent()->getName().data() << "\n";
    for (auto iter = callStack.rbegin(); iter != callStack.rend(); ++iter) {
        buffer << "\t[" << n << "] " << (*iter)->current->getParent()->getName().data() << "\n";
        n++;
    }
    buffer << "</FnContext>";

    return buffer.str();
}

bool FnContext::operator==(const FnContext &f) const {
    return current == f.current && callStack == f.callStack;
}

#pragma endregion

#pragma region ContextGraph

template <typename T>
void ContextGraph<T>::construct(FnContext &end) {
    std::deque<Node*> frontier;
    frontier.push_back(root);

    size_t nnodes = 1;
    /**
     * For each node:
     * 1. Get the successing function contexts
     * 2. Construct nodes for each child context
     * 3. Add as children if conditions work.
     */
    while (frontier.size()) {
        Node *n = frontier.front();
        frontier.pop_front();
        // Pre-check
        if (*n->ctx == end || n->ctx->isTerminator()) {
            errs() << "End traversal\n";
            leaves.push_back(n);
            continue;
        }

        errs() << "Traverse " << n->ctx->str() << "\n";

        for (FnContext *child_ctx : n->ctx->next()) {
            errs() << "\t child: \n" << child_ctx->str() << "\n";
            nnodes++;
            Node *child = new Node(child_ctx);
            n->addChild(child);

            frontier.push_back(child);
        }

        if (n->children.empty()) {
            errs() << "no kids!\n";
            leaves.push_back(n);
        }
    }

    errs() << "<<< Created " << nnodes << " nodes! >>>\n";
}

template <typename T>
ContextGraph<T>::ContextGraph(FnContext &start, FnContext &end) {
    errs() << "CONSTRUCT ME\n";
    root = new Node(start);

    construct(end);

    // Validate that the leaf nodes are all what we expect them to be.
    assert(leaves.size() >= 1 && "Did not construct leaves!");
    for (Node *n : leaves) {
        if (*n->ctx != end && !n->ctx->isTerminator()) {
            errs() << (*n->ctx != end) << " && " << (!n->ctx->isTerminator()) << "\n";
            assert(false && "wat");
        }
    }
}

template struct pmfix::ContextGraph<bool>;

#pragma endregion