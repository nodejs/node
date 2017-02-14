// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBALS_H_
#define V8_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>

#include "src/base/build_config.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

#ifdef V8_OS_WIN

// Setup for Windows shared library export.
#ifdef BUILDING_V8_SHARED
#define V8_EXPORT_PRIVATE __declspec(dllexport)
#elif USING_V8_SHARED
#define V8_EXPORT_PRIVATE __declspec(dllimport)
#else
#define V8_EXPORT_PRIVATE
#endif  // BUILDING_V8_SHARED

#else  // V8_OS_WIN

// Setup for Linux shared library export.
#if V8_HAS_ATTRIBUTE_VISIBILITY
#ifdef BUILDING_V8_SHARED
#define V8_EXPORT_PRIVATE __attribute__((visibility("default")))
#else
#define V8_EXPORT_PRIVATE
#endif
#else
#define V8_EXPORT_PRIVATE
#endif

#endif  // V8_OS_WIN

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
#elif V8_OS_AIX
#define V8_INFINITY (__builtin_inff())
#else
# define V8_INFINITY INFINITY
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
#if (V8_TARGET_ARCH_PPC && !V8_HOST_ARCH_PPC)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_MIPS && !V8_HOST_ARCH_MIPS)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_MIPS64 && !V8_HOST_ARCH_MIPS64)
#define USE_SIMULATOR 1
#endif
#if (V8_TARGET_ARCH_S390 && !V8_HOST_ARCH_S390)
#define USE_SIMULATOR 1
#endif
#endif

// Determine whether the architecture uses an embedded constant pool
// (contiguous constant pool embedded in code object).
#if V8_TARGET_ARCH_PPC
#define V8_EMBEDDED_CONSTANT_POOL 1
#else
#define V8_EMBEDDED_CONSTANT_POOL 0
#endif

#ifdef V8_TARGET_ARCH_ARM
// Set stack limit lower for ARM than for other architectures because
// stack allocating MacroAssembler takes 120K bytes.
// See issue crbug.com/405338
#define V8_DEFAULT_STACK_SIZE_KB 864
#else
// Slightly less than 1MB, since Windows' default stack size for
// the main execution thread is 1MB for both 32 and 64-bit.
#define V8_DEFAULT_STACK_SIZE_KB 984
#endif


// Determine whether double field unboxing feature is enabled.
#if V8_TARGET_ARCH_64_BIT
#define V8_DOUBLE_FIELDS_UNBOXING 1
#else
#define V8_DOUBLE_FIELDS_UNBOXING 0
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
const int kMinUInt32 = 0;

const int kCharSize      = sizeof(char);      // NOLINT
const int kShortSize     = sizeof(short);     // NOLINT
const int kIntSize       = sizeof(int);       // NOLINT
const int kInt32Size     = sizeof(int32_t);   // NOLINT
const int kInt64Size     = sizeof(int64_t);   // NOLINT
const int kFloatSize     = sizeof(float);     // NOLINT
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

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
const int kElidedFrameSlots = kPCOnStackSize / kPointerSize;
#else
const int kElidedFrameSlots = 0;
#endif

const int kDoubleSizeLog2 = 3;

