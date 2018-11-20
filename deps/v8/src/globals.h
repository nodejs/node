// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GLOBALS_H_
#define V8_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>

#include "include/v8.h"
#include "src/base/build_config.h"
#include "src/base/flags.h"
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

#define V8_INFINITY std::numeric_limits<double>::infinity()

namespace v8 {

namespace base {
class Mutex;
class RecursiveMutex;
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

// Minimum stack size in KB required by compilers.
constexpr int kStackSpaceRequiredForCompilation = 40;

// Determine whether double field unboxing feature is enabled.
#if V8_TARGET_ARCH_64_BIT
#define V8_DOUBLE_FIELDS_UNBOXING 1
#else
#define V8_DOUBLE_FIELDS_UNBOXING 0
#endif

// Some types of tracing require the SFI to store a unique ID.
#if defined(V8_TRACE_MAPS) || defined(V8_TRACE_IGNITION)
#define V8_SFI_HAS_UNIQUE_ID 1
#endif

// Superclass for classes only using static method functions.
// The subclass of AllStatic cannot be instantiated at all.
class AllStatic {
#ifdef DEBUG
 public:
  AllStatic() = delete;
#endif
};

// DEPRECATED
// TODO(leszeks): Delete this during a quiet period
#define BASE_EMBEDDED

typedef uint8_t byte;
typedef uintptr_t Address;
static const Address kNullAddress = 0;

// -----------------------------------------------------------------------------
// Constants

constexpr int KB = 1024;
constexpr int MB = KB * KB;
constexpr int GB = KB * KB * KB;
constexpr int kMaxInt = 0x7FFFFFFF;
constexpr int kMinInt = -kMaxInt - 1;
constexpr int kMaxInt8 = (1 << 7) - 1;
constexpr int kMinInt8 = -(1 << 7);
constexpr int kMaxUInt8 = (1 << 8) - 1;
constexpr int kMinUInt8 = 0;
constexpr int kMaxInt16 = (1 << 15) - 1;
constexpr int kMinInt16 = -(1 << 15);
constexpr int kMaxUInt16 = (1 << 16) - 1;
constexpr int kMinUInt16 = 0;

constexpr uint32_t kMaxUInt32 = 0xFFFFFFFFu;
constexpr int kMinUInt32 = 0;

constexpr int kUInt8Size = sizeof(uint8_t);
constexpr int kCharSize = sizeof(char);
constexpr int kShortSize = sizeof(short);  // NOLINT
constexpr int kUInt16Size = sizeof(uint16_t);
constexpr int kIntSize = sizeof(int);
constexpr int kInt32Size = sizeof(int32_t);
constexpr int kInt64Size = sizeof(int64_t);
constexpr int kUInt32Size = sizeof(uint32_t);
constexpr int kSizetSize = sizeof(size_t);
constexpr int kFloatSize = sizeof(float);
constexpr int kDoubleSize = sizeof(double);
constexpr int kIntptrSize = sizeof(intptr_t);
constexpr int kUIntptrSize = sizeof(uintptr_t);
constexpr int kPointerSize = sizeof(void*);
constexpr int kPointerHexDigits = kPointerSize == 4 ? 8 : 12;
#if V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
constexpr int kRegisterSize = kPointerSize + kPointerSize;
#else
constexpr int kRegisterSize = kPointerSize;
#endif
constexpr int kPCOnStackSize = kRegisterSize;
constexpr int kFPOnStackSize = kRegisterSize;

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
constexpr int kElidedFrameSlots = kPCOnStackSize / kPointerSize;
#else
constexpr int kElidedFrameSlots = 0;
#endif

constexpr int kDoubleSizeLog2 = 3;
#if V8_TARGET_ARCH_ARM64
// ARM64 only supports direct calls within a 128 MB range.
constexpr size_t kMaxWasmCodeMemory = 128 * MB;
#else
constexpr size_t kMaxWasmCodeMemory = 512 * MB;
#endif

#if V8_HOST_ARCH_64_BIT
constexpr int kPointerSizeLog2 = 3;
constexpr intptr_t kIntptrSignBit =
    static_cast<intptr_t>(uintptr_t{0x8000000000000000});
constexpr uintptr_t kUintptrAllBitsSet = uintptr_t{0xFFFFFFFFFFFFFFFF};
constexpr bool kRequiresCodeRange = true;
#if V8_TARGET_ARCH_MIPS64
// To use pseudo-relative jumps such as j/jal instructions which have 28-bit
// encoded immediate, the addresses have to be in range of 256MB aligned
// region. Used only for large object space.
constexpr size_t kMaximalCodeRangeSize = 256 * MB;
constexpr size_t kCodeRangeAreaAlignment = 256 * MB;
#elif V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
constexpr size_t kMaximalCodeRangeSize = 512 * MB;
constexpr size_t kCodeRangeAreaAlignment = 64 * KB;  // OS page on PPC Linux
#elif V8_TARGET_ARCH_ARM64
constexpr size_t kMaximalCodeRangeSize = 128 * MB;
constexpr size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#else
constexpr size_t kMaximalCodeRangeSize = 128 * MB;
constexpr size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#endif
#if V8_OS_WIN
constexpr size_t kMinimumCodeRangeSize = 4 * MB;
constexpr size_t kReservedCodeRangePages = 1;
#else
constexpr size_t kMinimumCodeRangeSize = 3 * MB;
constexpr size_t kReservedCodeRangePages = 0;
#endif
#else
constexpr int kPointerSizeLog2 = 2;
constexpr intptr_t kIntptrSignBit = 0x80000000;
constexpr uintptr_t kUintptrAllBitsSet = 0xFFFFFFFFu;
#if V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
// x32 port also requires code range.
constexpr bool kRequiresCodeRange = true;
constexpr size_t kMaximalCodeRangeSize = 256 * MB;
constexpr size_t kMinimumCodeRangeSize = 3 * MB;
constexpr size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#elif V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
constexpr bool kRequiresCodeRange = false;
constexpr size_t kMaximalCodeRangeSize = 0 * MB;
constexpr size_t kMinimumCodeRangeSize = 0 * MB;
constexpr size_t kCodeRangeAreaAlignment = 64 * KB;  // OS page on PPC Linux
#else
constexpr bool kRequiresCodeRange = false;
constexpr size_t kMaximalCodeRangeSize = 0 * MB;
constexpr size_t kMinimumCodeRangeSize = 0 * MB;
constexpr size_t kCodeRangeAreaAlignment = 4 * KB;  // OS page.
#endif
constexpr size_t kReservedCodeRangePages = 0;
#endif

// Trigger an incremental GCs once the external memory reaches this limit.
constexpr int kExternalAllocationSoftLimit = 64 * MB;

// Maximum object size that gets allocated into regular pages. Objects larger
// than that size are allocated in large object space and are never moved in
// memory. This also applies to new space allocation, since objects are never
// migrated from new space to large object space. Takes double alignment into
// account.
//
// Current value: Page::kAllocatableMemory (on 32-bit arch) - 512 (slack).
constexpr int kMaxRegularHeapObjectSize = 507136;

// Objects smaller or equal kMaxNewSpaceHeapObjectSize are allocated in the
// new large object space.
constexpr int kMaxNewSpaceHeapObjectSize = 32 * KB;

STATIC_ASSERT(kPointerSize == (1 << kPointerSizeLog2));

constexpr int kBitsPerByte = 8;
constexpr int kBitsPerByteLog2 = 3;
constexpr int kBitsPerPointer = kPointerSize * kBitsPerByte;
constexpr int kBitsPerInt = kIntSize * kBitsPerByte;

// IEEE 754 single precision floating point number bit layout.
constexpr uint32_t kBinary32SignMask = 0x80000000u;
constexpr uint32_t kBinary32ExponentMask = 0x7f800000u;
constexpr uint32_t kBinary32MantissaMask = 0x007fffffu;
constexpr int kBinary32ExponentBias = 127;
constexpr int kBinary32MaxExponent = 0xFE;
constexpr int kBinary32MinExponent = 0x01;
constexpr int kBinary32MantissaBits = 23;
constexpr int kBinary32ExponentShift = 23;

// Quiet NaNs have bits 51 to 62 set, possibly the sign bit, and no
// other bits set.
constexpr uint64_t kQuietNaNMask = static_cast<uint64_t>(0xfff) << 51;

// Latin1/UTF-16 constants
// Code-point values in Unicode 4.0 are 21 bits wide.
// Code units in UTF-16 are 16 bits wide.
typedef uint16_t uc16;
typedef int32_t uc32;
constexpr int kOneByteSize = kCharSize;
constexpr int kUC16Size = sizeof(uc16);  // NOLINT

// 128 bit SIMD value size.
constexpr int kSimd128Size = 16;

// FUNCTION_ADDR(f) gets the address of a C function f.
#define FUNCTION_ADDR(f) (reinterpret_cast<v8::internal::Address>(f))

// FUNCTION_CAST<F>(addr) casts an address into a function
// of type F. Used to invoke generated code from within C.
template <typename F>
F FUNCTION_CAST(byte* addr) {
  return reinterpret_cast<F>(reinterpret_cast<Address>(addr));
}

template <typename F>
F FUNCTION_CAST(Address addr) {
  return reinterpret_cast<F>(addr);
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
// Declarations for use in both the preparser and the rest of V8.

// The Strict Mode (ECMA-262 5th edition, 4.2.2).

enum class LanguageMode : bool { kSloppy, kStrict };
static const size_t LanguageModeSize = 2;

inline size_t hash_value(LanguageMode mode) {
  return static_cast<size_t>(mode);
}

inline std::ostream& operator<<(std::ostream& os, const LanguageMode& mode) {
  switch (mode) {
    case LanguageMode::kSloppy:
      return os << "sloppy";
    case LanguageMode::kStrict:
      return os << "strict";
  }
  UNREACHABLE();
}

inline bool is_sloppy(LanguageMode language_mode) {
  return language_mode == LanguageMode::kSloppy;
}

inline bool is_strict(LanguageMode language_mode) {
  return language_mode != LanguageMode::kSloppy;
}

inline bool is_valid_language_mode(int language_mode) {
  return language_mode == static_cast<int>(LanguageMode::kSloppy) ||
         language_mode == static_cast<int>(LanguageMode::kStrict);
}

inline LanguageMode construct_language_mode(bool strict_bit) {
  return static_cast<LanguageMode>(strict_bit);
}

// Return kStrict if either of the language modes is kStrict, or kSloppy
// otherwise.
inline LanguageMode stricter_language_mode(LanguageMode mode1,
                                           LanguageMode mode2) {
  STATIC_ASSERT(LanguageModeSize == 2);
  return static_cast<LanguageMode>(static_cast<int>(mode1) |
                                   static_cast<int>(mode2));
}

enum TypeofMode : int { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };

// Enums used by CEntry.
enum SaveFPRegsMode { kDontSaveFPRegs, kSaveFPRegs };
enum ArgvMode { kArgvOnStack, kArgvInRegister };

// This constant is used as an undefined value when passing source positions.
constexpr int kNoSourcePosition = -1;

// This constant is used to indicate missing deoptimization information.
constexpr int kNoDeoptimizationId = -1;

// Deoptimize bailout kind:
// - Eager: a check failed in the optimized code and deoptimization happens
//   immediately.
// - Lazy: the code has been marked as dependent on some assumption which
//   is checked elsewhere and can trigger deoptimization the next time the
//   code is executed.
// - Soft: similar to lazy deoptimization, but does not contribute to the
//   total deopt count which can lead to disabling optimization for a function.
enum class DeoptimizeKind : uint8_t {
  kEager,
  kSoft,
  kLazy,
  kLastDeoptimizeKind = kLazy
};
inline size_t hash_value(DeoptimizeKind kind) {
  return static_cast<size_t>(kind);
}
inline std::ostream& operator<<(std::ostream& os, DeoptimizeKind kind) {
  switch (kind) {
    case DeoptimizeKind::kEager:
      return os << "Eager";
    case DeoptimizeKind::kSoft:
      return os << "Soft";
    case DeoptimizeKind::kLazy:
      return os << "Lazy";
  }
  UNREACHABLE();
}

// Indicates whether the lookup is related to sloppy-mode block-scoped
// function hoisting, and is a synthetic assignment for that.
enum class LookupHoistingMode { kNormal, kLegacySloppy };

inline std::ostream& operator<<(std::ostream& os,
                                const LookupHoistingMode& mode) {
  switch (mode) {
    case LookupHoistingMode::kNormal:
      return os << "normal hoisting";
    case LookupHoistingMode::kLegacySloppy:
      return os << "legacy sloppy hoisting";
  }
  UNREACHABLE();
}

static_assert(kSmiValueSize <= 32, "Unsupported Smi tagging scheme");
// Smi sign bit position must be 32-bit aligned so we can use sign extension
// instructions on 64-bit architectures without additional shifts.
static_assert((kSmiValueSize + kSmiShiftSize + kSmiTagSize) % 32 == 0,
              "Unsupported Smi tagging scheme");

constexpr bool kIsSmiValueInUpper32Bits =
    (kSmiValueSize + kSmiShiftSize + kSmiTagSize) == 64;
constexpr bool kIsSmiValueInLower32Bits =
    (kSmiValueSize + kSmiShiftSize + kSmiTagSize) == 32;
static_assert(!SmiValuesAre32Bits() == SmiValuesAre31Bits(),
              "Unsupported Smi tagging scheme");
static_assert(SmiValuesAre32Bits() == kIsSmiValueInUpper32Bits,
              "Unsupported Smi tagging scheme");
static_assert(SmiValuesAre31Bits() == kIsSmiValueInLower32Bits,
              "Unsupported Smi tagging scheme");

// Mask for the sign bit in a smi.
constexpr intptr_t kSmiSignMask = static_cast<intptr_t>(
    uintptr_t{1} << (kSmiValueSize + kSmiShiftSize + kSmiTagSize - 1));

constexpr int kObjectAlignmentBits = kPointerSizeLog2;
constexpr intptr_t kObjectAlignment = 1 << kObjectAlignmentBits;
constexpr intptr_t kObjectAlignmentMask = kObjectAlignment - 1;

// Desired alignment for pointers.
constexpr intptr_t kPointerAlignment = (1 << kPointerSizeLog2);
constexpr intptr_t kPointerAlignmentMask = kPointerAlignment - 1;

// Desired alignment for double values.
constexpr intptr_t kDoubleAlignment = 8;
constexpr intptr_t kDoubleAlignmentMask = kDoubleAlignment - 1;

// Desired alignment for generated code is 32 bytes (to improve cache line
// utilization).
constexpr int kCodeAlignmentBits = 5;
constexpr intptr_t kCodeAlignment = 1 << kCodeAlignmentBits;
constexpr intptr_t kCodeAlignmentMask = kCodeAlignment - 1;

const intptr_t kWeakHeapObjectMask = 1 << 1;
const intptr_t kClearedWeakHeapObject = 3;

// Zap-value: The value used for zapping dead objects.
// Should be a recognizable hex value tagged as a failure.
#ifdef V8_HOST_ARCH_64_BIT
constexpr uint64_t kClearedFreeMemoryValue = 0;
constexpr uint64_t kZapValue = uint64_t{0xdeadbeedbeadbeef};
constexpr uint64_t kHandleZapValue = uint64_t{0x1baddead0baddeaf};
constexpr uint64_t kGlobalHandleZapValue = uint64_t{0x1baffed00baffedf};
constexpr uint64_t kFromSpaceZapValue = uint64_t{0x1beefdad0beefdaf};
constexpr uint64_t kDebugZapValue = uint64_t{0xbadbaddbbadbaddb};
constexpr uint64_t kSlotsZapValue = uint64_t{0xbeefdeadbeefdeef};
constexpr uint64_t kFreeListZapValue = 0xfeed1eaffeed1eaf;
#else
constexpr uint32_t kClearedFreeMemoryValue = 0;
constexpr uint32_t kZapValue = 0xdeadbeef;
constexpr uint32_t kHandleZapValue = 0xbaddeaf;
constexpr uint32_t kGlobalHandleZapValue = 0xbaffedf;
constexpr uint32_t kFromSpaceZapValue = 0xbeefdaf;
constexpr uint32_t kSlotsZapValue = 0xbeefdeef;
constexpr uint32_t kDebugZapValue = 0xbadbaddb;
constexpr uint32_t kFreeListZapValue = 0xfeed1eaf;
#endif

constexpr int kCodeZapValue = 0xbadc0de;
constexpr uint32_t kPhantomReferenceZap = 0xca11bac;

// On Intel architecture, cache line size is 64 bytes.
// On ARM it may be less (32 bytes), but as far this constant is
// used for aligning data, it doesn't hurt to align on a greater value.
#define PROCESSOR_CACHE_LINE_SIZE 64

// Constants relevant to double precision floating point numbers.
// If looking only at the top 32 bits, the QNaN mask is bits 19 to 30.
constexpr uint32_t kQuietNaNHighBitsMask = 0xfff << (51 - 32);

// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes

class AccessorInfo;
class Arguments;
class Assembler;
class Code;
class CodeSpace;
class CodeStub;
class Context;
class Debug;
class DebugInfo;
class Descriptor;
class DescriptorArray;
class TransitionArray;
class ExternalReference;
class FixedArray;
class FreeStoreAllocationPolicy;
class FunctionTemplateInfo;
class MemoryChunk;
class NumberDictionary;
class SimpleNumberDictionary;
class NameDictionary;
class GlobalDictionary;
template <typename T> class MaybeHandle;
template <typename T> class Handle;
class Heap;
class HeapObject;
class HeapObjectReference;
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
class MaybeObject;
class NewSpace;
class NewLargeObjectSpace;
class Object;
class OldSpace;
class ParameterCount;
class ReadOnlySpace;
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
class FeedbackVector;
class Variable;
class RelocInfo;
class MessageLocation;

typedef bool (*WeakSlotCallback)(Object** pointer);

typedef bool (*WeakSlotCallbackWithHeap)(Heap* heap, Object** pointer);

// -----------------------------------------------------------------------------
// Miscellaneous

// NOTE: SpaceIterator depends on AllocationSpace enumeration values being
// consecutive.
enum AllocationSpace {
  // TODO(v8:7464): Actually map this space's memory as read-only.
  RO_SPACE,    // Immortal, immovable and immutable objects,
  NEW_SPACE,   // Young generation semispaces for regular objects collected with
               // Scavenger.
  OLD_SPACE,   // Old generation regular object space.
  CODE_SPACE,  // Old generation code object space, marked executable.
  MAP_SPACE,   // Old generation map object space, non-movable.
  LO_SPACE,    // Old generation large object space.
  NEW_LO_SPACE,  // Young generation large object space.

  FIRST_SPACE = RO_SPACE,
  LAST_SPACE = NEW_LO_SPACE,
  FIRST_GROWABLE_PAGED_SPACE = OLD_SPACE,
  LAST_GROWABLE_PAGED_SPACE = MAP_SPACE
};
constexpr int kSpaceTagSize = 3;
STATIC_ASSERT(FIRST_SPACE == 0);

enum AllocationAlignment { kWordAligned, kDoubleAligned, kDoubleUnaligned };

enum class AccessMode { ATOMIC, NON_ATOMIC };

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
}

// A flag that indicates whether objects should be pretenured when
// allocated (allocated directly into either the old generation or read-only
// space), or not (allocated in the young generation if the object size and type
// allows).
enum PretenureFlag { NOT_TENURED, TENURED, TENURED_READ_ONLY };

inline std::ostream& operator<<(std::ostream& os, const PretenureFlag& flag) {
  switch (flag) {
    case NOT_TENURED:
      return os << "NotTenured";
    case TENURED:
      return os << "Tenured";
    case TENURED_READ_ONLY:
      return os << "TenuredReadOnly";
  }
  UNREACHABLE();
}

enum MinimumCapacity {
  USE_DEFAULT_MINIMUM_CAPACITY,
  USE_CUSTOM_MINIMUM_CAPACITY
};

enum GarbageCollector { SCAVENGER, MARK_COMPACTOR, MINOR_MARK_COMPACTOR };

enum Executability { NOT_EXECUTABLE, EXECUTABLE };

enum Movability { kMovable, kImmovable };

enum VisitMode {
  VISIT_ALL,
  VISIT_ALL_IN_MINOR_MC_MARK,
  VISIT_ALL_IN_MINOR_MC_UPDATE,
  VISIT_ALL_IN_SCAVENGE,
  VISIT_ALL_IN_SWEEP_NEWSPACE,
  VISIT_ONLY_STRONG,
  VISIT_FOR_SERIALIZATION,
};

// Flag indicating whether code is built into the VM (one of the natives files).
enum NativesFlag {
  NOT_NATIVES_CODE,
  EXTENSION_CODE,
  NATIVES_CODE,
  INSPECTOR_CODE
};

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

enum WhereToStart { kStartAtReceiver, kStartAtPrototype };

enum ResultSentinel { kNotFound = -1, kUnsupported = -2 };

enum ShouldThrow { kThrowOnError, kDontThrow };

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
constexpr int kIeeeDoubleMantissaWordOffset = 0;
constexpr int kIeeeDoubleExponentWordOffset = 4;
#else
typedef IeeeDoubleBigEndianArchType IeeeDoubleArchType;
constexpr int kIeeeDoubleMantissaWordOffset = 4;
constexpr int kIeeeDoubleExponentWordOffset = 0;
#endif

// -----------------------------------------------------------------------------
// Macros

// Testers for test.

#define HAS_SMI_TAG(value) \
  ((reinterpret_cast<intptr_t>(value) & ::i::kSmiTagMask) == ::i::kSmiTag)

#define HAS_HEAP_OBJECT_TAG(value)                                   \
  (((reinterpret_cast<intptr_t>(value) & ::i::kHeapObjectTagMask) == \
    ::i::kHeapObjectTag))

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
  MIPS_SIMD,  // MSA instructions
  // PPC
  FPR_GPR_MOV,
  LWSYNC,
  ISELECT,
  VSX,
  MODULO,
  // S390
  DISTINCT_OPS,
  GENERAL_INSTR_EXT,
  FLOATING_POINT_EXT,
  VECTOR_FACILITY,
  MISC_INSTR_EXT2,

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
}

enum ScopeType : uint8_t {
  EVAL_SCOPE,      // The top-level scope for an eval source.
  FUNCTION_SCOPE,  // The top-level scope for a function.
  MODULE_SCOPE,    // The scope introduced by a module literal
  SCRIPT_SCOPE,    // The top-level scope for a script or a top-level eval.
  CATCH_SCOPE,     // The scope introduced by catch.
  BLOCK_SCOPE,     // The scope introduced by a new block.
  WITH_SCOPE       // The scope introduced by with.
};

inline std::ostream& operator<<(std::ostream& os, ScopeType type) {
  switch (type) {
    case ScopeType::EVAL_SCOPE:
      return os << "EVAL_SCOPE";
    case ScopeType::FUNCTION_SCOPE:
      return os << "FUNCTION_SCOPE";
    case ScopeType::MODULE_SCOPE:
      return os << "MODULE_SCOPE";
    case ScopeType::SCRIPT_SCOPE:
      return os << "SCRIPT_SCOPE";
    case ScopeType::CATCH_SCOPE:
      return os << "CATCH_SCOPE";
    case ScopeType::BLOCK_SCOPE:
      return os << "BLOCK_SCOPE";
    case ScopeType::WITH_SCOPE:
      return os << "WITH_SCOPE";
  }
  UNREACHABLE();
}

// AllocationSiteMode controls whether allocations are tracked by an allocation
// site.
enum AllocationSiteMode {
  DONT_TRACK_ALLOCATION_SITE,
  TRACK_ALLOCATION_SITE,
  LAST_ALLOCATION_SITE_MODE = TRACK_ALLOCATION_SITE
};

// The mips architecture prior to revision 5 has inverted encoding for sNaN.
#if (V8_TARGET_ARCH_MIPS && !defined(_MIPS_ARCH_MIPS32R6) &&           \
     (!defined(USE_SIMULATOR) || !defined(_MIPS_TARGET_SIMULATOR))) || \
    (V8_TARGET_ARCH_MIPS64 && !defined(_MIPS_ARCH_MIPS64R6) &&         \
     (!defined(USE_SIMULATOR) || !defined(_MIPS_TARGET_SIMULATOR)))
