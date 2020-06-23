#include "BugFixer.hpp"

using namespace pmfix;
using namespace llvm;

#pragma region FixGenerators

bool PMTestFixGenerator::insertFlush(llvm::Instruction *i) {

    return false;
}

#pragma endregion

#pragma region BugFixer

bool BugFixer::doRepair(void) {

    for (int bug_index : trace_.bugs()) {
        errs() << "Bug Index: " << bug_index << "\n";
    }

    return false;
}

#pragma endregion