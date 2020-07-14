/**
 * Used to determine if there are any non-PM paths through the program.
 */

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"
#include "AndersenAA.h"

#include "BugReports.hpp"

namespace pmfix {
    typedef std::shared_ptr<AndersenAAWrapperPass> SharedAndersen; 
    typedef std::unordered_map<const llvm::Value*,                                
                               std::unordered_set<const llvm::Value*>> AndersenCache;
    typedef std::shared_ptr<AndersenCache> SharedAndersenCache;     

    /**
     * Description of the state of persistent memory in the program.
     * 
     * Given the input from traces as seeds, we figure out what other memory
     * in the system can point to persistent memory.
     */
    class PMDesc {
    private:
        AndersenAAWrapperPass anders_;
        AndersenCache cache_;

        std::unordered_set<const llvm::Value *> pm_values_;

        /**
         * Goes through the cache.
         */
        bool getPointsToSet(const llvm::Value *v,                                  
                            std::unordered_set<const llvm::Value *> &ptsSet);

    public:
        PMDesc(llvm::Module &m);

        /** 
         * Add a known PM value.
         */
        void addKnownPMValue(llvm::Value *pmv);

        bool pointsToPM(llvm::Value *val);

    };

    /**
     * Describes the current context.
     * 
     * I would prefer to do this as basic blocks, but to track function calls,
     * etc, you need to do it at the instruction level.
     * 
     * Need to track the first and last instruction in the context so we know
     * the range of the program covered.
     */
    class FnContext {
    private:

        std::list<FnContext*> callStack_;
        llvm::Instruction *first_ = nullptr;
        llvm::Instruction *last_ = nullptr;
        std::list<FnContext*> successors_;

        bool constructed_ = false;

        FnContext() = delete;
        // For calls.
        FnContext(FnContext *parent, llvm::Instruction *last);
        // For returns
        FnContext(std::list<FnContext*> cs, llvm::Instruction *f) 
            : callStack_(cs), first_(f) {}

    public:

        FnContext(const FnContext &fctx) = default;

        void constructSuccessors(void);

        std::list<FnContext*> &callStack() { return callStack_; }
        llvm::Instruction *first() { return first_; }

        llvm::Instruction *last() { 
            assert(constructed_ && "must construct first!");
            return last_; 
        }
        
        std::list<FnContext*> &successors() {
            assert(constructed_ && "must construct first!");
            return successors_;
        }

        static FnContext *create(const BugLocationMapper &mapper, 
                                 const TraceEvent &te);

        /**
         * Returns true if this leads to program termination.
         */
        bool isTerminator(void) const { 
            assert(constructed_ && "must construct first!");
            return successors_.empty(); 
        }
        

        bool operator==(const FnContext &f) const;
        bool operator!=(const FnContext &f) const { return !(*this == f); }

        /**
         * Debug: create a string representation
         */
        std::string str() const;
    };

    /**
     * Represents the
     */
    template <typename T>
    struct ContextGraph {
    private:
        void construct(FnContext &end);
    public:
        struct Node {
            FnContext *ctx;
            std::list<Node*> children;
            T metadata;

            Node(FnContext &fc) : ctx(new FnContext(fc)) {}
            Node(FnContext *fc) : ctx(fc) {}

            void addChild(Node *c) { children.push_back(c); }
        };

        Node *root;
        std::list<Node*> leaves;

        ContextGraph(FnContext &start, FnContext &end);
    };

    /**
     * We want to keep the branch points
     */
    class FlowAnalyzer {
    private:
        llvm::Module &m_;

    public:
        FlowAnalyzer(llvm::Module &m);


    };  
}
