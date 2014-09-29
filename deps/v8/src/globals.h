// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBALS_H_
#define V8_GLOBALS_H_

#include "include/v8stdint.h"

#include "src/base/build_config.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

// Unfortunately, the INFINITY macro cannot be used with the '-pedantic'
// warning flag and certain versions of GCC due to a bug:
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11931
// For now, we use the more involved template-based version from <limits>, but
// only when compiling with GCC versions affected by the bug (2.96.x - 4.0.x)
#if V8_CC_GNU && V8_GNUC_PREREQ(2, 96, 0) && !V8_GNUC_PREREQ(4, 1, 0)
# include <limits>  // NOLINT
# define V8_INFINITY std::numeric_limits<double>::infinity()
#elif V8_LIBC_MSVCRT
# define V8_INFINITY HUGE_VAL
#else
# define V8_INFINITY INFINITY
#endif

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM || \
    V8_TARGET_ARCH_ARM64
#define V8_TURBOFAN_BACKEND 1
#else
#define V8_TURBOFAN_BACKEND 0
#endif
#if V8_TURBOFAN_BACKEND && !(V8_OS_WIN && V8_TARGET_ARCH_X64)
#define V8_TURBOFAN_TARGET 1
#else
#define V8_TURBOFAN_TARGET 0
#endif

