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
     * We do this as basic blocks.
     * 
     * EG call site, stack. Also current instruction.
     */
    struct FnContext {
    private:
        FnContext() = delete;
        // For calls.
        FnContext(FnContext *parent, llvm::BasicBlock *current);

        /**
         * This gets the successors based purely off of the intra-procedural
         * control flow, or returns.
         */
        std::list<FnContext*> nextSuccessors(void) const;
    public:

        FnContext(const FnContext &fctx) = default;

        std::list<FnContext*> callStack;
        llvm::BasicBlock *current;

        static FnContext *create(const BugLocationMapper &mapper, 
                                 const TraceEvent &te);

        /**
         *
         * TODO: Handle exception semantics. 
         */
        std::list<FnContext*> next(void);

        /**
         * Returns true if this leads to program termination.
         */
        bool isTerminator(void) const;

        bool operator==(const FnContext &f) const;
        bool operator!=(const FnContext &f) const { return !(*this == f); }

        /**
         * Debug: create a string representation
         */
        std::string str() const;
    };

    /**
     * 
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
