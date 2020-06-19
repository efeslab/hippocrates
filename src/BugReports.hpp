#pragma once
/**
 * Bug reports contain two things:
 */

#include <vector>

#include "yaml-cpp/yaml.h"

namespace pmfix {

class TraceEvent {

private:

public:

};

class BugEvent : TraceEvent {

};

class TraceInfo {
private:
    friend class TraceInfoBuilder;

    // Don't want direct construction of this class.
    TraceInfo() {}



public:
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
