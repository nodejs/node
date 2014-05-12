// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8GLOBALS_H_
#define V8_V8GLOBALS_H_

#include "globals.h"
#include "checks.h"

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

// Desired alignment for double values.
const intptr_t kDoubleAlignment = 8;
const intptr_t kDoubleAlignmentMask = kDoubleAlignment - 1;

// Desired alignment for generated code is 32 bytes (to improve cache line
// utilization).
const int kCodeAlignmentBits = 5;
const intptr_t kCodeAlignment = 1 << kCodeAlignmentBits;
const intptr_t kCodeAlignmentMask = kCodeAlignment - 1;

// Tag information for Failure.
// TODO(yangguo): remove this from space owner calculation.
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
const Address kGlobalHandleZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1baffed00baffedf));
const Address kFromSpaceZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1beefdad0beefdaf));
const uint64_t kDebugZapValue = V8_UINT64_C(0xbadbaddbbadbaddb);
const uint64_t kSlotsZapValue = V8_UINT64_C(0xbeefdeadbeefdeef);
const uint64_t kFreeListZapValue = 0xfeed1eaffeed1eaf;
#else
const Address kZapValue = reinterpret_cast<Address>(0xdeadbeef);
const Address kHandleZapValue = reinterpret_cast<Address>(0xbaddeaf);
const Address kGlobalHandleZapValue = reinterpret_cast<Address>(0xbaffedf);
const Address kFromSpaceZapValue = reinterpret_cast<Address>(0xbeefdaf);
const uint32_t kSlotsZapValue = 0xbeefdeef;
const uint32_t kDebugZapValue = 0xbadbaddb;
const uint32_t kFreeListZapValue = 0xfeed1eaf;
#endif

const int kCodeZapValue = 0xbadc0de;

// Number of bits to represent the page size for paged spaces. The value of 20
// gives 1Mb bytes per page.
const int kPageSizeBits = 20;

// On Intel architecture, cache line size is 64 bytes.
// On ARM it may be less (32 bytes), but as far this constant is
// used for aligning data, it doesn't hurt to align on a greater value.
#define PROCESSOR_CACHE_LINE_SIZE 64

// Constants relevant to double precision floating point numbers.
// If looking only at the top 32 bits, the QNaN mask is bits 19 to 30.
const uint32_t kQuietNaNHighBitsMask = 0xfff << (51 - 32);


// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes

class AccessorInfo;
class Allocation;
class Arguments;
class Assembler;
class Code;
class CodeGenerator;
class CodeStub;
class Context;
class Debug;
class Debugger;
class DebugInfo;
class Descriptor;
class DescriptorArray;
class TransitionArray;
class ExternalReference;
class FixedArray;
class FunctionTemplateInfo;
class MemoryChunk;
class SeededNumberDictionary;
class UnseededNumberDictionary;
class NameDictionary;
template <typename T> class MaybeHandle;
template <typename T> class Handle;
class Heap;
class HeapObject;
class IC;
class InterceptorInfo;
class Isolate;
class JSReceiver;
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
class Object;
class OldSpace;
class Foreign;
class Scope;
class ScopeInfo;
class Script;
class Smi;
template <typename Config, class Allocator = FreeStoreAllocationPolicy>
    class SplayTree;
class String;
class Name;
class Struct;
class Variable;
class RelocInfo;
class Deserializer;
class MessageLocation;
class VirtualMemory;
class Mutex;
class RecursiveMutex;

typedef bool (*WeakSlotCallback)(Object** pointer);

typedef bool (*WeakSlotCallbackWithHeap)(Heap* heap, Object** pointer);

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
  PROPERTY_CELL_SPACE,  // Only and all global property cell objects.
  LO_SPACE,             // Promoted large objects.
  INVALID_SPACE,        // Only used in AllocationResult to signal success.

  FIRST_SPACE = NEW_SPACE,
  LAST_SPACE = LO_SPACE,
  FIRST_PAGED_SPACE = OLD_POINTER_SPACE,
  LAST_PAGED_SPACE = PROPERTY_CELL_SPACE
};
const int kSpaceTagSize = 3;
const int kSpaceTagMask = (1 << kSpaceTagSize) - 1;


