#include "BugReports.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unistd.h>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace pmfix;
using namespace std;

#pragma region AddressInfo

bool AddressInfo::isSingleCacheLine(void) const {
    static uint64_t cl_sz = (uint64_t)sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    uint64_t cl_start = start() / cl_sz;
    uint64_t cl_end = end() / cl_sz;
    return cl_start == cl_end;
}

bool AddressInfo::overlaps(const AddressInfo &other) const {
    return start() < other.end() || other.start() < end();
}

bool AddressInfo::operator==(const AddressInfo &other) const {
    return address == other.address && length == other.length;
}

#pragma endregion

#pragma region TraceEvent

TraceEvent::Type TraceEvent::getType(string typeString) {
    transform(typeString.begin(), typeString.end(), typeString.begin(), 
              [](unsigned char c) -> unsigned char { return std::tolower(c); });
    if (typeString == "store") return TraceEvent::STORE;
    if (typeString == "flush") return TraceEvent::FLUSH;
    if (typeString == "fence") return TraceEvent::FENCE;
    if (typeString == "assert_persisted") return TraceEvent::ASSERT_PERSISTED;
    if (typeString == "assert_ordered") return TraceEvent::ASSERT_ORDERED;
    
    return TraceEvent::INVALID;
}

template< typename T >
std::string int_to_hex( T i )
{
  std::stringstream stream;
  stream << "0x" 
         << std::setfill ('0') << std::setw(sizeof(T)*2) 
         << std::hex << i;
  return stream.str();
}

std::string TraceEvent::str() const {
    std::stringstream buffer;

    buffer << "Event (time=" << timestamp << ")\n";
    buffer << "\tType: " << typeString << '\n';
    buffer << "\tLocation: " << location.file << ":" << location.line << '\n';
    if (addresses.size()) {
        buffer << "\tAddress Info:\n";
        for (const auto &ai : addresses) {
            buffer << "\t\tAddress: " << int_to_hex(ai.address) << '\n';
            buffer << "\t\tLength: " << ai.length << '\n';
        }
    }

    return buffer.str();
}

#pragma endregion

#pragma region TraceInfo

void TraceInfo::addEvent(TraceEvent &&event) {
    if (event.isBug) {
        bugs_.push_back(events_.size());
    }

    events_.emplace_back(event); 
}

std::string TraceInfo::str(void) const {
    std::stringstream buffer;

    for (const auto &event : events_) {
        buffer << event.str() << '\n';
    }

    return buffer.str();
}

#pragma endregion

#pragma region TraceInfoBuilder

void TraceInfoBuilder::processEvent(TraceInfo &ti, YAML::Node event) {
    TraceEvent e;
    e.typeString = event["event"].as<string>();

    TraceEvent::Type event_type = TraceEvent::getType(e.typeString);

    assert(event_type != TraceEvent::INVALID);
    
    e.type = event_type;
    e.timestamp = event["timestamp"].as<uint64_t>();
    e.location.file = event["file"].as<string>();
    e.location.line = event["line"].as<uint64_t>();
    e.isBug = event["is_bug"].as<bool>();

    switch (e.type) {
        case TraceEvent::STORE:
        case TraceEvent::FLUSH:
        case TraceEvent::ASSERT_PERSISTED:
            AddressInfo ai;
            ai.address = event["address"].as<uint64_t>();
            ai.length = event["length"].as<uint64_t>();
            e.addresses.push_back(ai);
            break;
        case TraceEvent::ASSERT_ORDERED:
            AddressInfo a, b;
            a.address = event["address_a"].as<uint64_t>();
            a.length = event["length_a"].as<uint64_t>();
            b.address = event["address_b"].as<uint64_t>();
            b.length = event["length_b"].as<uint64_t>();
            e.addresses.push_back(a);
            e.addresses.push_back(b);
            break;
        default:
            break;
    }

    ti.addEvent(std::move(e));
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