#if V8_HOST_ARCH_64_BIT
const int kPointerSizeLog2 = 3;
const intptr_t kIntptrSignBit = V8_INT64_C(0x8000000000000000);
const uintptr_t kUintptrAllBitsSet = V8_UINT64_C(0xFFFFFFFFFFFFFFFF);
const bool kRequiresCodeRange = true;
#if V8_TARGET_ARCH_MIPS64
// To use pseudo-relative jumps such as j/jal instructions which have 28-bit
// encoded immediate, the addresses have to be in range of 256MB aligned
// region. Used only for large object space.
const size_t kMaximalCodeRangeSize = 256 * MB;
const size_t kCodeRangeAreaAlignment = 256 * MB;
#elif V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
const size_t kMaximalCodeRangeSize = 512 * MB;
const size_t kCodeRangeAreaAlignment = 64 * KB;  // OS page on PPC Linux
#else
const size_t kMaximalCodeRangeSize = 512 * MB;
const size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#endif
#if V8_OS_WIN
const size_t kMinimumCodeRangeSize = 4 * MB;
const size_t kReservedCodeRangePages = 1;
#else
const size_t kMinimumCodeRangeSize = 3 * MB;
const size_t kReservedCodeRangePages = 0;
#endif
#else
const int kPointerSizeLog2 = 2;
const intptr_t kIntptrSignBit = 0x80000000;
const uintptr_t kUintptrAllBitsSet = 0xFFFFFFFFu;
#if V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
// x32 port also requires code range.
const bool kRequiresCodeRange = true;
const size_t kMaximalCodeRangeSize = 256 * MB;
const size_t kMinimumCodeRangeSize = 3 * MB;
const size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#elif V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
const bool kRequiresCodeRange = false;
const size_t kMaximalCodeRangeSize = 0 * MB;
const size_t kMinimumCodeRangeSize = 0 * MB;
const size_t kCodeRangeAreaAlignment = 64 * KB;  // OS page on PPC Linux
#else
const bool kRequiresCodeRange = false;
const size_t kMaximalCodeRangeSize = 0 * MB;
const size_t kMinimumCodeRangeSize = 0 * MB;
const size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#endif
const size_t kReservedCodeRangePages = 0;
#endif

// Trigger an incremental GCs once the external memory reaches this limit.
const int kExternalAllocationSoftLimit = 64 * MB;

// Maximum object size that gets allocated into regular pages. Objects larger
// than that size are allocated in large object space and are never moved in
// memory. This also applies to new space allocation, since objects are never
// migrated from new space to large object space. Takes double alignment into
// account.
//
// Current value: Page::kAllocatableMemory (on 32-bit arch) - 512 (slack).
const int kMaxRegularHeapObjectSize = 507136;

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

// 128 bit SIMD value size.
const int kSimd128Size = 16;

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


// Determine whether the architecture uses function descriptors
// which provide a level of indirection between the function pointer
// and the function entrypoint.
#if V8_HOST_ARCH_PPC && \
    (V8_OS_AIX || (V8_TARGET_ARCH_PPC64 && V8_TARGET_BIG_ENDIAN))
#define USES_FUNCTION_DESCRIPTORS 1
#define FUNCTION_ENTRYPOINT_ADDRESS(f)       \
  (reinterpret_cast<v8::internal::Address*>( \
      &(reinterpret_cast<intptr_t*>(f)[0])))
#else
#define USES_FUNCTION_DESCRIPTORS 0
#endif


// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes
// (sorted alphabetically)

class FreeStoreAllocationPolicy;
template <typename T, class P = FreeStoreAllocationPolicy> class List;

// -----------------------------------------------------------------------------
// Declarations for use in both the preparser and the rest of V8.

// The Strict Mode (ECMA-262 5th edition, 4.2.2).

enum LanguageMode : uint32_t { SLOPPY, STRICT, LANGUAGE_END };

inline std::ostream& operator<<(std::ostream& os, const LanguageMode& mode) {
  switch (mode) {
    case SLOPPY: return os << "sloppy";
    case STRICT: return os << "strict";
    default: UNREACHABLE();
  }
  return os;
}


inline bool is_sloppy(LanguageMode language_mode) {
  return language_mode == SLOPPY;
}


inline bool is_strict(LanguageMode language_mode) {
  return language_mode != SLOPPY;
}


inline bool is_valid_language_mode(int language_mode) {
  return language_mode == SLOPPY || language_mode == STRICT;
}


inline LanguageMode construct_language_mode(bool strict_bit) {
  return static_cast<LanguageMode>(strict_bit);
}

// This constant is used as an undefined value when passing source positions.
const int kNoSourcePosition = -1;

// This constant is used to indicate missing deoptimization information.
const int kNoDeoptimizationId = -1;

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

