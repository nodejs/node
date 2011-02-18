// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_V8GLOBALS_H_
#define V8_V8GLOBALS_H_

#include "globals.h"

namespace v8 {
namespace internal {

// This file contains constants and global declarations related to the
// V8 system.

// Mask for the sign bit in a smi.
const intptr_t kSmiSignMask = kIntptrSignBit;

const int kObjectAlignmentBits = kPointerSizeLog2;
const intptr_t kObjectAlignment = 1 << kObjectAlignmentBits;
const intptr_t kObjectAlignmentMask = kObjectAlignment - 1;

// Desired alignment for pointers.
const intptr_t kPointerAlignment = (1 << kPointerSizeLog2);
const intptr_t kPointerAlignmentMask = kPointerAlignment - 1;

// Desired alignment for maps.
#if V8_HOST_ARCH_64_BIT
const intptr_t kMapAlignmentBits = kObjectAlignmentBits;
#else
const intptr_t kMapAlignmentBits = kObjectAlignmentBits + 3;
#endif
const intptr_t kMapAlignment = (1 << kMapAlignmentBits);
const intptr_t kMapAlignmentMask = kMapAlignment - 1;

// Desired alignment for generated code is 32 bytes (to improve cache line
// utilization).
const int kCodeAlignmentBits = 5;
const intptr_t kCodeAlignment = 1 << kCodeAlignmentBits;
const intptr_t kCodeAlignmentMask = kCodeAlignment - 1;

// Tag information for Failure.
const int kFailureTag = 3;
const int kFailureTagSize = 2;
const intptr_t kFailureTagMask = (1 << kFailureTagSize) - 1;


// Zap-value: The value used for zapping dead objects.
// Should be a recognizable hex value tagged as a failure.
#ifdef V8_HOST_ARCH_64_BIT
const Address kZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0xdeadbeedbeadbeef));
const Address kHandleZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1baddead0baddeaf));
const Address kFromSpaceZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1beefdad0beefdaf));
const uint64_t kDebugZapValue = V8_UINT64_C(0xbadbaddbbadbaddb);
const uint64_t kSlotsZapValue = V8_UINT64_C(0xbeefdeadbeefdeef);
#else
const Address kZapValue = reinterpret_cast<Address>(0xdeadbeef);
const Address kHandleZapValue = reinterpret_cast<Address>(0xbaddeaf);
const Address kFromSpaceZapValue = reinterpret_cast<Address>(0xbeefdaf);
const uint32_t kSlotsZapValue = 0xbeefdeef;
const uint32_t kDebugZapValue = 0xbadbaddb;
#endif


// Number of bits to represent the page size for paged spaces. The value of 13
// gives 8K bytes per page.
const int kPageSizeBits = 13;

// On Intel architecture, cache line size is 64 bytes.
// On ARM it may be less (32 bytes), but as far this constant is
// used for aligning data, it doesn't hurt to align on a greater value.
const int kProcessorCacheLineSize = 64;

// Constants relevant to double precision floating point numbers.

// Quiet NaNs have bits 51 to 62 set, possibly the sign bit, and no
// other bits set.
const uint64_t kQuietNaNMask = static_cast<uint64_t>(0xfff) << 51;
// If looking only at the top 32 bits, the QNaN mask is bits 19 to 30.
const uint32_t kQuietNaNHighBitsMask = 0xfff << (51 - 32);


// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes
// (sorted alphabetically)

class AccessorInfo;
class Allocation;
class Arguments;
class Assembler;
class AssertNoAllocation;
class BreakableStatement;
class Code;
class CodeGenerator;
class CodeStub;
class Context;
class Debug;
class Debugger;
class DebugInfo;
class Descriptor;
class DescriptorArray;
class Expression;
class ExternalReference;
class FixedArray;
class FunctionEntry;
class FunctionLiteral;
class FunctionTemplateInfo;
class NumberDictionary;
class StringDictionary;
template <typename T> class Handle;
class Heap;
class HeapObject;
class IC;
class InterceptorInfo;
class IterationStatement;
class JSArray;
class JSFunction;
class JSObject;
class LargeObjectSpace;
class LookupResult;
class MacroAssembler;
class Map;
class MapSpace;
class MarkCompactCollector;
class NewSpace;
class NodeVisitor;
class Object;
class MaybeObject;
class OldSpace;
class Property;
class Proxy;
class RegExpNode;
struct RegExpCompileData;
class RegExpTree;
class RegExpCompiler;
class RegExpVisitor;
class Scope;
template<class Allocator = FreeStoreAllocationPolicy> class ScopeInfo;
class SerializedScopeInfo;
class Script;
class Slot;
class Smi;
template <typename Config, class Allocator = FreeStoreAllocationPolicy>
    class SplayTree;
class Statement;
class String;
class Struct;
class SwitchStatement;
class AstVisitor;
class Variable;
class VariableProxy;
class RelocInfo;
class Deserializer;
class MessageLocation;
class ObjectGroup;
class TickSample;
class VirtualMemory;
class Mutex;

