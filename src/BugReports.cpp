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

bool AddressInfo::canAdd(const AddressInfo &other) const {
    if (length == 0) {
        assert(!address && "bad construction!");
        return true;
    }

    // Combine ranges, ensure no gaps.
    if (other.address < address) {
       return other.end() + 1 >= address;
    } else {
       return end() + 1 >= other.address;
    }
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
        if (end() + 1 < other.address) {
            errs() << str() << " < " << other.str() << "\n";
        }
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

bool FixLoc::Compare::operator()(const FixLoc &a, const FixLoc &b) const {
    return a.first < b.first;
}

bool FixLoc::isValid() const {
    if (!first || !last) return false;
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

std::list<Instruction*> FixLoc::insts() const {
    std::list<Instruction*> ilist;
    if (!isValid()) return ilist;

    for (Instruction *t = first; t != last->getNextNonDebugInstruction();
         t = t->getNextNonDebugInstruction()) ilist.push_back(t);
    
    return ilist;
}

std::string FixLoc::str() const {
    std::string str;
    llvm::raw_string_ostream ss(str);

    ss << "<FixLoc>\n";
    ss << "\tFunction:\t" << first->getFunction()->getName() << "\n";
    ss << "\tSource Location: " << dbgLoc.str() << "\n";
    ss << "\tInstructions:\n";
    for (Instruction *i : insts()) {
        ss << "\t\t" << *i << "\n";
    }
    ss << "</FixLoc>\n";

    return ss.str();
}

#pragma endregion

#pragma region BugLocationMapper

std::unique_ptr<BugLocationMapper> BugLocationMapper::instance(nullptr);

BugLocationMapper &BugLocationMapper::getInstance(Module &m) {
    if (!instance) {
        instance.reset(new BugLocationMapper(m));
    }

    return *instance;
}

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

            locs.emplace_back(first, last, location);
        }

        fixLocMap_[location] = locs;
    }

    // errs() << "fix map size: " << fixLocMap_.size() << "\n";
    // const char fnHack[] = "do_slabs_free";
    // errs() << *m.getFunction(fnHack) << "\n";
    // for (auto &p : fixLocMap_) {
    //     if (p.first.function == fnHack) {
    //         errs() << p.first.str() << "\n";
    //         for (FixLoc &fl : p.second) {
    //             errs() << "\t" << fl.str() << "\n";
    //         }
    //     }
    // }
    // errs() << "inst map size " << locMap_.size() << "\n";
    // for (auto &p : locMap_) {
    //     if (p.first.function == fnHack) {
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

static list<Value*> getGenericPmValues(
    const BugLocationMapper &mapper, const FixLoc &fLoc) {
    
    list<Value*> values;
    for (Instruction *i : fLoc.insts()) {
        errs() << "GET PMV FIRST:" << *i << "\n";
    }


    for (Instruction *i : fLoc.insts()) {
        errs() << "GET PMV:" << *i << "\n";
        if (auto *cb = dyn_cast<CallBase>(i)) {

            Function *intr = cb->getCalledFunction();
            if (intr) {
                switch (intr->getIntrinsicID()) {
                    case Intrinsic::memcpy:
                    case Intrinsic::memset:
                    case Intrinsic::memmove:
                        values.push_back(cb->getArgOperand(0));
                        continue;
                    default: 
                        break;
                }
            }
            
            const Function *f = utils::getFlush(cb);
            if (f) {
                values.push_back(cb->getArgOperand(0));
                continue;
            } 
            
            if (auto *ci = dyn_cast<CallInst>(cb)) {
                if (ci->isInlineAsm()) {
                    bool isFlushAsm = false;
                    // Get the inline ASM string
                    auto *ia = dyn_cast<InlineAsm>(ci->getCalledValue());
                    assert(ia);
                    auto str = ia->getAsmString();
                    // errs() << str << "\n";
                    if (str == ".byte 0x66; xsaveopt $0") {
                        isFlushAsm = true;
                    } 
                    // else {
                    //     errs() << "'" << str << "' != " << ".byte 0x66; xsaveopt $0" << "\n";
                    // }
                    // Check if it is
                    if (isFlushAsm) {
                        values.push_back(cb->getArgOperand(0));
                    }
                }
            }
        } else if (auto *si = dyn_cast<StoreInst>(i)) {
            /**
             * This could be a VALGRIND_DO_FLUSH, so we should
             * see if there's the magic number for the request.
             */
            // errs() << *si << "\n";
            Value *v = si->getValueOperand();
            if (auto *ci = dyn_cast<ConstantInt>(v)) {
                if (ci->getZExtValue() == 1346568197) {
                    // errs() << "We found valgrind value!\n";
                    // The very next instruction should have what we want.
                    Instruction *curr = si->getNextNonDebugInstruction();
                    StoreInst *pmStore = dyn_cast<StoreInst>(curr);
                    while (!pmStore) {
                        curr = curr->getNextNonDebugInstruction();
                        assert(curr);
                        pmStore = dyn_cast<StoreInst>(curr);
                    }
                    assert(pmStore);
                    // errs() << "PM store:" << *pmStore << "\n"; 
                    // errs() << "\tAddr:" << *pmStore->getValueOperand() << "\n";
                    Value *pmAddr = pmStore->getValueOperand();
                    if (!pmAddr->getType()->isPointerTy()) {
                        if (auto *pi = dyn_cast<PtrToIntInst>(pmAddr)) {
                            pmAddr = pi->getPointerOperand();
                        }
                    }
                    // errs() << "\tAddr (no cast):" << *pmAddr << "\n";
                    assert(pmAddr->getType()->isPointerTy());
                    values.push_back(pmAddr);
                    continue;
                }
                // Fall-through, looking for magic VALGRIND number
            }

            // return si->getPointerOperand();
            values.push_back(si->getPointerOperand());
            // Fall-through, we already checked for valgrind
        } else if (auto *cx = dyn_cast<AtomicCmpXchgInst>(i)) {
            // return cx->getPointerOperand();
            values.push_back(cx->getPointerOperand());
        }
    }

    return values;                      
}