// Desired alignment for 128 bit SIMD values.
const intptr_t kSimd128Alignment = 16;
const intptr_t kSimd128AlignmentMask = kSimd128Alignment - 1;

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
const uint32_t kPhantomReferenceZap = 0xca11bac;

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
class GlobalDictionary;
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
class MacroAssembler;
class Map;
class MapSpace;
class MarkCompactCollector;
class NewSpace;
class Object;
class OldSpace;
class ParameterCount;
class Foreign;
class Scope;
class DeclarationScope;
class ModuleScope;
class ScopeInfo;
class Script;
class Smi;
template <typename Config, class Allocator = FreeStoreAllocationPolicy>
class SplayTree;
class String;
class Symbol;
class Name;
class Struct;
class TypeFeedbackVector;
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
// Keep this enum in sync with the ObjectSpace enum in v8.h
enum AllocationSpace {
  NEW_SPACE,   // Semispaces collected with copying collector.
  OLD_SPACE,   // May contain pointers to new space.
  CODE_SPACE,  // No pointers to new space, marked executable.
  MAP_SPACE,   // Only and all map objects.
  LO_SPACE,    // Promoted large objects.

  FIRST_SPACE = NEW_SPACE,
  LAST_SPACE = LO_SPACE,
  FIRST_PAGED_SPACE = OLD_SPACE,
  LAST_PAGED_SPACE = MAP_SPACE
};
const int kSpaceTagSize = 3;
const int kSpaceTagMask = (1 << kSpaceTagSize) - 1;

enum AllocationAlignment {
  kWordAligned,
  kDoubleAligned,
  kDoubleUnaligned,
  kSimd128Unaligned
};

// Possible outcomes for decisions.
enum class Decision : uint8_t { kUnknown, kTrue, kFalse };

inline size_t hash_value(Decision decision) {
  return static_cast<uint8_t>(decision);
}

inline std::ostream& operator<<(std::ostream& os, Decision decision) {
  switch (decision) {
    case Decision::kUnknown:
      return os << "Unknown";
    case Decision::kTrue:
      return os << "True";
    case Decision::kFalse:
      return os << "False";
  }
  UNREACHABLE();
  return os;
}

// Supported write barrier modes.
enum WriteBarrierKind : uint8_t {
  kNoWriteBarrier,
  kMapWriteBarrier,
  kPointerWriteBarrier,
  kFullWriteBarrier
};

inline size_t hash_value(WriteBarrierKind kind) {
  return static_cast<uint8_t>(kind);
}

inline std::ostream& operator<<(std::ostream& os, WriteBarrierKind kind) {
  switch (kind) {
    case kNoWriteBarrier:
      return os << "NoWriteBarrier";
    case kMapWriteBarrier:
      return os << "MapWriteBarrier";
    case kPointerWriteBarrier:
      return os << "PointerWriteBarrier";
    case kFullWriteBarrier:
      return os << "FullWriteBarrier";
  }
  UNREACHABLE();
  return os;
}

// A flag that indicates whether objects should be pretenured when
// allocated (allocated directly into the old generation) or not
// (allocated in the young generation if the object size and type
// allows).
enum PretenureFlag { NOT_TENURED, TENURED };

inline std::ostream& operator<<(std::ostream& os, const PretenureFlag& flag) {
  switch (flag) {
    case NOT_TENURED:
      return os << "NotTenured";
    case TENURED:
      return os << "Tenured";
  }
  UNREACHABLE();
  return os;
}

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
  VISIT_ONLY_STRONG,
  VISIT_ONLY_STRONG_FOR_SERIALIZATION,
  VISIT_ONLY_STRONG_ROOT_LIST,
};

// Flag indicating whether code is built into the VM (one of the natives files).
enum NativesFlag { NOT_NATIVES_CODE, EXTENSION_CODE, NATIVES_CODE };

// JavaScript defines two kinds of 'nil'.
enum NilValue { kNullValue, kUndefinedValue };