constexpr uint32_t kHoleNanUpper32 = 0xFFFF7FFF;
constexpr uint32_t kHoleNanLower32 = 0xFFFF7FFF;
#else
constexpr uint32_t kHoleNanUpper32 = 0xFFF7FFFF;
constexpr uint32_t kHoleNanLower32 = 0xFFF7FFFF;
#endif

constexpr uint64_t kHoleNanInt64 =
    (static_cast<uint64_t>(kHoleNanUpper32) << 32) | kHoleNanLower32;

// ES6 section 20.1.2.6 Number.MAX_SAFE_INTEGER
constexpr double kMaxSafeInteger = 9007199254740991.0;  // 2^53-1

// The order of this enum has to be kept in sync with the predicates below.
enum class VariableMode : uint8_t {
  // User declared variables:
  kLet,  // declared via 'let' declarations (first lexical)

  kConst,  // declared via 'const' declarations (last lexical)

  kVar,  // declared via 'var', and 'function' declarations

  // Variables introduced by the compiler:
  kTemporary,  // temporary variables (not user-visible), stack-allocated
               // unless the scope as a whole has forced context allocation

  kDynamic,  // always require dynamic lookup (we don't know
             // the declaration)

  kDynamicGlobal,  // requires dynamic lookup, but we know that the
                   // variable is global unless it has been shadowed
                   // by an eval-introduced variable