// A flag that indicates whether objects should be pretenured when
// allocated (allocated directly into the old generation) or not
// (allocated in the young generation if the object size and type
// allows).
enum PretenureFlag { NOT_TENURED, TENURED };

enum MinimumCapacity {
  USE_DEFAULT_MINIMUM_CAPACITY,
  USE_CUSTOM_MINIMUM_CAPACITY
};

enum GarbageCollector { SCAVENGER, MARK_COMPACTOR };

enum Executability { NOT_EXECUTABLE, EXECUTABLE };

enum VisitMode {
  VISIT_ALL,
  VISIT_ALL_IN_SCAVENGE,
  VISIT_ALL_IN_SWEEP_NEWSPACE,
  VISIT_ONLY_STRONG
};

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
  POLYMORPHIC,
  // Many receiver types have been seen.
  MEGAMORPHIC,
  // A generic handler is installed and no extra typefeedback is recorded.
  GENERIC,
  // Special state for debug break or step in prepare stubs.
  DEBUG_STUB
};


enum CallFunctionFlags {
  NO_CALL_FUNCTION_FLAGS,
  CALL_AS_METHOD,
  // Always wrap the receiver and call to the JSFunction. Only use this flag
  // both the receiver type and the target method are statically known.
  WRAP_AND_CALL
};


enum CallConstructorFlags {
  NO_CALL_CONSTRUCTOR_FLAGS,
  // The call target is cached in the instruction stream.
  RECORD_CONSTRUCTOR_TARGET
};


enum InlineCacheHolderFlag {
  OWN_MAP,  // For fast properties objects.
  PROTOTYPE_MAP  // For slow properties objects (except GlobalObjects).
};


// The Store Buffer (GC).
typedef enum {
  kStoreBufferFullEvent,
  kStoreBufferStartScanningPagesEvent,
  kStoreBufferScanningPageEvent
} StoreBufferEvent;


typedef void (*StoreBufferCallback)(Heap* heap,
                                    MemoryChunk* page,
                                    StoreBufferEvent event);


// Union used for fast testing of specific double values.
union DoubleRepresentation {
  double  value;
  int64_t bits;
  DoubleRepresentation(double x) { value = x; }
  bool operator==(const DoubleRepresentation& other) const {
    return bits == other.bits;
  }
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
  Object* (*getter)(Isolate* isolate, Object* object, void* data);
  Object* (*setter)(
      Isolate* isolate, JSObject* object, Object* value, void* data);
  void* data;
};


// Logging and profiling.  A StateTag represents a possible state of
// the VM. The logger maintains a stack of these. Creating a VMState
// object enters a state by pushing on the stack, and destroying a
// VMState object leaves a state by popping the current state from the
// stack.

enum StateTag {
  JS,
  GC,
  COMPILER,
  OTHER,
  EXTERNAL,
  IDLE
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
    Logger::NewEventStatic(name, result, size); \
    return result; \
  } \
  void operator delete(void* object) { \
    Logger::DeleteEventStatic(name, object); \
    ::operator delete(object); \
  }
#else
#define TRACK_MEMORY(name)
#endif


// Feature flags bit positions. They are mostly based on the CPUID spec.
// On X86/X64, values below 32 are bits in EDX, values above 32 are bits in ECX.
enum CpuFeature { SSE4_1 = 32 + 19,  // x86
                  SSE3 = 32 + 0,     // x86
                  SSE2 = 26,   // x86
                  CMOV = 15,   // x86
                  VFP3 = 1,    // ARM
                  ARMv7 = 2,   // ARM
                  SUDIV = 3,   // ARM
                  UNALIGNED_ACCESSES = 4,  // ARM
                  MOVW_MOVT_IMMEDIATE_LOADS = 5,  // ARM
                  VFP32DREGS = 6,  // ARM
                  NEON = 7,    // ARM
                  SAHF = 0,    // x86
                  FPU = 1};    // MIPS


// Used to specify if a macro instruction must perform a smi check on tagged
// values.
enum SmiCheckType {
  DONT_DO_SMI_CHECK,
  DO_SMI_CHECK
};


