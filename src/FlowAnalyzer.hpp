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
    class PmDesc {
    private:
        SharedAndersen anders_;
        SharedAndersenCache cache_;

        /**
         * There should be no need to clear/reset anything, only on a return when
         * the locals are implicitly forgotten.
         * 
         * The globals, however, should be copied.
         */
        std::unordered_set<const llvm::Value *> pm_locals_;
        std::unordered_set<const llvm::Value *> pm_globals_;
        
        /**
         * Goes through the cache.
         */
        bool getPointsToSet(const llvm::Value *v,                                  
                            std::unordered_set<const llvm::Value *> &ptsSet);

    public:
        PmDesc(llvm::Module &m);

        /** 
         * Add a known PM value.
         * 
         * We don't really want to remove anything, part of the conservative
         * analysis.
         */
        void addKnownPmValue(llvm::Value *pmv);

        bool pointsToPm(llvm::Value *val);

        void doReturn(const PmDesc &d) { pm_globals_ = d.pm_globals_; }

        /**
         * Returns true if this is subset of possSuper
         */
        bool isSubsetOf(const PmDesc &possSuper);

    };

    /**
     * Describes the current function context.
     * 
     * I would prefer to do this as basic blocks, but to track function calls,
     * etc, you need to do it at the instruction level.
     * 
     * Need to track the first and last instruction in the context so we know
     * the range of the program covered.
     * 
     */
    class FnContext : public std::enable_shared_from_this<FnContext> {
    public:
        typedef std::shared_ptr<FnContext> FnContextPtr;
        typedef std::shared_ptr<FnContext> Shared;
    private:

        /**
         * A stack is representable by who called it. A single CallBase
         * instruction gives us all the information we need.
         */
        std::list<llvm::CallBase*> callStack_;
        // Allows us to reuse contexts.
        FnContextPtr parent_;
        // Tracks PM state at the context level.
        PmDesc pm_;

        /**
         * We should also cache the called contexts for all the callsites in
         * this function.
         */
        std::shared_ptr<
            std::unordered_map<llvm::CallBase*, FnContextPtr>
        > callBaseCache_;

        FnContext(llvm::Module &m) 
            : callBaseCache_(new std::unordered_map<llvm::CallBase*, FnContextPtr>), 
              callStack_(), parent_(nullptr), pm_(m) {}

    public:


        FnContext(const FnContext &fctx) = default;
        
        /**
         * Also handles propagation of PM.
         * 
         * To check for recursion, we want to see if any of the call base
         * instructions in the stack are the same. If so, we 
         *
         * With explicit call.
         */
        FnContextPtr doCall(llvm::Function *f, llvm::CallBase *cb);

        /**
         * Also handles propagation of PM back
         */
        FnContextPtr doReturn(llvm::ReturnInst *ri);

        static FnContextPtr create(llvm::Module &m) {
            return std::make_shared<FnContext>(m);
        }

        bool operator==(const FnContext &f) const;
        bool operator!=(const FnContext &f) const { return !(*this == f); }

        /**
         * Debug: create a string representation
         */
        std::string str() const;
    };

    /**
     * Like a basic block, but smaller.
     * 
     * All the successor and parent stuff will be handled in the graph
     */
    struct ContextBlock {
        typedef std::shared_ptr<ContextBlock> ContextBlockPtr;
        typedef std::shared_ptr<ContextBlock> Shared;
    // private:
    //     std::shared_ptr<
    //         std::unordered_map<
    //             llvm::Instruction*,
    //             ContextBlockPtr
    //         >
    //     > existing_;

    public:
        FnContext::Shared ctx;
        llvm::Instruction *first = nullptr;
        llvm::Instruction *last = nullptr;

        static ContextNodePtr create<T>(const BugLocationMapper &mapper, 
                                        const TraceEvent &te);
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
            typedef std::shared_ptr<Node> NodePtr;
            typedef std::shared_ptr<Node> Shared;

            ContextBlock block;
        };

        

        NodePtr root;
        std::list<NodePtr> leaves;

        ContextGraph(FnContext &start, FnContext &end);
    };

    /**
     * We want to keep the branch points
     */
    class FlowAnalyzer {
    private:
        llvm::Module &m_;

    public:
        FlowAnalyzer(llvm::Module &m) : m_(m) {}


    };  
}