  kDynamicLocal  // requires dynamic lookup, but we know that the
                 // variable is local and where it is unless it
                 // has been shadowed by an eval-introduced
                 // variable
};

// Printing support
#ifdef DEBUG
inline const char* VariableMode2String(VariableMode mode) {
  switch (mode) {
    case VariableMode::kVar:
      return "VAR";
    case VariableMode::kLet:
      return "LET";
    case VariableMode::kConst:
      return "CONST";
    case VariableMode::kDynamic:
      return "DYNAMIC";
    case VariableMode::kDynamicGlobal:
      return "DYNAMIC_GLOBAL";
    case VariableMode::kDynamicLocal:
      return "DYNAMIC_LOCAL";
    case VariableMode::kTemporary:
      return "TEMPORARY";
  }
  UNREACHABLE();
}
#endif

enum VariableKind : uint8_t {
  NORMAL_VARIABLE,
  FUNCTION_VARIABLE,
  THIS_VARIABLE,
  SLOPPY_FUNCTION_NAME_VARIABLE
};

inline bool IsDynamicVariableMode(VariableMode mode) {
  return mode >= VariableMode::kDynamic && mode <= VariableMode::kDynamicLocal;
}

inline bool IsDeclaredVariableMode(VariableMode mode) {
  STATIC_ASSERT(static_cast<uint8_t>(VariableMode::kLet) ==
                0);  // Implies that mode >= VariableMode::kLet.
  return mode <= VariableMode::kVar;
}