// ParseRestriction is used to restrict the set of valid statements in a
// unit of compilation.  Restriction violations cause a syntax error.
enum ParseRestriction {
  NO_PARSE_RESTRICTION,         // All expressions are allowed.
  ONLY_SINGLE_FUNCTION_LITERAL  // Only a single FunctionLiteral expression.
};

// A CodeDesc describes a buffer holding instructions and relocation
// information. The instructions start at the beginning of the buffer
// and grow forward, the relocation information starts at the end of
// the buffer and grows backward.  A constant pool may exist at the
// end of the instructions.
//
//  |<--------------- buffer_size ----------------------------------->|
//  |<------------- instr_size ---------->|        |<-- reloc_size -->|
//  |               |<- const_pool_size ->|                           |
//  +=====================================+========+==================+
//  |  instructions |        data         |  free  |    reloc info    |
//  +=====================================+========+==================+
//  ^
//  |
//  buffer

struct CodeDesc {
  byte* buffer;
  int buffer_size;
  int instr_size;
  int reloc_size;
  int constant_pool_size;
  byte* unwinding_info;
  int unwinding_info_size;
  Assembler* origin;
};


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
  RECOMPUTE_HANDLER,
  // Multiple receiver types have been seen.
  POLYMORPHIC,
  // Many receiver types have been seen.
  MEGAMORPHIC,
  // A generic handler is installed and no extra typefeedback is recorded.
  GENERIC,
};

enum CacheHolderFlag {
  kCacheOnPrototype,
  kCacheOnPrototypeReceiverIsDictionary,
  kCacheOnPrototypeReceiverIsPrimitive,
  kCacheOnReceiver
};

enum WhereToStart { kStartAtReceiver, kStartAtPrototype };

// The Store Buffer (GC).
typedef enum {
  kStoreBufferFullEvent,
  kStoreBufferStartScanningPagesEvent,
  kStoreBufferScanningPageEvent
} StoreBufferEvent;


typedef void (*StoreBufferCallback)(Heap* heap,
                                    MemoryChunk* page,
                                    StoreBufferEvent event);

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

#if V8_TARGET_LITTLE_ENDIAN
typedef IeeeDoubleLittleEndianArchType IeeeDoubleArchType;
const int kIeeeDoubleMantissaWordOffset = 0;
const int kIeeeDoubleExponentWordOffset = 4;
#else
typedef IeeeDoubleBigEndianArchType IeeeDoubleArchType;
const int kIeeeDoubleMantissaWordOffset = 4;
const int kIeeeDoubleExponentWordOffset = 0;
#endif

// AccessorCallback
struct AccessorDescriptor {
  Object* (*getter)(Isolate* isolate, Object* object, void* data);
  Object* (*setter)(
      Isolate* isolate, JSObject* object, Object* value, void* data);
  void* data;
};


// -----------------------------------------------------------------------------
// Macros

// Testers for test.

#define HAS_SMI_TAG(value) \
  ((reinterpret_cast<intptr_t>(value) & kSmiTagMask) == kSmiTag)

// OBJECT_POINTER_ALIGN returns the value aligned as a HeapObject pointer
#define OBJECT_POINTER_ALIGN(value)                             \
  (((value) + kObjectAlignmentMask) & ~kObjectAlignmentMask)

// POINTER_SIZE_ALIGN returns the value aligned as a pointer.
#define POINTER_SIZE_ALIGN(value)                               \
  (((value) + kPointerAlignmentMask) & ~kPointerAlignmentMask)

// CODE_POINTER_ALIGN returns the value aligned as a generated code segment.
#define CODE_POINTER_ALIGN(value)                               \
  (((value) + kCodeAlignmentMask) & ~kCodeAlignmentMask)

// DOUBLE_POINTER_ALIGN returns the value algined for double pointers.
#define DOUBLE_POINTER_ALIGN(value) \
  (((value) + kDoubleAlignmentMask) & ~kDoubleAlignmentMask)


