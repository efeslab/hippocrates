#include "BugReports.hpp"

#include <cstdint>
#include <string>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace pmfix;
using namespace std;

#pragma region TraceInfoBuilder

void TraceInfoBuilder::processEvent(TraceInfo &ti, YAML::Node event) {
    errs() << "Event timestamp: " << event["timestamp"].as<string>() << '\n';
}

TraceInfo TraceInfoBuilder::build(void) {
    TraceInfo ti;

    assert(doc_.IsSequence() && "Don't know what to do otherwise!");
    for (size_t i = 0; i < doc_.size(); ++i) {
        processEvent(ti, doc_[i]);
    }

    return ti;
}

#pragma endregion