inline bool IsLexicalVariableMode(VariableMode mode) {
  STATIC_ASSERT(static_cast<uint8_t>(VariableMode::kLet) ==
                0);  // Implies that mode >= VariableMode::kLet.
  return mode <= VariableMode::kConst;
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

// ES6 specifies declarative environment records with mutable and immutable
// bindings that can be in two states: initialized and uninitialized.
// When accessing a binding, it needs to be checked for initialization.
// However in the following cases the binding is initialized immediately
// after creation so the initialization check can always be skipped:
//
// 1. Var declared local variables.
//      var foo;
// 2. A local variable introduced by a function declaration.
//      function foo() {}
// 3. Parameters
//      function x(foo) {}
// 4. Catch bound variables.
//      try {} catch (foo) {}
// 6. Function name variables of named function expressions.
//      var x = function foo() {}
// 7. Implicit binding of 'this'.
// 8. Implicit binding of 'arguments' in functions.
//
// The following enum specifies a flag that indicates if the binding needs a
// distinct initialization step (kNeedsInitialization) or if the binding is
// immediately initialized upon creation (kCreatedInitialized).
enum InitializationFlag : uint8_t { kNeedsInitialization, kCreatedInitialized };

enum MaybeAssignedFlag : uint8_t { kNotAssigned, kMaybeAssigned };

// Serialized in PreparseData, so numeric values should not be changed.
enum ParseErrorType { kSyntaxError = 0, kReferenceError = 1 };

enum FunctionKind : uint8_t {
  kNormalFunction,
  kArrowFunction,
  kGeneratorFunction,
  kConciseMethod,
  kDerivedConstructor,
  kBaseConstructor,
  kGetterFunction,
  kSetterFunction,
  kAsyncFunction,
  kModule,
  kClassFieldsInitializerFunction,

  kDefaultBaseConstructor,
  kDefaultDerivedConstructor,
  kAsyncArrowFunction,
  kAsyncConciseMethod,

  kConciseGeneratorMethod,
  kAsyncConciseGeneratorMethod,
  kAsyncGeneratorFunction,
  kLastFunctionKind = kAsyncGeneratorFunction,
};

inline bool IsArrowFunction(FunctionKind kind) {
  return kind == FunctionKind::kArrowFunction ||
         kind == FunctionKind::kAsyncArrowFunction;
}

inline bool IsModule(FunctionKind kind) {
  return kind == FunctionKind::kModule;
}

inline bool IsAsyncGeneratorFunction(FunctionKind kind) {
  return kind == FunctionKind::kAsyncGeneratorFunction ||
         kind == FunctionKind::kAsyncConciseGeneratorMethod;
}

inline bool IsGeneratorFunction(FunctionKind kind) {
  return kind == FunctionKind::kGeneratorFunction ||
         kind == FunctionKind::kConciseGeneratorMethod ||
         IsAsyncGeneratorFunction(kind);
}

inline bool IsAsyncFunction(FunctionKind kind) {
  return kind == FunctionKind::kAsyncFunction ||
         kind == FunctionKind::kAsyncArrowFunction ||
         kind == FunctionKind::kAsyncConciseMethod ||
         IsAsyncGeneratorFunction(kind);
}

inline bool IsResumableFunction(FunctionKind kind) {
  return IsGeneratorFunction(kind) || IsAsyncFunction(kind) || IsModule(kind);
}

inline bool IsConciseMethod(FunctionKind kind) {
  return kind == FunctionKind::kConciseMethod ||
         kind == FunctionKind::kConciseGeneratorMethod ||
         kind == FunctionKind::kAsyncConciseMethod ||
         kind == FunctionKind::kAsyncConciseGeneratorMethod ||
         kind == FunctionKind::kClassFieldsInitializerFunction;
}

inline bool IsGetterFunction(FunctionKind kind) {
  return kind == FunctionKind::kGetterFunction;
}

inline bool IsSetterFunction(FunctionKind kind) {
  return kind == FunctionKind::kSetterFunction;
}

inline bool IsAccessorFunction(FunctionKind kind) {
  return kind == FunctionKind::kGetterFunction ||
         kind == FunctionKind::kSetterFunction;
}

inline bool IsDefaultConstructor(FunctionKind kind) {
  return kind == FunctionKind::kDefaultBaseConstructor ||
         kind == FunctionKind::kDefaultDerivedConstructor;
}

inline bool IsBaseConstructor(FunctionKind kind) {
  return kind == FunctionKind::kBaseConstructor ||
         kind == FunctionKind::kDefaultBaseConstructor;
}

inline bool IsDerivedConstructor(FunctionKind kind) {
  return kind == FunctionKind::kDerivedConstructor ||
         kind == FunctionKind::kDefaultDerivedConstructor;
}


inline bool IsClassConstructor(FunctionKind kind) {
  return IsBaseConstructor(kind) || IsDerivedConstructor(kind);
}

inline bool IsClassFieldsInitializerFunction(FunctionKind kind) {
  return kind == FunctionKind::kClassFieldsInitializerFunction;
}

inline bool IsConstructable(FunctionKind kind) {
  if (IsAccessorFunction(kind)) return false;
  if (IsConciseMethod(kind)) return false;
  if (IsArrowFunction(kind)) return false;
  if (IsGeneratorFunction(kind)) return false;
  if (IsAsyncFunction(kind)) return false;
  return true;
}

inline std::ostream& operator<<(std::ostream& os, FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kNormalFunction:
      return os << "NormalFunction";
    case FunctionKind::kArrowFunction:
      return os << "ArrowFunction";
    case FunctionKind::kGeneratorFunction:
      return os << "GeneratorFunction";
    case FunctionKind::kConciseMethod:
      return os << "ConciseMethod";
    case FunctionKind::kDerivedConstructor:
      return os << "DerivedConstructor";
    case FunctionKind::kBaseConstructor:
      return os << "BaseConstructor";
    case FunctionKind::kGetterFunction:
      return os << "GetterFunction";
    case FunctionKind::kSetterFunction:
      return os << "SetterFunction";
    case FunctionKind::kAsyncFunction:
      return os << "AsyncFunction";
    case FunctionKind::kModule:
      return os << "Module";
    case FunctionKind::kClassFieldsInitializerFunction:
      return os << "ClassFieldsInitializerFunction";
    case FunctionKind::kDefaultBaseConstructor:
      return os << "DefaultBaseConstructor";
    case FunctionKind::kDefaultDerivedConstructor:
      return os << "DefaultDerivedConstructor";
    case FunctionKind::kAsyncArrowFunction:
      return os << "AsyncArrowFunction";
    case FunctionKind::kAsyncConciseMethod:
      return os << "AsyncConciseMethod";
    case FunctionKind::kConciseGeneratorMethod:
      return os << "ConciseGeneratorMethod";
    case FunctionKind::kAsyncConciseGeneratorMethod:
      return os << "AsyncConciseGeneratorMethod";
    case FunctionKind::kAsyncGeneratorFunction:
      return os << "AsyncGeneratorFunction";
  }
  UNREACHABLE();
}