// CPU feature flags.
enum CpuFeature {
  // x86
  SSE4_1,
  SSSE3,
  SSE3,
  SAHF,
  AVX,
  FMA3,
  BMI1,
  BMI2,
  LZCNT,
  POPCNT,
  ATOM,
  // ARM
  // - Standard configurations. The baseline is ARMv6+VFPv2.
  ARMv7,        // ARMv7-A + VFPv3-D32 + NEON
  ARMv7_SUDIV,  // ARMv7-A + VFPv4-D32 + NEON + SUDIV
  ARMv8,        // ARMv8-A (+ all of the above)
  // MIPS, MIPS64
  FPU,
  FP64FPU,
  MIPSr1,
  MIPSr2,
  MIPSr6,
  // ARM64
  ALWAYS_ALIGN_CSP,
  // PPC
  FPR_GPR_MOV,
  LWSYNC,
  ISELECT,
  // S390
  DISTINCT_OPS,
  GENERAL_INSTR_EXT,
  FLOATING_POINT_EXT,

  NUMBER_OF_CPU_FEATURES,

  // ARM feature aliases (based on the standard configurations above).
  VFPv3 = ARMv7,
  NEON = ARMv7,
  VFP32DREGS = ARMv7,
  SUDIV = ARMv7_SUDIV
};

// Defines hints about receiver values based on structural knowledge.
enum class ConvertReceiverMode : unsigned {
  kNullOrUndefined,     // Guaranteed to be null or undefined.
  kNotNullOrUndefined,  // Guaranteed to never be null or undefined.
  kAny                  // No specific knowledge about receiver.
};

inline size_t hash_value(ConvertReceiverMode mode) {
  return bit_cast<unsigned>(mode);
}

inline std::ostream& operator<<(std::ostream& os, ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return os << "NULL_OR_UNDEFINED";
    case ConvertReceiverMode::kNotNullOrUndefined:
      return os << "NOT_NULL_OR_UNDEFINED";
    case ConvertReceiverMode::kAny:
      return os << "ANY";
  }
  UNREACHABLE();
  return os;
}

// Defines whether tail call optimization is allowed.
enum class TailCallMode : unsigned { kAllow, kDisallow };

inline size_t hash_value(TailCallMode mode) { return bit_cast<unsigned>(mode); }

inline std::ostream& operator<<(std::ostream& os, TailCallMode mode) {
  switch (mode) {
    case TailCallMode::kAllow:
      return os << "ALLOW_TAIL_CALLS";
    case TailCallMode::kDisallow:
      return os << "DISALLOW_TAIL_CALLS";
  }
  UNREACHABLE();
  return os;
}

// Valid hints for the abstract operation OrdinaryToPrimitive,
// implemented according to ES6, section 7.1.1.
enum class OrdinaryToPrimitiveHint { kNumber, kString };

// Valid hints for the abstract operation ToPrimitive,
// implemented according to ES6, section 7.1.1.
enum class ToPrimitiveHint { kDefault, kNumber, kString };

// Defines specifics about arguments object or rest parameter creation.
enum class CreateArgumentsType : uint8_t {
  kMappedArguments,
  kUnmappedArguments,
  kRestParameter
};

inline size_t hash_value(CreateArgumentsType type) {
  return bit_cast<uint8_t>(type);
}

inline std::ostream& operator<<(std::ostream& os, CreateArgumentsType type) {
  switch (type) {
    case CreateArgumentsType::kMappedArguments:
      return os << "MAPPED_ARGUMENTS";
    case CreateArgumentsType::kUnmappedArguments:
      return os << "UNMAPPED_ARGUMENTS";
    case CreateArgumentsType::kRestParameter:
      return os << "REST_PARAMETER";
  }
  UNREACHABLE();
  return os;
}

// Used to specify if a macro instruction must perform a smi check on tagged
// values.
enum SmiCheckType {
  DONT_DO_SMI_CHECK,
  DO_SMI_CHECK
};

