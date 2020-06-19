#pragma once
/**
 * Bug reports contain two things:
 */

#include <cstdint>
#include <string>
#include <vector>

#include "yaml-cpp/yaml.h"

namespace pmfix {

/**
 * Just a wrapper for virtual address information.
 */
struct AddressInfo {
    uint64_t address;
    uint64_t length;

    // TODO: methods for checking overlap.
};

/**
 * Just a wrapper for source code location information.
 */
struct LocationInfo {
    std::string file;
    uint64_t line;

    // TODO: turn this into LLVM types.
};

struct TraceEvent {
    enum Type {
        INVALID = -1,
        STORE = 0, FLUSH, FENCE, ASSERT_PERSISTED, ASSERT_ORDERED
    };

    static Type getType(std::string typeString);

    // Event data.
    Type type;
    uint64_t timestamp;
    std::vector<AddressInfo> addresses;
    LocationInfo location;
    bool isBug;

    // Debug
    std::string typeString;

    std::string str() const;
};


class TraceInfo {
private:
    friend class TraceInfoBuilder;

    // Trace data.
    // -- indices where bugs live
    std::list<int> bugs_; 
    std::vector<TraceEvent> events_;

    // Don't want direct construction of this class.
    TraceInfo() {}

    void addEvent(TraceEvent &&event) { events_.emplace_back(event); }

public:

    std::string str() const;
};

/**
 * Ian: my thought on using this builder style is that it will de-clutter
 * the TraceInfo class.
 */
class TraceInfoBuilder {
private:
    YAML::Node doc_;

    void processEvent(TraceInfo &ti, YAML::Node event);

public:
    TraceInfoBuilder(YAML::Node document) : doc_(document) {};

    TraceInfo build(void);
};

}