enum class InterpreterPushArgsMode : unsigned {
  kArrayFunction,
  kWithFinalSpread,
  kOther
};

inline size_t hash_value(InterpreterPushArgsMode mode) {
  return bit_cast<unsigned>(mode);
}

inline std::ostream& operator<<(std::ostream& os,
                                InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kArrayFunction:
      return os << "ArrayFunction";
    case InterpreterPushArgsMode::kWithFinalSpread:
      return os << "WithFinalSpread";
    case InterpreterPushArgsMode::kOther:
      return os << "Other";
  }
  UNREACHABLE();
}

inline uint32_t ObjectHash(Address address) {
  // All objects are at least pointer aligned, so we can remove the trailing
  // zeros.
  return static_cast<uint32_t>(address >> kPointerSizeLog2);
}

// Type feedback is encoded in such a way that, we can combine the feedback
// at different points by performing an 'OR' operation. Type feedback moves
// to a more generic type when we combine feedback.
//
//   kSignedSmall -> kSignedSmallInputs -> kNumber  -> kNumberOrOddball -> kAny
//                                                     kString          -> kAny
//                                                     kBigInt          -> kAny
//
// Technically we wouldn't need the separation between the kNumber and the
// kNumberOrOddball values here, since for binary operations, we always
// truncate oddballs to numbers. In practice though it causes TurboFan to
// generate quite a lot of unused code though if we always handle numbers
// and oddballs everywhere, although in 99% of the use sites they are only
// used with numbers.
class BinaryOperationFeedback {
 public:
  enum {
    kNone = 0x0,
    kSignedSmall = 0x1,
    kSignedSmallInputs = 0x3,
    kNumber = 0x7,
    kNumberOrOddball = 0xF,
    kString = 0x10,
    kBigInt = 0x20,
    kAny = 0x7F
  };
};