enum ScopeType : uint8_t {
  EVAL_SCOPE,      // The top-level scope for an eval source.
  FUNCTION_SCOPE,  // The top-level scope for a function.
  MODULE_SCOPE,    // The scope introduced by a module literal
  SCRIPT_SCOPE,    // The top-level scope for a script or a top-level eval.
  CATCH_SCOPE,     // The scope introduced by catch.
  BLOCK_SCOPE,     // The scope introduced by a new block.
  WITH_SCOPE       // The scope introduced by with.
};

// The mips architecture prior to revision 5 has inverted encoding for sNaN.
// The x87 FPU convert the sNaN to qNaN automatically when loading sNaN from
// memmory.
// Use mips sNaN which is a not used qNaN in x87 port as sNaN to workaround this
// issue
// for some test cases.
#if (V8_TARGET_ARCH_MIPS && !defined(_MIPS_ARCH_MIPS32R6) &&           \
     (!defined(USE_SIMULATOR) || !defined(_MIPS_TARGET_SIMULATOR))) || \
    (V8_TARGET_ARCH_MIPS64 && !defined(_MIPS_ARCH_MIPS64R6) &&         \
     (!defined(USE_SIMULATOR) || !defined(_MIPS_TARGET_SIMULATOR))) || \
    (V8_TARGET_ARCH_X87)
const uint32_t kHoleNanUpper32 = 0xFFFF7FFF;
const uint32_t kHoleNanLower32 = 0xFFFF7FFF;
#else
const uint32_t kHoleNanUpper32 = 0xFFF7FFFF;
const uint32_t kHoleNanLower32 = 0xFFF7FFFF;
#endif

const uint64_t kHoleNanInt64 =
    (static_cast<uint64_t>(kHoleNanUpper32) << 32) | kHoleNanLower32;


// ES6 section 20.1.2.6 Number.MAX_SAFE_INTEGER
const double kMaxSafeInteger = 9007199254740991.0;  // 2^53-1


// The order of this enum has to be kept in sync with the predicates below.
enum VariableMode : uint8_t {
  // User declared variables:
  VAR,  // declared via 'var', and 'function' declarations

  LET,  // declared via 'let' declarations (first lexical)

  CONST,  // declared via 'const' declarations (last lexical)

  // Variables introduced by the compiler:
  TEMPORARY,  // temporary variables (not user-visible), stack-allocated
              // unless the scope as a whole has forced context allocation

  DYNAMIC,  // always require dynamic lookup (we don't know
            // the declaration)

  DYNAMIC_GLOBAL,  // requires dynamic lookup, but we know that the
                   // variable is global unless it has been shadowed
                   // by an eval-introduced variable

  DYNAMIC_LOCAL,  // requires dynamic lookup, but we know that the
                  // variable is local and where it is unless it
                  // has been shadowed by an eval-introduced
                  // variable

  kLastVariableMode = DYNAMIC_LOCAL
};

// Printing support
#ifdef DEBUG
inline const char* VariableMode2String(VariableMode mode) {
  switch (mode) {
    case VAR:
      return "VAR";
    case LET:
      return "LET";
    case CONST:
      return "CONST";
    case DYNAMIC:
      return "DYNAMIC";
    case DYNAMIC_GLOBAL:
      return "DYNAMIC_GLOBAL";
    case DYNAMIC_LOCAL:
      return "DYNAMIC_LOCAL";
    case TEMPORARY:
      return "TEMPORARY";
  }
  UNREACHABLE();
  return NULL;
}
#endif

enum VariableKind : uint8_t {
  NORMAL_VARIABLE,
  FUNCTION_VARIABLE,
  THIS_VARIABLE,
  SLOPPY_FUNCTION_NAME_VARIABLE,
  kLastKind = SLOPPY_FUNCTION_NAME_VARIABLE
};

inline bool IsDynamicVariableMode(VariableMode mode) {
  return mode >= DYNAMIC && mode <= DYNAMIC_LOCAL;
}


inline bool IsDeclaredVariableMode(VariableMode mode) {
  STATIC_ASSERT(VAR == 0);  // Implies that mode >= VAR.
  return mode <= CONST;
}


