#pragma once

#include <stdint.h>
#include <cassert>
#include <cstdarg>
#include <vector>
#include <iostream>
#include <iomanip>
#include <map>
#include <unordered_set>
#include <queue>
#include <list>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Dominators.h"

using namespace llvm;
using namespace std;

namespace pmfix {
namespace utils {

    #pragma region PMFix



    #pragma endregion

    #pragma region Agamotto

    /**
     * Checks whether the given Instruction i is:
     * 1. An instance of a InlineAsm instruction, and
     * 2. Equals any of the specified assembly instruction strings given in the
     * varargs.
     *
     * The varargs should be string literals which contain inline assembly. The
     * final vararg should be a nullptr, which the function interprets as the
     * end of the list.
     *
     * Returns true if the Instruction is an inline assembly call and equal to
     * any one of the specified assembly literals and returns false otherwise.
     */
    bool checkInlineAsmEq(const Instruction *iptr...);


    /**
     * Checks whether the given Instruction i is:
     * 1. An instance of a LLVM Instrinic instruction/function call, and
     * 2. If the name of the instrinic function contains any of the partial
     * names as specified in the varargs.
     *
     * The varargs should be string literals which contain partial names. The
     * final vararg should be a nullptr, which the function interprets as the
     * end of the list.
     *
     * Returns true if the Instruction is an instrinic call and contains any one
     * of the partial string names and returns false otherwise.
     */
    bool checkInstrinicInst(const Instruction *iptr...);

    /**
     * Returns true if any instruction represents a cache-flushing instruction.
     */
    bool isFlush(const Instruction &i);

    /**
     * Returns true if any instruction represents a cache-flushing instruction.
     */
    bool isFence(const Instruction &i);

    /**
     * For a pointer of type T*, find where the pointer is stored, aka a value
     * of type T**.
     *
     * TODO: only works if T** is a stack allocation.
     */
    Value* getPtrLoc(Value *v);

    /**
     * For a given set of pointers, find all derivative pointers, aka
     * pointers that are some offset of the given pointer.
     *
     * Operates recursively until there are no new derivative pointers.
     *
     * Thought of doing this by checking the operation type, but instead
     * we could just check that the type of the User is the same as the
     * input (aka, also a pointer).
     */
    void getDerivativePtrs(unordered_set<const Value*> &s);

    /**
     * Sometimes, a pointer (particularly a pointer argument) is stored somewhere
     * on the stack (T**) before being loaded again (T*) and then used. This
     * sort of breaks up the typical chain of derivation.
     *
     * Therefore, for the given pointer, find all the locations it is stored into.
     * Then, return all the pointers that are loaded from those locations.
     */
    unordered_set<const Value*> getPtrsFromStoredLocs(const Value *ptr);

    /**
     * Return a set of instructions that modify the data that is pointed to by
     * the given pointer value.
     */
    void getModifiers(const Value* ptr, unordered_set<const Value*> &s);

    /**
     * Finds all of the function calls nested within the given basic block and
     * returns them in a list ordered from first encountered to last encountered.
     */
    list<const Function*> getNestedFunctionCalls(const BasicBlock*);

    #pragma endregion
}
}