// Type feedback is encoded in such a way that, we can combine the feedback
// at different points by performing an 'OR' operation. Type feedback moves
// to a more generic type when we combine feedback.
//
//   kSignedSmall -> kNumber             -> kNumberOrOddball -> kAny
//                   kInternalizedString -> kString          -> kAny
//                                          kSymbol          -> kAny
//                                          kBigInt          -> kAny
//                                          kReceiver        -> kAny
//
// This is distinct from BinaryOperationFeedback on purpose, because the
// feedback that matters differs greatly as well as the way it is consumed.
class CompareOperationFeedback {
 public:
  enum {
    kNone = 0x00,
    kSignedSmall = 0x01,
    kNumber = 0x3,
    kNumberOrOddball = 0x7,
    kInternalizedString = 0x8,
    kString = 0x18,
    kSymbol = 0x20,
    kBigInt = 0x30,
    kReceiver = 0x40,
    kAny = 0xff
  };
};

enum class Operation {
  // Binary operations.
  kAdd,
  kSubtract,
  kMultiply,
  kDivide,
  kModulus,
  kExponentiate,
  kBitwiseAnd,
  kBitwiseOr,
  kBitwiseXor,
  kShiftLeft,
  kShiftRight,
  kShiftRightLogical,
  // Unary operations.
  kBitwiseNot,
  kNegate,
  kIncrement,
  kDecrement,
  // Compare operations.
  kEqual,
  kStrictEqual,
  kLessThan,
  kLessThanOrEqual,
  kGreaterThan,
  kGreaterThanOrEqual,
};