namespace v8 {

namespace base {
class Mutex;
class RecursiveMutex;
class VirtualMemory;
}

namespace internal {

// Determine whether we are running in a simulated environment.
// Setting USE_SIMULATOR explicitly from the build script will force
// the use of a simulated environment.
#if !defined(USE_SIMULATOR)
#if (V8_TARGET_ARCH_ARM64 && !V8_HOST_ARCH_ARM64)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_ARM && !V8_HOST_ARCH_ARM)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_MIPS && !V8_HOST_ARCH_MIPS)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_MIPS64 && !V8_HOST_ARCH_MIPS64)
#define USE_SIMULATOR 1
#endif
#endif

// Determine whether the architecture uses an out-of-line constant pool.
#define V8_OOL_CONSTANT_POOL 0

// Support for alternative bool type. This is only enabled if the code is
// compiled with USE_MYBOOL defined. This catches some nasty type bugs.
// For instance, 'bool b = "false";' results in b == true! This is a hidden
// source of bugs.
// However, redefining the bool type does have some negative impact on some
// platforms. It gives rise to compiler warnings (i.e. with
// MSVC) in the API header files when mixing code that uses the standard
// bool with code that uses the redefined version.
// This does not actually belong in the platform code, but needs to be
// defined here because the platform code uses bool, and platform.h is
// include very early in the main include file.

#ifdef USE_MYBOOL
typedef unsigned int __my_bool__;
#define bool __my_bool__  // use 'indirection' to avoid name clashes
#endif

typedef uint8_t byte;
typedef byte* Address;

// -----------------------------------------------------------------------------
// Constants

const int KB = 1024;
const int MB = KB * KB;
const int GB = KB * KB * KB;
const int kMaxInt = 0x7FFFFFFF;
const int kMinInt = -kMaxInt - 1;
const int kMaxInt8 = (1 << 7) - 1;
const int kMinInt8 = -(1 << 7);
const int kMaxUInt8 = (1 << 8) - 1;
const int kMinUInt8 = 0;
const int kMaxInt16 = (1 << 15) - 1;
const int kMinInt16 = -(1 << 15);
const int kMaxUInt16 = (1 << 16) - 1;
const int kMinUInt16 = 0;

const uint32_t kMaxUInt32 = 0xFFFFFFFFu;

const int kCharSize      = sizeof(char);      // NOLINT
const int kShortSize     = sizeof(short);     // NOLINT
const int kIntSize       = sizeof(int);       // NOLINT
const int kInt32Size     = sizeof(int32_t);   // NOLINT
const int kInt64Size     = sizeof(int64_t);   // NOLINT
const int kDoubleSize    = sizeof(double);    // NOLINT
const int kIntptrSize    = sizeof(intptr_t);  // NOLINT
const int kPointerSize   = sizeof(void*);     // NOLINT
#if V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
const int kRegisterSize  = kPointerSize + kPointerSize;
#else
const int kRegisterSize  = kPointerSize;
#endif
const int kPCOnStackSize = kRegisterSize;
const int kFPOnStackSize = kRegisterSize;

const int kDoubleSizeLog2 = 3;

#if V8_HOST_ARCH_64_BIT
const int kPointerSizeLog2 = 3;
const intptr_t kIntptrSignBit = V8_INT64_C(0x8000000000000000);
const uintptr_t kUintptrAllBitsSet = V8_UINT64_C(0xFFFFFFFFFFFFFFFF);
const bool kRequiresCodeRange = true;
const size_t kMaximalCodeRangeSize = 512 * MB;
#else
const int kPointerSizeLog2 = 2;
const intptr_t kIntptrSignBit = 0x80000000;
const uintptr_t kUintptrAllBitsSet = 0xFFFFFFFFu;
#if V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
// x32 port also requires code range.
const bool kRequiresCodeRange = true;
const size_t kMaximalCodeRangeSize = 256 * MB;
#else
const bool kRequiresCodeRange = false;
const size_t kMaximalCodeRangeSize = 0 * MB;
#endif
#endif

STATIC_ASSERT(kPointerSize == (1 << kPointerSizeLog2));

const int kBitsPerByte = 8;
const int kBitsPerByteLog2 = 3;
const int kBitsPerPointer = kPointerSize * kBitsPerByte;
const int kBitsPerInt = kIntSize * kBitsPerByte;

// IEEE 754 single precision floating point number bit layout.
const uint32_t kBinary32SignMask = 0x80000000u;
const uint32_t kBinary32ExponentMask = 0x7f800000u;
const uint32_t kBinary32MantissaMask = 0x007fffffu;
const int kBinary32ExponentBias = 127;
const int kBinary32MaxExponent  = 0xFE;
const int kBinary32MinExponent  = 0x01;
const int kBinary32MantissaBits = 23;
const int kBinary32ExponentShift = 23;

// Quiet NaNs have bits 51 to 62 set, possibly the sign bit, and no
// other bits set.
const uint64_t kQuietNaNMask = static_cast<uint64_t>(0xfff) << 51;

// Latin1/UTF-16 constants
// Code-point values in Unicode 4.0 are 21 bits wide.
// Code units in UTF-16 are 16 bits wide.
typedef uint16_t uc16;
typedef int32_t uc32;
const int kOneByteSize    = kCharSize;
const int kUC16Size     = sizeof(uc16);      // NOLINT


// Round up n to be a multiple of sz, where sz is a power of 2.
#define ROUND_UP(n, sz) (((n) + ((sz) - 1)) & ~((sz) - 1))


// FUNCTION_ADDR(f) gets the address of a C function f.
#define FUNCTION_ADDR(f)                                        \
  (reinterpret_cast<v8::internal::Address>(reinterpret_cast<intptr_t>(f)))


// FUNCTION_CAST<F>(addr) casts an address into a function
// of type F. Used to invoke generated code from within C.
template <typename F>
F FUNCTION_CAST(Address addr) {
  return reinterpret_cast<F>(reinterpret_cast<intptr_t>(addr));
}


// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes
// (sorted alphabetically)

class FreeStoreAllocationPolicy;
template <typename T, class P = FreeStoreAllocationPolicy> class List;

// -----------------------------------------------------------------------------
// Declarations for use in both the preparser and the rest of V8.

// The Strict Mode (ECMA-262 5th edition, 4.2.2).

enum StrictMode { SLOPPY, STRICT };


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

// The owner field of a page is tagged with the page header tag. We need that
// to find out if a slot is part of a large object. If we mask out the lower
// 0xfffff bits (1M pages), go to the owner offset, and see that this field
// is tagged with the page header tag, we can just look up the owner.
// Otherwise, we know that we are somewhere (not within the first 1M) in a
// large object.
const int kPageHeaderTag = 3;
const int kPageHeaderTagSize = 2;
const intptr_t kPageHeaderTagMask = (1 << kPageHeaderTagSize) - 1;


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
  // Check failed due to prototype (or map deprecation).
  PROTOTYPE_FAILURE,
  // Multiple receiver types have been seen.
  POLYMORPHIC,
  // Many receiver types have been seen.
  MEGAMORPHIC,
  // A generic handler is installed and no extra typefeedback is recorded.
  GENERIC,
  // Special state for debug break or step in prepare stubs.
  DEBUG_STUB,
  // Type-vector-based ICs have a default state, with the full calculation
  // of IC state only determined by a look at the IC and the typevector
  // together.
  DEFAULT
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


enum CacheHolderFlag {
  kCacheOnPrototype,
  kCacheOnPrototypeReceiverIsDictionary,
  kCacheOnPrototypeReceiverIsPrimitive,
  kCacheOnReceiver
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


// CPU feature flags.
enum CpuFeature {
    // x86
    SSE4_1,
    SSE3,
    SAHF,
    // ARM
    VFP3,
    ARMv7,
    SUDIV,
    MLS,
    UNALIGNED_ACCESSES,
    MOVW_MOVT_IMMEDIATE_LOADS,
    VFP32DREGS,
    NEON,
    // MIPS
    FPU,
    // ARM64
    ALWAYS_ALIGN_CSP,
    NUMBER_OF_CPU_FEATURES
};


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


enum MaybeAssignedFlag { kNotAssigned, kMaybeAssigned };


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

#endif  // V8_GLOBALS_H_