inline bool IsLexicalVariableMode(VariableMode mode) {
  return mode >= LET && mode <= CONST;
}

enum VariableLocation : uint8_t {
  // Before and during variable allocation, a variable whose location is
  // not yet determined.  After allocation, a variable looked up as a
  // property on the global object (and possibly absent).  name() is the
  // variable name, index() is invalid.
  UNALLOCATED,

  // A slot in the parameter section on the stack.  index() is the
  // parameter index, counting left-to-right.  The receiver is index -1;
  // the first parameter is index 0.
  PARAMETER,

  // A slot in the local section on the stack.  index() is the variable
  // index in the stack frame, starting at 0.
  LOCAL,

  // An indexed slot in a heap context.  index() is the variable index in
  // the context object on the heap, starting at 0.  scope() is the
  // corresponding scope.
  CONTEXT,

  // A named slot in a heap context.  name() is the variable name in the
  // context object on the heap, with lookup starting at the current
  // context.  index() is invalid.
  LOOKUP,

  // A named slot in a module's export table.
  MODULE,

  kLastVariableLocation = MODULE
};

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
enum InitializationFlag : uint8_t { kNeedsInitialization, kCreatedInitialized };

enum MaybeAssignedFlag : uint8_t { kNotAssigned, kMaybeAssigned };

// Serialized in PreparseData, so numeric values should not be changed.
enum ParseErrorType { kSyntaxError = 0, kReferenceError = 1 };


enum MinusZeroMode {
  TREAT_MINUS_ZERO_AS_ZERO,
  FAIL_ON_MINUS_ZERO
};


enum Signedness { kSigned, kUnsigned };

enum FunctionKind : uint16_t {
  kNormalFunction = 0,
  kArrowFunction = 1 << 0,
  kGeneratorFunction = 1 << 1,
  kConciseMethod = 1 << 2,
  kConciseGeneratorMethod = kGeneratorFunction | kConciseMethod,
  kDefaultConstructor = 1 << 3,
  kSubclassConstructor = 1 << 4,
  kBaseConstructor = 1 << 5,
  kGetterFunction = 1 << 6,
  kSetterFunction = 1 << 7,
  kAsyncFunction = 1 << 8,
  kModule = 1 << 9,
  kAccessorFunction = kGetterFunction | kSetterFunction,
  kDefaultBaseConstructor = kDefaultConstructor | kBaseConstructor,
  kDefaultSubclassConstructor = kDefaultConstructor | kSubclassConstructor,
  kClassConstructor =
      kBaseConstructor | kSubclassConstructor | kDefaultConstructor,
  kAsyncArrowFunction = kArrowFunction | kAsyncFunction,
  kAsyncConciseMethod = kAsyncFunction | kConciseMethod
};

inline bool IsValidFunctionKind(FunctionKind kind) {
  return kind == FunctionKind::kNormalFunction ||
         kind == FunctionKind::kArrowFunction ||
         kind == FunctionKind::kGeneratorFunction ||
         kind == FunctionKind::kModule ||
         kind == FunctionKind::kConciseMethod ||
         kind == FunctionKind::kConciseGeneratorMethod ||
         kind == FunctionKind::kGetterFunction ||
         kind == FunctionKind::kSetterFunction ||
         kind == FunctionKind::kAccessorFunction ||
         kind == FunctionKind::kDefaultBaseConstructor ||
         kind == FunctionKind::kDefaultSubclassConstructor ||
         kind == FunctionKind::kBaseConstructor ||
         kind == FunctionKind::kSubclassConstructor ||
         kind == FunctionKind::kAsyncFunction ||
         kind == FunctionKind::kAsyncArrowFunction ||
         kind == FunctionKind::kAsyncConciseMethod;
}


inline bool IsArrowFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kArrowFunction;
}


inline bool IsGeneratorFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kGeneratorFunction;
}

inline bool IsModule(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kModule;
}

inline bool IsAsyncFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kAsyncFunction;
}

