#pragma once
/**
 * Bug reports contain two things:
 */

#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "yaml-cpp/yaml.h"

namespace pmfix {

/**
 * Just a wrapper for virtual address information.
 */
struct AddressInfo {
    uint64_t address;
    uint64_t length;

    AddressInfo() : address(0), length(0) {}

    // Helpers -- both inclusive.
    uint64_t start(void) const { return address; }
    uint64_t end(void) const { return address + length - 1llu; }

    // Methods for checking overlap with others/cache lines.
    bool isSingleCacheLine(void) const;
    bool overlaps(const AddressInfo &other) const;
    bool operator==(const AddressInfo &other) const;

    std::string str() const {
        std::stringstream buffer;
        buffer << "<AddressInfo: addr=" << address << 
            " len=" << length << ">";
        return buffer.str();
    }
};

/**
 * Just a wrapper for source code location information.
 */
struct LocationInfo {
    std::string function;
    std::string file;
    // -1 represents unknown
    int64_t line;

    // Returns just the file name, trims directory information.
    std::string getFilename(void) const;

    struct Hash {
        // We only want to hash the last part of the file to avoid 
        // hash issues when the directories differ.
        uint64_t operator()(const LocationInfo &li) const {
            return std::hash<std::string>{}(li.function) ^
                   std::hash<std::string>{}(li.getFilename()) ^ 
                   std::hash<int64_t>{}(li.line);
        }
    };

    bool operator==(const LocationInfo &other) const;
    bool operator!=(const LocationInfo &li) const { return !(*this == li); }

    bool valid(void) const {return line > 0;}

    std::string str() const;
};

/**
 * Creates a map of source code location -> LLVM IR location. Then allows lookups
 * so that bugs can be mapped from trace info (which are at source level) to
 * somewhere in the IR so we can start fixing bugs.
 * 
 * Sometimes, there are multiple locations that map to the same line of source
 * code---not just multiple assembly instructions, but multiple locations. The
 * solution I think is just apply fixes to both locations. The instructions 
 * should be identical.
 */
class BugLocationMapper {
private:

    llvm::Module &m_;

    std::unordered_map<LocationInfo, 
                       std::list<llvm::Instruction*>, 
                       LocationInfo::Hash> locMap_;

    void insertMapping(llvm::Instruction *i);

    void createMappings(llvm::Module &m);

public:
    BugLocationMapper(llvm::Module &m) : m_(m) { createMappings(m); }

    bool contains(const LocationInfo &li) const { return locMap_.count(li); }

    const std::list<llvm::Instruction*> &operator[](const LocationInfo &li) const 
        { return locMap_.at(li); }

    llvm::Module &module() const { return m_; }

};

struct TraceEvent {
    /**
     * The type of event, i.e. the kind of operation.
     */
    enum Type {
        INVALID = -1,
        STORE = 0, FLUSH, FENCE, 
        ASSERT_PERSISTED, ASSERT_ORDERED, REQUIRED_FLUSH
    };

    /**
     * Which bug finder the source came from.
     */
    enum Source {
        UNKNOWN = -1, PMTEST = 0, GENERIC = 1
    };

    static Type getType(std::string typeString);

    // Event data.
    Source source;
    Type type;
    uint64_t timestamp;
    std::vector<AddressInfo> addresses;
    LocationInfo location;
    bool isBug;
    std::vector<LocationInfo> callstack;

    // Debug
    std::string typeString;

    // Helper

    bool isOperation(void) const { 
        return type == STORE || type == FLUSH || type == FENCE; }
    bool isAssertion(void) const { 
        return type == ASSERT_PERSISTED || type == ASSERT_ORDERED ||
               type == REQUIRED_FLUSH; }

    static bool callStacksEqual(const TraceEvent &a, const TraceEvent &b);

    std::string str() const;

    /**
     * Get the values of the PM pointers associated with the events.
     */
    std::list<llvm::Value*> pmValues(const BugLocationMapper &mapper) const;
};


class TraceInfo {
private:
    friend class TraceInfoBuilder;

    // Trace data.
    // -- indices where bugs live
    std::list<int> bugs_; 
    // -- the actual events
    std::vector<TraceEvent> events_;
    // -- the source of the trace
    TraceEvent::Source source_;

    // Metadata. For stuff like which fix generator to use.
    YAML::Node meta_;

    // Don't want direct construction of this class.
    TraceInfo() {}

    void addEvent(TraceEvent &&event);

    TraceInfo(YAML::Node m);

public:

    const TraceEvent &operator[](int i) const { return events_[i]; }

    const std::list<int> &bugs() const { return bugs_; }

    std::string str() const;

    template<typename T>
    T getMetadata(const char *key) const { return meta_[key].as<T>(); }

    TraceEvent::Source getSource() const { return source_; }
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