std::list<Value*> TraceEvent::pmValues(const BugLocationMapper &mapper) const {
    std::list<Value*> pmAddrs;

    // if (location != callstack[0]) {
    //     errs() << str() << "\n";
    // }
    // assert(location == callstack[0] && "wat");

    if (!mapper.contains(location)) return pmAddrs;
    if (type == FENCE || type == ASSERT_PERSISTED ||
        type == ASSERT_ORDERED || type == REQUIRED_FLUSH) return pmAddrs;

    assert(type != INVALID && "doesn't make sense!");

    // errs() << "\n\nPMVALUES\n\n";
    // for (auto &fLoc : mapper[location]) {
    //     errs() << "New FLOC\n";
    //     for (Instruction *i : fLoc.insts()) errs() << "\t" << *i << "\n";
    // }

    for (auto &fLoc : mapper[location]) {
        // errs() << fLoc.str() << "\n";
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
                        // errs() << "Try get store value!\n";
                        // errs() << str() << "\n";
                        for (Value *pmv : getGenericPmValues(mapper, fLoc)) {
                            if (pmv) pmAddrs.push_back(pmv);
                        }
                        break;
                    }  
                    case FLUSH: {
                        // errs() << "Try get flush value!\n";
                        // errs() << str() << "\n";
                        for (Value *pmv : getGenericPmValues(mapper, fLoc)) {
                            if (pmv) pmAddrs.push_back(pmv);
                        }
                        break;
                    }        
                    default:
                        errs() << "DEFAULT\n";
                        errs() << str() << "\n";
                        for (auto &ai : addresses) {
                            errs() << ai.str() << "\n";
                        }
                        errs() << "FIRST:" << *fLoc.first << 
                            "\nLAST:" << *fLoc.last << "\n";
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

    // if (pmAddrs.empty()) {
    //     errs() << "EMPTY: " << str() << "\n";
    // }

    assert(!pmAddrs.empty() && "bad usage!");

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

    /**
     * Sanity checking.
     */
    assert(e.callstack[0] == e.location);

    ti.addEvent(std::move(e));
}

