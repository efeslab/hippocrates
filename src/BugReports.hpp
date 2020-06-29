#pragma once
/**
 * Bug reports contain two things:
 */

#include <cstdint>
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

    // Helpers -- both inclusive.
    uint64_t start(void) const { return address; }
    uint64_t end(void) const { return address + length - 1llu; }

    // Methods for checking overlap with others/cache lines.
    bool isSingleCacheLine(void) const;
    bool overlaps(const AddressInfo &other) const;
    bool operator==(const AddressInfo &other) const;
};

/**
 * Just a wrapper for source code location information.
 */
struct LocationInfo {
    std::string file;
    uint64_t line;

    // Returns just the file name, trims directory information.
    std::string getFilename(void) const;

    struct Hash {
        // We only want to hash the last part of the file to avoid 
        // hash issues when the directories differ.
        uint64_t operator()(const LocationInfo &li) const {
            return std::hash<std::string>{}(li.getFilename()) ^ 
                   std::hash<uint64_t>{}(li.line);
        }
    };

    bool operator==(const LocationInfo &other) const;
};

/**
 * Creates a map of source code location -> LLVM IR location. Then allows lookups
 * so that bugs can be mapped from trace info (which are at source level) to
 * somewhere in the IR so we can start fixing bugs.
 */
class BugLocationMapper {
private:

    std::unordered_map<LocationInfo, 
                       llvm::Instruction*, 
                       LocationInfo::Hash> locMap_;

    void insertMapping(llvm::Instruction *i);

    void createMappings(llvm::Module &m);

public:
    BugLocationMapper(llvm::Module &m) { createMappings(m); }

    llvm::Instruction *operator[](const LocationInfo &li) const 
        { return locMap_.at(li); }
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

    // Helper

    bool isOperation(void) const { 
        return type == STORE || type == FLUSH || type == FENCE; }
    bool isAssertion(void) const { 
        return type == ASSERT_PERSISTED || type == ASSERT_ORDERED; }

    std::string str() const;
};


class TraceInfo {
private:
    friend class TraceInfoBuilder;

    // Trace data.
    // -- indices where bugs live
    std::list<int> bugs_; 
    std::vector<TraceEvent> events_;

    // Metadata. For stuff like which fix generator to use.
    YAML::Node meta_;

    // Don't want direct construction of this class.
    TraceInfo() {}

    void addEvent(TraceEvent &&event);

    TraceInfo(YAML::Node m) : meta_(m) {}

public:

    const TraceEvent &operator[](int i) const { return events_[i]; }

    const std::list<int> &bugs() const { return bugs_; }

    std::string str() const;

    template<typename T>
    T getMetadata(const char *key) const { return meta_[key].as<T>(); }
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
