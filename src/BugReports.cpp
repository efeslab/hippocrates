#include "BugReports.hpp"
#include "PassUtils.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unistd.h>

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/OrderedBasicBlock.h"

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
    return start() <= other.end() && other.start() <= end();
}

bool AddressInfo::contains(const AddressInfo &other) const {
    return start() <= other.start() && end() >= other.end();
}

bool AddressInfo::operator==(const AddressInfo &other) const {
    return address == other.address && length == other.length;
}

void AddressInfo::operator+=(const AddressInfo &other) {
    if (length == 0) {
        assert(!address && "bad construction!");
        address = other.address;
        length = other.length;
        return;
    }

    // Combine ranges, ensure no gaps.
    if (other.address < address) {
        assert(other.end() + 1 >= address && "bad range!");
    } else {
        assert(end() + 1 >= other.address && "bad range!");
    }

    uint64_t newAddress = std::min(other.address, address);
    uint64_t newLength = std::max(other.end(), end()) + 1 - newAddress;

    address = newAddress;
    length = newLength;
}

#pragma endregion

#pragma region LocationInfo

std::string LocationInfo::getFilename(void) const {
    size_t pos = file.find_last_of("/");
    if (pos == std::string::npos) return file;
    return file.substr(pos+1);
}

bool LocationInfo::operator==(const LocationInfo &other) const {
    if (function != other.function) return false;
    if (line != other.line) return false;

    // We want more of a partial match, since the file name strings (directories)
    // can vary. So if the shorter string fits in the longer, it's good enough.
    size_t pos = string::npos;
    if (file.size() < other.file.size()) {
        pos = other.file.find(file);
    } else {
        pos = file.find(other.file);
    }

    // if (pos != string::npos && "memset_mov_sse2_empty" == function) {
    //     errs() << "EQ: " << str() << " == " << other.str() << "\n";
    // }

    return pos != string::npos;
}

std::string LocationInfo::str() const {
    std::stringstream buffer;

    buffer << "<LocationInfo: " << function << " @ ";
    buffer << file << ":" << line << ">";

    return buffer.str();
}

#pragma endregion

#pragma region FixLoc

uint64_t FixLoc::Hash::operator()(const FixLoc &fl) const {
    return std::hash<void*>{}((void*)fl.first) 
            ^ std::hash<void*>{}((void*)fl.last); 
}

bool FixLoc::isValid() const {
    // Make sure there are all in the same function
    bool sameFn = first->getFunction() == last->getFunction();
    // Are they all in the same basic block, do we dream?
    bool sameBB = first->getParent() == last->getParent();
    return sameFn && sameBB;
}

const FixLoc &FixLoc::NullLoc() {
    static FixLoc empty = FixLoc();
    return empty;
}

#pragma endregion

#pragma region BugLocationMapper

void BugLocationMapper::insertMapping(Instruction *i) {
    // Essentially, need to get the line number and file name from the 
    // instruction debug information.
    if (!i->hasMetadata()) return;
    if (!i->getMetadata("dbg")) return;

    if (DILocation *di = dyn_cast<DILocation>(i->getMetadata("dbg"))) {
        LocationInfo li;
        li.function = i->getFunction()->getName();
        li.line = di->getLine();

        DILocalScope *ls = di->getScope();
        DIFile *df = ls->getFile();
        li.file = df->getFilename();

        // if ("memset_mov_sse2_empty" == li.function) {
        //     errs() << "DBG: " << li.str() << " ===== " << *i << '\n';
        // }

        // Now, there may already be a mapping, but it should be the previous 
        // instruction. They aren't guaranteed to be in the same basic block, 
        // but everything should be in the right function context.
        // if (locMap_.count(li)) {
        //     if (locMap_[li]->getParent() != i->getParent()) {
        //         errs() << "ERROR\n";
        //         errs() << li.str() << '\n';
        //         errs() << "Old: " << *locMap_[li] << "\n";
        //         errs() << "New: " << *i << "\n";
        //         errs() << "Context: " << *i->getFunction() << "\n";
        //     }
        //     assert(locMap_[li]->getParent() == i->getParent() && 
        //            "Assumptions violated, instructions not in same basic block!");
        // }
        locMap_[li].push_back(i);
    }
}

