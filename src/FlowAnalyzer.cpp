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

FnContext::FnContext(FnContext *p, Instruction *l) : last_(l) {
    if (p) {
        callStack_.insert(callStack_.end(), 
                          p->callStack_.begin(), p->callStack_.end());
        callStack_.push_back(p);
    }
    // Assumption: this is okay
    first_ = &last_->getFunction()->getEntryBlock().front();
}

FnContext* FnContext::create(const BugLocationMapper &mapper, const TraceEvent &te) {
    // Start from the top down.
    FnContext *parent = nullptr;
    for (int i = te.callstack.size() - 1; i >= 0; --i) {
        // const LocationInfo &caller = te.callstack[i];
        const LocationInfo &li = te.callstack[i];
        if (!li.valid() || !mapper.contains(li)) {
            if (!parent) continue;
            else assert(false && "wat");
        }

        errs() << "VALID: " << li.str() << "\n";
        // errs() << "\t called by " << caller.str() << "\n";

        std::list<Instruction*> possibleCallSites;
        for (auto *inst : mapper[li]) {
            possibleCallSites.push_back(inst);
        }

        assert(possibleCallSites.size() > 0 && "don't know how to handle!");
        assert(possibleCallSites.size() == 1 && "don't know how to handle!");

        Instruction *possible = possibleCallSites.front();

        if (parent) {
            errs() << "\t\t\tparent!\n";
        }

        FnContext *curr = new FnContext(parent, possible);
        
        parent = curr;
    }

    errs() << "\t\tRET: " << *parent->last_ << "\n";
    return parent;
}

void FnContext::constructSuccessors(void) {
    assert(!constructed_ && "double call!");
    constructed_ = true;

    /**
     * Normally, a basic block has it's own branching successors. However,
     * we also want to capture function calls. So, we will split basic blocks
     * at calls.
     * 
     * We also want to capture return instructions.
     */
    Instruction *i = first_;
    while ((i = i->getNextNonDebugInstruction())) {
        if (CallBase *cb = dyn_cast<CallBase>(i)) {
            Function *fn = cb->getCalledFunction();

            if (fn && fn->isIntrinsic()) continue;
            else if (fn && fn->isDeclaration()) continue;
            else if (fn == first_->getFunction()) {
                // TODO: recursion protection;
                assert(false && "TODO RECURSE");
            }

            // this sets the last, and the successor is the call.
            last_ = cb;
            Instruction *fnFirst = &fn->getEntryBlock().front();
            FnContext *called = new FnContext(this, fnFirst);
            successors_.push_back(called);
            return;
        }

        if (ReturnInst *ri = dyn_cast<ReturnInst>(i)) {
            last_ = ri;
            // This would mean that we're returning past the end of our scope.
            // Remember, it's a limited graph.
            FnContext *parent = callStack_.back();
            if (!parent) {
                return;
            }

            assert(parent->last_ && "last not set!!!");

            /**
             * We want essentially set the parent as the successor, but
             * make sure to move the "first" instruction cursor so that
             * we don't loop over this too much.
             */
            if (callStack_.back()) {
                
                Instruction *ni = parent->last_->getNextNonDebugInstruction();
                if (!ni) {
                    assert(false && "TODO! Likely need to do something here.");
                }
                auto *rchild = new FnContext(parent->callStack_, ni);
                successors_.push_back(rchild);
            }

            // No successors.
            return;
        }
    }
    
    /**
     * Now, we get the successor basic block.
     */
    last_ = first_->getParent()->getTerminator();
    for (BasicBlock *succ : llvm::successors(first_->getParent())) {
        FnContext *sctx = new FnContext(callStack_, succ->getFirstNonPHI());
        successors_.push_back(sctx);
    }
}

std::string FnContext::str() const {
    std::string tmp;
    llvm::raw_string_ostream buffer(tmp);
    int n = 1;
    errs() << __PRETTY_FUNCTION__ << " INST PTR: " << first_ << "\n";
    errs() << __PRETTY_FUNCTION__ << " INST: " << *first_ << "\n";

    if (constructed_) {
        buffer << "<FnContext first=" << first_ << " (" << *first_ 
            << "), last=(" << *last_ << ")>\n";
    } else {
        buffer << "<FnContext first=" << first_ << " (" << *first_ 
            << "), last=(unconstructed)>\n";
    }
    
    buffer << "\t[0] " << first_->getFunction()->getName().data() << "\n";
    for (auto iter = callStack_.rbegin(); iter != callStack_.rend(); ++iter) {
        buffer << "\t[" << n << "] " << 
            (*iter)->first_->getFunction()->getName().data() << "\n";
        n++;
    }
    buffer << "</FnContext>";

    return buffer.str();
}

bool FnContext::operator==(const FnContext &f) const {
    return first_ == f.first_ && callStack_ == f.callStack_;
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
        if (*n->ctx == end) {
            errs() << "End traversal\n";
            leaves.push_back(n);
            continue;
        }

        // Construct successors.
        n->ctx->constructSuccessors();

        errs() << "Traverse " << n->ctx->str() << "\n";

        for (FnContext *child_ctx : n->ctx->successors()) {
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