// Type feedback is encoded in such a way that, we can combine the feedback
// at different points by performing an 'OR' operation. Type feedback moves
// to a more generic type when we combine feedback.
// kNone -> kEnumCacheKeysAndIndices -> kEnumCacheKeys -> kAny
class ForInFeedback {
 public:
  enum {
    kNone = 0x0,
    kEnumCacheKeysAndIndices = 0x1,
    kEnumCacheKeys = 0x3,
    kAny = 0x7
  };
};
STATIC_ASSERT((ForInFeedback::kNone |
               ForInFeedback::kEnumCacheKeysAndIndices) ==
              ForInFeedback::kEnumCacheKeysAndIndices);
STATIC_ASSERT((ForInFeedback::kEnumCacheKeysAndIndices |
               ForInFeedback::kEnumCacheKeys) == ForInFeedback::kEnumCacheKeys);
STATIC_ASSERT((ForInFeedback::kEnumCacheKeys | ForInFeedback::kAny) ==
              ForInFeedback::kAny);

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
}

enum class IterationKind { kKeys, kValues, kEntries };

inline std::ostream& operator<<(std::ostream& os, IterationKind kind) {
  switch (kind) {
    case IterationKind::kKeys:
      return os << "IterationKind::kKeys";
    case IterationKind::kValues:
      return os << "IterationKind::kValues";
    case IterationKind::kEntries:
      return os << "IterationKind::kEntries";
  }
  UNREACHABLE();
}

enum class CollectionKind { kMap, kSet };

inline std::ostream& operator<<(std::ostream& os, CollectionKind kind) {
  switch (kind) {
    case CollectionKind::kMap:
      return os << "CollectionKind::kMap";
    case CollectionKind::kSet:
      return os << "CollectionKind::kSet";
  }
  UNREACHABLE();
}

// Flags for the runtime function kDefineDataPropertyInLiteral. A property can
// be enumerable or not, and, in case of functions, the function name
// can be set or not.
enum class DataPropertyInLiteralFlag {
  kNoFlags = 0,
  kDontEnum = 1 << 0,
  kSetFunctionName = 1 << 1
};
typedef base::Flags<DataPropertyInLiteralFlag> DataPropertyInLiteralFlags;
DEFINE_OPERATORS_FOR_FLAGS(DataPropertyInLiteralFlags)