void BugLocationMapper::createMappings(Module &m) {
    for (Function &f : m) {
        for (BasicBlock &b : f) {
            for (Instruction &i : b) {
                // Ignore instructions we don't care too much about.
                // if (!isa<StoreInst>(&i) && !isa<CallBase>(&i)) continue;
                // Turns out we DO care.
                insertMapping(&i);
            }
        }
    }

    assert(locMap_.size() && "no debug information found!!!");

    /**
     * Now, we do the fix mapping.
     */
    for (auto &p : locMap_) {
        auto &location = p.first;
        auto &instructions = p.second;

        std::unordered_map<BasicBlock*, std::list<Instruction*>> blocks;
        for (Instruction *q : instructions) {
            if (auto *cb = dyn_cast<CallBase>(q)) {
                Function *f = cb->getCalledFunction();
                if (f && f->getIntrinsicID() == Intrinsic::dbg_declare) {
                    continue;
                }
            }

            blocks[q->getParent()].push_back(q);
        }

        std::list<FixLoc> locs;
        for (auto &p : blocks) {
            auto &insts = p.second;
            assert(insts.size() && "wat");
            // Need to find the first and last instruction
            Instruction *first = insts.front();
            Instruction *last = insts.back();

            OrderedBasicBlock obb(p.first);

            for (auto *ii : insts) {
                if (obb.dominates(ii, first)) first = ii;
                if (obb.dominates(last, ii)) last = ii;
            }

            locs.emplace_back(first, last);
        }

        fixLocMap_[location] = locs;
    }

    // errs() << "fix map\n";
    // for (auto &p : fixLocMap_) {
    //     if (p.first.function == "clht_gc_thread_init") {
    //         errs() << p.first.str() << "\n";
    //     }
    // }
    // errs() << "inst map\n";
    // for (auto &p : locMap_) {
    //     if (p.first.function == "clht_gc_thread_init") {
    //         errs() << p.first.str() << "\n";
    //     }
    // }
    // assert(false);
    
    assert(!fixLocMap_.empty() && "wat");
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
    if (typeString == "required_flush") return TraceEvent::REQUIRED_FLUSH;
    
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
    buffer << "\tLocation: " << location.str() << '\n';
    if (addresses.size()) {
        buffer << "\tAddress Info:\n";
        for (const auto &ai : addresses) {
            buffer << "\t\tAddress: " << int_to_hex(ai.address) << '\n';
            buffer << "\t\tLength: " << ai.length << '\n';
        }
    }
    buffer << "\tCall Stack:\n";
    int i = 0;
    for (const LocationInfo &li : callstack) {
        buffer << "[" << i << "] " << li.str() << '\n';
        i++;
    }

    return buffer.str();
}

bool TraceEvent::callStacksEqual(const TraceEvent &a, const TraceEvent &b) {
    if (a.callstack.size() != b.callstack.size()) {
        return false;
    }

    for (int i = 0; i < a.callstack.size(); i++) {
        const LocationInfo &la = a.callstack[i];
        const LocationInfo &lb = b.callstack[i];
        errs() << "\t\t" << la.str() << " ?= " << lb.str() << '\n';
        if (la.function != lb.function) return false;
        if (la.file != lb.file) return false;
        if (i > 0 && la.line != lb.line) return false;
    }

    return true;
}

static Value *getGenericPmValues(const BugLocationMapper &mapper, const FixLoc &fLoc) {
    Instruction *i = nullptr;
    for (i = fLoc.first; i != fLoc.last; i = i->getNextNonDebugInstruction()) {
        if (auto *cb = dyn_cast<CallBase>(i)) {
            Function *f = utils::getFlush(cb);
            if (f) {
                return cb->getArgOperand(0);
            } 
            
            if (auto *ci = dyn_cast<CallInst>(cb)) {
                if (ci->isInlineAsm()) {
                    bool isFlushAsm = false;
                    // Get the inline ASM string
                    auto *ia = dyn_cast<InlineAsm>(ci->getCalledValue());
                    assert(ia);
                    auto str = ia->getAsmString();
                    errs() << str << "\n";
                    if (str == ".byte 0x66; xsaveopt $0") {
                        isFlushAsm = true;
                    } else {
                        errs() << "'" << str << "' != " << ".byte 0x66; xsaveopt $0" << "\n";
                    }
                    // Check if it is
                    if (isFlushAsm) {
                        return cb->getArgOperand(0);
                    }
                }
            }
        } else if (auto *si = dyn_cast<StoreInst>(i)) {
            /**
             * This could be a VALGRIND_DO_FLUSH, so we should
             * see if there's the magic number for the request.
             */
            errs() << *si << "\n";
            Value *v = si->getValueOperand();
            if (auto *ci = dyn_cast<ConstantInt>(v)) {
                if (ci->getZExtValue() == 1346568197) {
                    errs() << "We found valgrind value!\n";
                    // The very next instruction should have what we want.
                    Instruction *curr = si->getNextNonDebugInstruction();
                    StoreInst *pmStore = dyn_cast<StoreInst>(curr);
                    while (!pmStore) {
                        curr = curr->getNextNonDebugInstruction();
                        assert(curr);
                        pmStore = dyn_cast<StoreInst>(curr);
                    }
                    assert(pmStore);
                    errs() << "PM store:" << *pmStore << "\n"; 
                    errs() << "\tAddr:" << *pmStore->getValueOperand() << "\n";
                    Value *pmAddr = pmStore->getValueOperand();
                    if (!pmAddr->getType()->isPointerTy()) {
                        if (auto *pi = dyn_cast<PtrToIntInst>(pmAddr)) {
                            pmAddr = pi->getPointerOperand();
                        }
                    }
                    errs() << "\tAddr (no cast):" << *pmAddr << "\n";
                    assert(pmAddr->getType()->isPointerTy());
                    return pmAddr;
                }
                // Fall-through, looking for magic VALGRIND number
            }
            // Fall-through, we already checked for valgrind
        }
    }

    return nullptr;                      
}