enum ScopeType {
  EVAL_SCOPE,      // The top-level scope for an eval source.
  FUNCTION_SCOPE,  // The top-level scope for a function.
  MODULE_SCOPE,    // The scope introduced by a module literal
  GLOBAL_SCOPE,    // The top-level scope for a program or a top-level eval.
  CATCH_SCOPE,     // The scope introduced by catch.
  BLOCK_SCOPE,     // The scope introduced by a new block.
  WITH_SCOPE       // The scope introduced by with.
};


const uint32_t kHoleNanUpper32 = 0x7FFFFFFF;
const uint32_t kHoleNanLower32 = 0xFFFFFFFF;
const uint32_t kNaNOrInfinityLowerBoundUpper32 = 0x7FF00000;

const uint64_t kHoleNanInt64 =
    (static_cast<uint64_t>(kHoleNanUpper32) << 32) | kHoleNanLower32;
const uint64_t kLastNonNaNInt64 =
    (static_cast<uint64_t>(kNaNOrInfinityLowerBoundUpper32) << 32);


// The order of this enum has to be kept in sync with the predicates below.
enum VariableMode {
  // User declared variables:
  VAR,             // declared via 'var', and 'function' declarations

  CONST_LEGACY,    // declared via legacy 'const' declarations

  LET,             // declared via 'let' declarations (first lexical)

  CONST,           // declared via 'const' declarations

  MODULE,          // declared via 'module' declaration (last lexical)

  // Variables introduced by the compiler:
  INTERNAL,        // like VAR, but not user-visible (may or may not
                   // be in a context)

  TEMPORARY,       // temporary variables (not user-visible), stack-allocated
                   // unless the scope as a whole has forced context allocation

  DYNAMIC,         // always require dynamic lookup (we don't know
                   // the declaration)

  DYNAMIC_GLOBAL,  // requires dynamic lookup, but we know that the
                   // variable is global unless it has been shadowed
                   // by an eval-introduced variable

  DYNAMIC_LOCAL    // requires dynamic lookup, but we know that the
                   // variable is local and where it is unless it
                   // has been shadowed by an eval-introduced
                   // variable
};


inline bool IsDynamicVariableMode(VariableMode mode) {
  return mode >= DYNAMIC && mode <= DYNAMIC_LOCAL;
}


inline bool IsDeclaredVariableMode(VariableMode mode) {
  return mode >= VAR && mode <= MODULE;
}


inline bool IsLexicalVariableMode(VariableMode mode) {
  return mode >= LET && mode <= MODULE;
}


inline bool IsImmutableVariableMode(VariableMode mode) {
  return (mode >= CONST && mode <= MODULE) || mode == CONST_LEGACY;
}


// ES6 Draft Rev3 10.2 specifies declarative environment records with mutable
// and immutable bindings that can be in two states: initialized and
// uninitialized. In ES5 only immutable bindings have these two states. When
// accessing a binding, it needs to be checked for initialization. However in
// the following cases the binding is initialized immediately after creation
// so the initialization check can always be skipped:
// 1. Var declared local variables.
//      var foo;
// 2. A local variable introduced by a function declaration.
//      function foo() {}
// 3. Parameters
//      function x(foo) {}
// 4. Catch bound variables.
//      try {} catch (foo) {}
// 6. Function variables of named function expressions.
//      var x = function foo() {}
// 7. Implicit binding of 'this'.
// 8. Implicit binding of 'arguments' in functions.
//
// ES5 specified object environment records which are introduced by ES elements
// such as Program and WithStatement that associate identifier bindings with the
// properties of some object. In the specification only mutable bindings exist
// (which may be non-writable) and have no distinct initialization step. However
// V8 allows const declarations in global code with distinct creation and
// initialization steps which are represented by non-writable properties in the
// global object. As a result also these bindings need to be checked for
// initialization.
//
// The following enum specifies a flag that indicates if the binding needs a
// distinct initialization step (kNeedsInitialization) or if the binding is
// immediately initialized upon creation (kCreatedInitialized).
enum InitializationFlag {
  kNeedsInitialization,
  kCreatedInitialized
};


enum ClearExceptionFlag {
  KEEP_EXCEPTION,
  CLEAR_EXCEPTION
};


enum MinusZeroMode {
  TREAT_MINUS_ZERO_AS_ZERO,
  FAIL_ON_MINUS_ZERO
};

} }  // namespace v8::internal

namespace i = v8::internal;

#endif  // V8_V8GLOBALS_H_