enum ExternalArrayType {
  kExternalInt8Array = 1,
  kExternalUint8Array,
  kExternalInt16Array,
  kExternalUint16Array,
  kExternalInt32Array,
  kExternalUint32Array,
  kExternalFloat32Array,
  kExternalFloat64Array,
  kExternalUint8ClampedArray,
  kExternalBigInt64Array,
  kExternalBigUint64Array,
};

struct AssemblerDebugInfo {
  AssemblerDebugInfo(const char* name, const char* file, int line)
      : name(name), file(file), line(line) {}
  const char* name;
  const char* file;
  int line;
};

inline std::ostream& operator<<(std::ostream& os,
                                const AssemblerDebugInfo& info) {
  os << "(" << info.name << ":" << info.file << ":" << info.line << ")";
  return os;
}

enum class OptimizationMarker {
  kLogFirstExecution,
  kNone,
  kCompileOptimized,
  kCompileOptimizedConcurrent,
  kInOptimizationQueue
};

inline std::ostream& operator<<(std::ostream& os,
                                const OptimizationMarker& marker) {
  switch (marker) {
    case OptimizationMarker::kLogFirstExecution:
      return os << "OptimizationMarker::kLogFirstExecution";
    case OptimizationMarker::kNone:
      return os << "OptimizationMarker::kNone";
    case OptimizationMarker::kCompileOptimized:
      return os << "OptimizationMarker::kCompileOptimized";
    case OptimizationMarker::kCompileOptimizedConcurrent:
      return os << "OptimizationMarker::kCompileOptimizedConcurrent";
    case OptimizationMarker::kInOptimizationQueue:
      return os << "OptimizationMarker::kInOptimizationQueue";
  }
  UNREACHABLE();
  return os;
}

enum class SpeculationMode { kAllowSpeculation, kDisallowSpeculation };

inline std::ostream& operator<<(std::ostream& os,
                                SpeculationMode speculation_mode) {
  switch (speculation_mode) {
    case SpeculationMode::kAllowSpeculation:
      return os << "SpeculationMode::kAllowSpeculation";
    case SpeculationMode::kDisallowSpeculation:
      return os << "SpeculationMode::kDisallowSpeculation";
  }
  UNREACHABLE();
  return os;
}

enum class BlockingBehavior { kBlock, kDontBlock };

enum class ConcurrencyMode { kNotConcurrent, kConcurrent };

#define FOR_EACH_ISOLATE_ADDRESS_NAME(C)                       \
  C(Handler, handler)                                          \
  C(CEntryFP, c_entry_fp)                                      \
  C(CFunction, c_function)                                     \
  C(Context, context)                                          \
  C(PendingException, pending_exception)                       \
  C(PendingHandlerContext, pending_handler_context)            \
  C(PendingHandlerEntrypoint, pending_handler_entrypoint)      \
  C(PendingHandlerConstantPool, pending_handler_constant_pool) \
  C(PendingHandlerFP, pending_handler_fp)                      \
  C(PendingHandlerSP, pending_handler_sp)                      \
  C(ExternalCaughtException, external_caught_exception)        \
  C(JSEntrySP, js_entry_sp)

enum IsolateAddressId {
#define DECLARE_ENUM(CamelName, hacker_name) k##CamelName##Address,
  FOR_EACH_ISOLATE_ADDRESS_NAME(DECLARE_ENUM)
#undef DECLARE_ENUM
      kIsolateAddressCount
};

V8_INLINE static bool HasWeakHeapObjectTag(const internal::MaybeObject* value) {
  return ((reinterpret_cast<intptr_t>(value) & kHeapObjectTagMask) ==
          kWeakHeapObjectTag);
}

// Object* should never have the weak tag; this variant is for overzealous
// checking.
V8_INLINE static bool HasWeakHeapObjectTag(const Object* value) {
  return ((reinterpret_cast<intptr_t>(value) & kHeapObjectTagMask) ==
          kWeakHeapObjectTag);
}

V8_INLINE static bool IsClearedWeakHeapObject(MaybeObject* value) {
  return reinterpret_cast<intptr_t>(value) == kClearedWeakHeapObject;
}

V8_INLINE static HeapObject* RemoveWeakHeapObjectMask(
    HeapObjectReference* value) {
  return reinterpret_cast<HeapObject*>(reinterpret_cast<intptr_t>(value) &
                                       ~kWeakHeapObjectMask);
}

V8_INLINE static HeapObjectReference* AddWeakHeapObjectMask(Object* value) {
  return reinterpret_cast<HeapObjectReference*>(
      reinterpret_cast<intptr_t>(value) | kWeakHeapObjectMask);
}

V8_INLINE static MaybeObject* AddWeakHeapObjectMask(MaybeObject* value) {
  return reinterpret_cast<MaybeObject*>(reinterpret_cast<intptr_t>(value) |
                                        kWeakHeapObjectMask);
}

enum class HeapObjectReferenceType {
  WEAK,
  STRONG,
};

enum class PoisoningMitigationLevel {
  kPoisonAll,
  kDontPoison,
  kPoisonCriticalOnly
};

enum class LoadSensitivity {
  kCritical,  // Critical loads are poisoned whenever we can run untrusted
              // code (i.e., when --untrusted-code-mitigations is on).
  kUnsafe,    // Unsafe loads are poisoned when full poisoning is on
              // (--branch-load-poisoning).
  kSafe       // Safe loads are never poisoned.
};

// The reason for a WebAssembly trap.
#define FOREACH_WASM_TRAPREASON(V) \
  V(TrapUnreachable)               \
  V(TrapMemOutOfBounds)            \
  V(TrapDivByZero)                 \
  V(TrapDivUnrepresentable)        \
  V(TrapRemByZero)                 \
  V(TrapFloatUnrepresentable)      \
  V(TrapFuncInvalid)               \
  V(TrapFuncSigMismatch)

}  // namespace internal
}  // namespace v8

namespace i = v8::internal;

#endif  // V8_GLOBALS_H_