std::list<Value*> TraceEvent::pmValues(const BugLocationMapper &mapper) const {
    std::list<Value*> pmAddrs;

    // if (location != callstack[0]) {
    //     errs() << str() << "\n";
    // }
    // assert(location == callstack[0] && "wat");
    assert(mapper.contains(location) && "wat");
    assert(type != INVALID && type != FENCE && "doesn't make sense!");

    for (auto &fLoc : mapper[location]) {
        switch (source) {
            case PMTEST: {
                switch (type) {
                    case STORE:
                    case FLUSH: {
                        auto *cb = dyn_cast<CallBase>(fLoc.last);
                        assert(cb && "bad trace!");
                        Function *f = cb->getCalledFunction();
                        assert(f && "bad trace!");
                        assert(f->getName() == "C_createMetadata_Flush" ||
                               f->getName() == "C_createMetadata_Assign");
                        // The second operand is the address.
                        pmAddrs.push_back(cb->getArgOperand(1));
                        break;
                    }
                    default:
                        assert(false && "wat");
                }
                break;
            }
            case GENERIC: {
                switch (type) {
                    case STORE: {
                        assert(false && "DO ME");
                        auto *cb = dyn_cast<CallBase>(fLoc.last);
                        assert(cb && "bad trace!");
                        Function *f = utils::getFlush(cb);
                        assert(f && "bad trace!");
                        pmAddrs.push_back(cb->getArgOperand(0));
                        break;
                    }  
                    case FLUSH: {
                        Value *pmv = getGenericPmValues(mapper, fLoc);
                        assert(pmv && "bad trace!");
                        pmAddrs.push_back(pmv);
                        break;
                    }        
                    default:
                        for (auto &ai : addresses) {
                            errs() << ai.str() << "\n";
                        }
                        errs() << "FIRST:" << *fLoc.first << 
                            "\nLAST:" << fLoc.last << "\n";
                        assert(false && "wat");
                        break;
                }
                break;
            }
            default: {
                assert(false && "wat");
                break;
            }
        }
    }

    return pmAddrs;
}

#pragma endregion

#pragma region TraceInfo

TraceInfo::TraceInfo(YAML::Node m) : meta_(m), source_(TraceEvent::UNKNOWN) {
    std::string bugReportSrc = getMetadata<std::string>("source");
    if ("PMTEST" == bugReportSrc) {
        source_ = TraceEvent::PMTEST;
    } else if ("GENERIC" == bugReportSrc) {
        source_ = TraceEvent::GENERIC;
    }
}

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
    e.source = ti.getSource();
    e.typeString = event["event"].as<string>();

    TraceEvent::Type event_type = TraceEvent::getType(e.typeString);

    assert(event_type != TraceEvent::INVALID);
    
    e.type = event_type;
    e.timestamp = event["timestamp"].as<uint64_t>();
    e.location.function = event["function"].as<string>();
    e.location.file = event["file"].as<string>();
    e.location.line = event["line"].as<int64_t>();
    e.isBug = event["is_bug"].as<bool>();

    assert(event["stack"].IsSequence() && "Don't know what to do!");
    for (size_t i = 0; i < event["stack"].size(); ++i) {
        YAML::Node sf = event["stack"][i];
        LocationInfo li;
        li.function = sf["function"].as<string>();
        li.file = sf["file"].as<string>();
        li.line = sf["line"].as<int64_t>();
        e.callstack.emplace_back(li);
    }

    switch (e.type) {
        case TraceEvent::STORE:
        case TraceEvent::FLUSH:
        case TraceEvent::ASSERT_PERSISTED:
        case TraceEvent::REQUIRED_FLUSH: {
            AddressInfo ai;
            ai.address = event["address"].as<uint64_t>();
            ai.length = event["length"].as<uint64_t>();
            e.addresses.push_back(ai);
            break;
        }    
        case TraceEvent::ASSERT_ORDERED: {
            AddressInfo a, b;
            a.address = event["address_a"].as<uint64_t>();
            a.length = event["length_a"].as<uint64_t>();
            b.address = event["address_b"].as<uint64_t>();
            b.length = event["length_b"].as<uint64_t>();
            e.addresses.push_back(a);
            e.addresses.push_back(b);
            break;
        } 
        default:
            break;
    }

    ti.addEvent(std::move(e));
}

TraceInfo TraceInfoBuilder::build(void) {
    TraceInfo ti(doc_["metadata"]);

    auto trace = doc_["trace"];
    assert(trace.IsSequence() && "Don't know what to do otherwise!");
    for (size_t i = 0; i < trace.size(); ++i) {
        processEvent(ti, trace[i]);
    }

    return ti;
}

#pragma endregion