inline bool IsResumableFunction(FunctionKind kind) {
  return IsGeneratorFunction(kind) || IsAsyncFunction(kind) || IsModule(kind);
}

inline bool IsConciseMethod(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kConciseMethod;
}

inline bool IsGetterFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kGetterFunction;
}

inline bool IsSetterFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kSetterFunction;
}

inline bool IsAccessorFunction(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kAccessorFunction;
}


inline bool IsDefaultConstructor(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kDefaultConstructor;
}


inline bool IsBaseConstructor(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kBaseConstructor;
}


inline bool IsSubclassConstructor(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kSubclassConstructor;
}


inline bool IsClassConstructor(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  return kind & FunctionKind::kClassConstructor;
}


inline bool IsConstructable(FunctionKind kind, LanguageMode mode) {
  if (IsAccessorFunction(kind)) return false;
  if (IsConciseMethod(kind)) return false;
  if (IsArrowFunction(kind)) return false;
  if (IsGeneratorFunction(kind)) return false;
  if (IsAsyncFunction(kind)) return false;
  return true;
}

enum class CallableType : unsigned { kJSFunction, kAny };

inline size_t hash_value(CallableType type) { return bit_cast<unsigned>(type); }

inline std::ostream& operator<<(std::ostream& os, CallableType function_type) {
  switch (function_type) {
    case CallableType::kJSFunction:
      return os << "JSFunction";
    case CallableType::kAny:
      return os << "Any";
  }
  UNREACHABLE();
  return os;
}

inline uint32_t ObjectHash(Address address) {
  // All objects are at least pointer aligned, so we can remove the trailing
  // zeros.
  return static_cast<uint32_t>(bit_cast<uintptr_t>(address) >>
                               kPointerSizeLog2);
}

// Type feedback is encoded in such a way that, we can combine the feedback
// at different points by performing an 'OR' operation. Type feedback moves
// to a more generic type when we combine feedback.
// kSignedSmall -> kNumber  -> kAny
//                 kString  -> kAny
class BinaryOperationFeedback {
 public:
  enum {
    kNone = 0x0,
    kSignedSmall = 0x1,
    kNumber = 0x3,
    kString = 0x4,
    kAny = 0xF
  };
};

// TODO(epertoso): consider unifying this with BinaryOperationFeedback.
class CompareOperationFeedback {
 public:
  enum { kNone = 0x00, kSignedSmall = 0x01, kNumber = 0x3, kAny = 0x7 };
};

// Describes how exactly a frame has been dropped from stack.
enum LiveEditFrameDropMode {
  // No frame has been dropped.
  LIVE_EDIT_FRAMES_UNTOUCHED,
  // The top JS frame had been calling debug break slot stub. Patch the
  // address this stub jumps to in the end.
  LIVE_EDIT_FRAME_DROPPED_IN_DEBUG_SLOT_CALL,
  // The top JS frame had been calling some C++ function. The return address
  // gets patched automatically.
  LIVE_EDIT_FRAME_DROPPED_IN_DIRECT_CALL,
  LIVE_EDIT_FRAME_DROPPED_IN_RETURN_CALL,
  LIVE_EDIT_CURRENTLY_SET_MODE
};

enum class UnicodeEncoding : uint8_t {
  // Different unicode encodings in a |word32|:
  UTF16,  // hi 16bits -> trailing surrogate or 0, low 16bits -> lead surrogate
  UTF32,  // full UTF32 code unit / Unicode codepoint
};

inline size_t hash_value(UnicodeEncoding encoding) {
  return static_cast<uint8_t>(encoding);
}

inline std::ostream& operator<<(std::ostream& os, UnicodeEncoding encoding) {
  switch (encoding) {
    case UnicodeEncoding::UTF16:
      return os << "UTF16";
    case UnicodeEncoding::UTF32:
      return os << "UTF32";
  }
  UNREACHABLE();
  return os;
}

}  // namespace internal
}  // namespace v8

namespace i = v8::internal;

#endif  // V8_GLOBALS_H_