typedef bool (*WeakSlotCallback)(Object** pointer);

// -----------------------------------------------------------------------------
// Miscellaneous

// NOTE: SpaceIterator depends on AllocationSpace enumeration values being
// consecutive.
enum AllocationSpace {
  NEW_SPACE,            // Semispaces collected with copying collector.
  OLD_POINTER_SPACE,    // May contain pointers to new space.
  OLD_DATA_SPACE,       // Must not have pointers to new space.
  CODE_SPACE,           // No pointers to new space, marked executable.
  MAP_SPACE,            // Only and all map objects.
  CELL_SPACE,           // Only and all cell objects.
  LO_SPACE,             // Promoted large objects.

  FIRST_SPACE = NEW_SPACE,
  LAST_SPACE = LO_SPACE,
  FIRST_PAGED_SPACE = OLD_POINTER_SPACE,
  LAST_PAGED_SPACE = CELL_SPACE
};
const int kSpaceTagSize = 3;
const int kSpaceTagMask = (1 << kSpaceTagSize) - 1;


// A flag that indicates whether objects should be pretenured when
// allocated (allocated directly into the old generation) or not
// (allocated in the young generation if the object size and type
// allows).
enum PretenureFlag { NOT_TENURED, TENURED };

enum GarbageCollector { SCAVENGER, MARK_COMPACTOR };

enum Executability { NOT_EXECUTABLE, EXECUTABLE };

enum VisitMode { VISIT_ALL, VISIT_ALL_IN_SCAVENGE, VISIT_ONLY_STRONG };

// Flag indicating whether code is built into the VM (one of the natives files).
enum NativesFlag { NOT_NATIVES_CODE, NATIVES_CODE };


// A CodeDesc describes a buffer holding instructions and relocation
// information. The instructions start at the beginning of the buffer
// and grow forward, the relocation information starts at the end of
// the buffer and grows backward.
//
//  |<--------------- buffer_size ---------------->|
//  |<-- instr_size -->|        |<-- reloc_size -->|
//  +==================+========+==================+
//  |   instructions   |  free  |    reloc info    |
//  +==================+========+==================+
//  ^
//  |
//  buffer

struct CodeDesc {
  byte* buffer;
  int buffer_size;
  int instr_size;
  int reloc_size;
  Assembler* origin;
};


// Callback function on object slots, used for iterating heap object slots in
// HeapObjects, global pointers to heap objects, etc. The callback allows the
// callback function to change the value of the slot.
typedef void (*ObjectSlotCallback)(HeapObject** pointer);


// Callback function used for iterating objects in heap spaces,
// for example, scanning heap objects.
typedef int (*HeapObjectCallback)(HeapObject* obj);


// Callback function used for checking constraints when copying/relocating
// objects. Returns true if an object can be copied/relocated from its
// old_addr to a new_addr.
typedef bool (*ConstraintCallback)(Address new_addr, Address old_addr);


// Callback function on inline caches, used for iterating over inline caches
// in compiled code.
typedef void (*InlineCacheCallback)(Code* code, Address ic);


// State for inline cache call sites. Aliased as IC::State.
enum InlineCacheState {
  // Has never been executed.
  UNINITIALIZED,
  // Has been executed but monomorhic state has been delayed.
  PREMONOMORPHIC,
  // Has been executed and only one receiver type has been seen.
  MONOMORPHIC,
  // Like MONOMORPHIC but check failed due to prototype.
  MONOMORPHIC_PROTOTYPE_FAILURE,
  // Multiple receiver types have been seen.
  MEGAMORPHIC,
  // Special states for debug break or step in prepare stubs.
  DEBUG_BREAK,
  DEBUG_PREPARE_STEP_IN
};


enum CheckType {
  RECEIVER_MAP_CHECK,
  STRING_CHECK,
  NUMBER_CHECK,
  BOOLEAN_CHECK
};


enum InLoopFlag {
  NOT_IN_LOOP,
  IN_LOOP
};


enum CallFunctionFlags {
  NO_CALL_FUNCTION_FLAGS = 0,
  RECEIVER_MIGHT_BE_VALUE = 1 << 0  // Receiver might not be a JSObject.
};


enum InlineCacheHolderFlag {
  OWN_MAP,  // For fast properties objects.
  PROTOTYPE_MAP  // For slow properties objects (except GlobalObjects).
};


// Type of properties.
// Order of properties is significant.
// Must fit in the BitField PropertyDetails::TypeField.
// A copy of this is in mirror-debugger.js.
enum PropertyType {
  NORMAL              = 0,  // only in slow mode
  FIELD               = 1,  // only in fast mode
  CONSTANT_FUNCTION   = 2,  // only in fast mode
  CALLBACKS           = 3,
  INTERCEPTOR         = 4,  // only in lookup results, not in descriptors.
  MAP_TRANSITION      = 5,  // only in fast mode
  CONSTANT_TRANSITION = 6,  // only in fast mode
  NULL_DESCRIPTOR     = 7,  // only in fast mode
  // All properties before MAP_TRANSITION are real.
  FIRST_PHANTOM_PROPERTY_TYPE = MAP_TRANSITION,
  // There are no IC stubs for NULL_DESCRIPTORS. Therefore,
  // NULL_DESCRIPTOR can be used as the type flag for IC stubs for
  // nonexistent properties.
  NONEXISTENT = NULL_DESCRIPTOR
};