void TraceInfoBuilder::resolveLocations(TraceEvent &te) {

    std::vector<LocationInfo> &stack = te.callstack;

    // [0] is the current location, which we use to set up the node itself.
    for (int i = stack.size() - 1; i >= 1; --i) {
        LocationInfo &caller = stack[i];
        LocationInfo &callee = stack[i-1];

        // errs() << "\nCALLER [" << i << "]: " << caller.str() << "\n";
        // errs() << "CALLEE [" << (i-1) <<"]: " << callee.str() << "\n";
        
        if (!caller.valid() || !mapper_.contains(caller)) {
            // errs() << "SKIP: " << caller.valid() << " " << 
            //     mapper_.contains(caller) << "\n";
            // Function *f = mapper_.module().getFunction(caller.function);
            // if (!f) {
            //     errs() << "\tnull!\n";
            //     f = mapper_.module().getFunction("obj_alloc_construct.1488");
            //     if (!f) errs() << "\t\talso null!\n";
            //     else errs() << *f << "\n";
            //     errs() << "DEMANGLES: " << utils::demangle("obj_alloc_construct.1488") << "\n";
            // }
            // else errs() << *f << "\n";

            continue;
        }

        if (mapper_.contains(callee)) {
            // No reason to repair.
            continue;
        }

        // The location in the caller calls the function of the callee

        std::list<CallBase*> possibleCallSites;

        for (auto &fLoc : mapper_[caller]) {
            assert(fLoc.isValid() && "wat");
            // errs() << "START LOC: \n";
            // errs() << "\tFUNC NAME: "<< fLoc.insts().front()->getFunction()->getName() << "\n";
            for (Instruction *inst : fLoc.insts()) {
                errs() << *inst << "\n";
                if (auto *cb = dyn_cast<CallBase>(inst)) {
                    // errs() << *inst << "\n";
                    Function *f = cb->getCalledFunction();
                    if (f) {
                        if (f->getIntrinsicID() == Intrinsic::dbg_declare) {
                            continue;
                        }

                        std::string fname = utils::demangle(f->getName().data());
                        if (fname.find(callee.function) == std::string::npos) {
                            // errs() << fname << " !find " << callee.function << "\n";
                            if (fname.find("memset") == std::string::npos &&
                                fname.find("memcpy") == std::string::npos &&
                                fname.find("memmove") == std::string::npos &&
                                fname.find("strncpy") == std::string::npos) {
                                continue;
                            }
                        }
                    } 

                    // errs() << "POSSIBLE: " << *cb << "\n";
                    possibleCallSites.push_back(cb);
                }
            }
        }

        if (possibleCallSites.empty()) {
            errs() << "No calls to " << callee.function << "!\n";
        }

        assert(possibleCallSites.size() > 0 && "don't know how to handle!");
        if (possibleCallSites.size() > 1) {
            // If the functions are both the same, it shouldn't matter, 
            // since we're only reseting on the front of the path.
            Function *f = possibleCallSites.front()->getCalledFunction();
            for (auto *cb : possibleCallSites) {
                Function *called = cb->getCalledFunction();
                assert(called && called == f);
                // errs() << "Multiple call sites:" << *cb << "\n";
            }
            // We should be able to do something about this with debug info
            // errs() << "Too many options! Abort.\n";
            // assert(false && "TODO!");
            // return nullptr;
        }
        // assert(possibleCallSites.size() == 1 && "don't know how to handle!");

        Instruction *possible = possibleCallSites.front();
        CallBase *callInst = dyn_cast<CallBase>(possible);
        assert(callInst && "don't know how to handle a non-call!");

        Function *f = callInst->getCalledFunction();
        if (!f) {
            errs() << "Try get function pointer function (" << callee.function << ")\n";
            f = mapper_.module().getFunction(callee.function);
            if (!f) {
                std::list<Function*> fnCandidates;
                for (Function &fn : mapper_.module()) {
                    std::string fnName(fn.getName().data());
                    if (fnName.find(callee.function) != std::string::npos) {
                        // Skip false matches
                        auto ending = fnName.substr(fnName.find(callee.function) + callee.function.size());
                        if (ending[0] != '.') continue; // name mangling
                        errs() << "\t\t--- " << fnName << "\n"; 
                        fnCandidates.push_back(&fn);
                    }
                }
                assert(fnCandidates.size() == 1 && "wat");
                f = fnCandidates.front();
            }
        }
        assert(f && "don't know what's going on!!");

        if (f->getName() != callee.function) {
            callee.function = f->getName();
        }
    }

    /**
     * Now, we set up arguments so we can call the other create() function.
     */ 

    if (stack[0] != te.location) {
        te.location = stack[0];
    }
}

TraceInfo TraceInfoBuilder::build(void) {
    TraceInfo ti(doc_["metadata"]);

    auto trace = doc_["trace"];
    assert(trace.IsSequence() && "Don't know what to do otherwise!");
    for (size_t i = 0; i < trace.size(); ++i) {
        processEvent(ti, trace[i]);
    }

    for (size_t i = 0; i < ti.size(); ++i) {
        resolveLocations(ti[i]);
    }

    return ti;
}

#pragma endregion