// Whether to remove map transitions and constant transitions from a
// DescriptorArray.
enum TransitionFlag {
  REMOVE_TRANSITIONS,
  KEEP_TRANSITIONS
};


// Union used for fast testing of specific double values.
union DoubleRepresentation {
  double  value;
  int64_t bits;
  DoubleRepresentation(double x) { value = x; }
};


// Union used for customized checking of the IEEE double types
// inlined within v8 runtime, rather than going to the underlying
// platform headers and libraries
union IeeeDoubleLittleEndianArchType {
  double d;
  struct {
    unsigned int man_low  :32;
    unsigned int man_high :20;
    unsigned int exp      :11;
    unsigned int sign     :1;
  } bits;
};


union IeeeDoubleBigEndianArchType {
  double d;
  struct {
    unsigned int sign     :1;
    unsigned int exp      :11;
    unsigned int man_high :20;
    unsigned int man_low  :32;
  } bits;
};


// AccessorCallback
struct AccessorDescriptor {
  MaybeObject* (*getter)(Object* object, void* data);
  MaybeObject* (*setter)(JSObject* object, Object* value, void* data);
  void* data;
};


// Logging and profiling.
// A StateTag represents a possible state of the VM.  When compiled with
// ENABLE_VMSTATE_TRACKING, the logger maintains a stack of these.
// Creating a VMState object enters a state by pushing on the stack, and
// destroying a VMState object leaves a state by popping the current state
// from the stack.

#define STATE_TAG_LIST(V) \
  V(JS)                   \
  V(GC)                   \
  V(COMPILER)             \
  V(OTHER)                \
  V(EXTERNAL)

enum StateTag {
#define DEF_STATE_TAG(name) name,
  STATE_TAG_LIST(DEF_STATE_TAG)
#undef DEF_STATE_TAG
  // Pseudo-types.
  state_tag_count
};


// -----------------------------------------------------------------------------
// Macros

// Testers for test.

#define HAS_SMI_TAG(value) \
  ((reinterpret_cast<intptr_t>(value) & kSmiTagMask) == kSmiTag)

#define HAS_FAILURE_TAG(value) \
  ((reinterpret_cast<intptr_t>(value) & kFailureTagMask) == kFailureTag)

// OBJECT_POINTER_ALIGN returns the value aligned as a HeapObject pointer
#define OBJECT_POINTER_ALIGN(value)                             \
  (((value) + kObjectAlignmentMask) & ~kObjectAlignmentMask)

// POINTER_SIZE_ALIGN returns the value aligned as a pointer.
#define POINTER_SIZE_ALIGN(value)                               \
  (((value) + kPointerAlignmentMask) & ~kPointerAlignmentMask)

// MAP_POINTER_ALIGN returns the value aligned as a map pointer.
#define MAP_POINTER_ALIGN(value)                                \
  (((value) + kMapAlignmentMask) & ~kMapAlignmentMask)

// CODE_POINTER_ALIGN returns the value aligned as a generated code segment.
#define CODE_POINTER_ALIGN(value)                               \
  (((value) + kCodeAlignmentMask) & ~kCodeAlignmentMask)

// Support for tracking C++ memory allocation.  Insert TRACK_MEMORY("Fisk")
// inside a C++ class and new and delete will be overloaded so logging is
// performed.
// This file (globals.h) is included before log.h, so we use direct calls to
// the Logger rather than the LOG macro.
#ifdef DEBUG
#define TRACK_MEMORY(name) \
  void* operator new(size_t size) { \
    void* result = ::operator new(size); \
    Logger::NewEvent(name, result, size); \
    return result; \
  } \
  void operator delete(void* object) { \
    Logger::DeleteEvent(name, object); \
    ::operator delete(object); \
  }
#else
#define TRACK_MEMORY(name)
#endif


// Feature flags bit positions. They are mostly based on the CPUID spec.
// (We assign CPUID itself to one of the currently reserved bits --
// feel free to change this if needed.)
// On X86/X64, values below 32 are bits in EDX, values above 32 are bits in ECX.
enum CpuFeature { SSE4_1 = 32 + 19,  // x86
                  SSE3 = 32 + 0,     // x86
                  SSE2 = 26,   // x86
                  CMOV = 15,   // x86
                  RDTSC = 4,   // x86
                  CPUID = 10,  // x86
                  VFP3 = 1,    // ARM
                  ARMv7 = 2,   // ARM
                  SAHF = 0};   // x86

// The Strict Mode (ECMA-262 5th edition, 4.2.2).
enum StrictModeFlag {
  kNonStrictMode,
  kStrictMode
};

} }  // namespace v8::internal

#endif  // V8_V8GLOBALS_H_
