// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_GLOBALS_H_
#define V8_COMMON_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <ostream>

#include "include/v8-internal.h"
#include "src/base/atomic-utils.h"
#include "src/base/build_config.h"
#include "src/base/flags.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

#define V8_INFINITY std::numeric_limits<double>::infinity()

namespace v8 {

namespace base {
class Mutex;
class RecursiveMutex;
}  // namespace base

namespace internal {

constexpr int KB = 1024;
constexpr int MB = KB * 1024;
constexpr int GB = MB * 1024;

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
#define V8_EMBEDDED_CONSTANT_POOL true
#else
#define V8_EMBEDDED_CONSTANT_POOL false
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

// In order to emit more efficient stack checks in optimized code,
// deoptimization may implicitly exceed the V8 stack limit by this many bytes.
// Stack checks in functions with `difference between optimized and unoptimized
// stack frame sizes <= slack` can simply emit the simple stack check.
constexpr int kStackLimitSlackForDeoptimizationInBytes = 256;

// Sanity-check, assuming that we aim for a real OS stack size of at least 1MB.
STATIC_ASSERT(V8_DEFAULT_STACK_SIZE_KB* KB +
                  kStackLimitSlackForDeoptimizationInBytes <=
              MB);

// Determine whether double field unboxing feature is enabled.
#if V8_TARGET_ARCH_64_BIT && !defined(V8_COMPRESS_POINTERS)
#define V8_DOUBLE_FIELDS_UNBOXING false
#else
#define V8_DOUBLE_FIELDS_UNBOXING false
#endif

// Determine whether tagged pointers are 8 bytes (used in Torque layouts for
// choosing where to insert padding).
#if V8_TARGET_ARCH_64_BIT && !defined(V8_COMPRESS_POINTERS)
#define TAGGED_SIZE_8_BYTES true
#else
#define TAGGED_SIZE_8_BYTES false
#endif

// Some types of tracing require the SFI to store a unique ID.
#if defined(V8_TRACE_MAPS) || defined(V8_TRACE_IGNITION)
#define V8_SFI_HAS_UNIQUE_ID true
#else
#define V8_SFI_HAS_UNIQUE_ID false
#endif

#if defined(V8_OS_WIN) && defined(V8_TARGET_ARCH_X64)
#define V8_OS_WIN_X64 true
#endif

#if defined(V8_OS_WIN) && defined(V8_TARGET_ARCH_ARM64)
#define V8_OS_WIN_ARM64 true
#endif

#if defined(V8_OS_WIN_X64) || defined(V8_OS_WIN_ARM64)
#define V8_OS_WIN64 true
#endif

// Superclass for classes only using static method functions.
// The subclass of AllStatic cannot be instantiated at all.
class AllStatic {
#ifdef DEBUG
 public:
  AllStatic() = delete;
#endif
};

using byte = uint8_t;

// -----------------------------------------------------------------------------
// Constants

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
constexpr int kByteSize = sizeof(byte);
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
constexpr int kSystemPointerSize = sizeof(void*);
constexpr int kSystemPointerHexDigits = kSystemPointerSize == 4 ? 8 : 12;
constexpr int kPCOnStackSize = kSystemPointerSize;
constexpr int kFPOnStackSize = kSystemPointerSize;

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
constexpr int kElidedFrameSlots = kPCOnStackSize / kSystemPointerSize;
#else
constexpr int kElidedFrameSlots = 0;
#endif

constexpr int kDoubleSizeLog2 = 3;
constexpr size_t kMaxWasmCodeMB = 1024;
constexpr size_t kMaxWasmCodeMemory = kMaxWasmCodeMB * MB;
#if V8_TARGET_ARCH_ARM64
// ARM64 only supports direct calls within a 128 MB range.
constexpr size_t kMaxWasmCodeSpaceSize = 128 * MB;
#else
constexpr size_t kMaxWasmCodeSpaceSize = kMaxWasmCodeMemory;
#endif

#if V8_HOST_ARCH_64_BIT
constexpr int kSystemPointerSizeLog2 = 3;
constexpr intptr_t kIntptrSignBit =
    static_cast<intptr_t>(uintptr_t{0x8000000000000000});
constexpr bool kRequiresCodeRange = true;
#if V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
constexpr size_t kMaximalCodeRangeSize = 512 * MB;
constexpr size_t kMinExpectedOSPageSize = 64 * KB;  // OS page on PPC Linux
#elif V8_TARGET_ARCH_ARM64
constexpr size_t kMaximalCodeRangeSize = 128 * MB;
constexpr size_t kMinExpectedOSPageSize = 4 * KB;  // OS page.
#else
constexpr size_t kMaximalCodeRangeSize = 128 * MB;
constexpr size_t kMinExpectedOSPageSize = 4 * KB;  // OS page.
#endif
#if V8_OS_WIN
constexpr size_t kMinimumCodeRangeSize = 4 * MB;
constexpr size_t kReservedCodeRangePages = 1;
#else
constexpr size_t kMinimumCodeRangeSize = 3 * MB;
constexpr size_t kReservedCodeRangePages = 0;
#endif
#else
constexpr int kSystemPointerSizeLog2 = 2;
constexpr intptr_t kIntptrSignBit = 0x80000000;
#if V8_HOST_ARCH_PPC && V8_TARGET_ARCH_PPC && V8_OS_LINUX
constexpr bool kRequiresCodeRange = false;
constexpr size_t kMaximalCodeRangeSize = 0 * MB;
constexpr size_t kMinimumCodeRangeSize = 0 * MB;
constexpr size_t kMinExpectedOSPageSize = 64 * KB;  // OS page on PPC Linux
#elif V8_TARGET_ARCH_MIPS
constexpr bool kRequiresCodeRange = false;
constexpr size_t kMaximalCodeRangeSize = 2048LL * MB;
constexpr size_t kMinimumCodeRangeSize = 0 * MB;
constexpr size_t kMinExpectedOSPageSize = 4 * KB;  // OS page.
#else
constexpr bool kRequiresCodeRange = false;
constexpr size_t kMaximalCodeRangeSize = 0 * MB;
constexpr size_t kMinimumCodeRangeSize = 0 * MB;
constexpr size_t kMinExpectedOSPageSize = 4 * KB;  // OS page.
#endif
constexpr size_t kReservedCodeRangePages = 0;
#endif

STATIC_ASSERT(kSystemPointerSize == (1 << kSystemPointerSizeLog2));

#ifdef V8_COMPRESS_POINTERS
static_assert(
    kSystemPointerSize == kInt64Size,
    "Pointer compression can be enabled only for 64-bit architectures");

constexpr int kTaggedSize = kInt32Size;
constexpr int kTaggedSizeLog2 = 2;

// These types define raw and atomic storage types for tagged values stored
// on V8 heap.
using Tagged_t = uint32_t;
using AtomicTagged_t = base::Atomic32;

#else

constexpr int kTaggedSize = kSystemPointerSize;
constexpr int kTaggedSizeLog2 = kSystemPointerSizeLog2;

// These types define raw and atomic storage types for tagged values stored
// on V8 heap.
using Tagged_t = Address;
using AtomicTagged_t = base::AtomicWord;

#endif  // V8_COMPRESS_POINTERS

STATIC_ASSERT(kTaggedSize == (1 << kTaggedSizeLog2));
STATIC_ASSERT((kTaggedSize == 8) == TAGGED_SIZE_8_BYTES);

using AsAtomicTagged = base::AsAtomicPointerImpl<AtomicTagged_t>;
STATIC_ASSERT(sizeof(Tagged_t) == kTaggedSize);
STATIC_ASSERT(sizeof(AtomicTagged_t) == kTaggedSize);

STATIC_ASSERT(kTaggedSize == kApiTaggedSize);

// TODO(ishell): use kTaggedSize or kSystemPointerSize instead.
#ifndef V8_COMPRESS_POINTERS
constexpr int kPointerSize = kSystemPointerSize;
constexpr int kPointerSizeLog2 = kSystemPointerSizeLog2;
STATIC_ASSERT(kPointerSize == (1 << kPointerSizeLog2));
#endif

constexpr int kEmbedderDataSlotSize = kSystemPointerSize;

constexpr int kEmbedderDataSlotSizeInTaggedSlots =
    kEmbedderDataSlotSize / kTaggedSize;
STATIC_ASSERT(kEmbedderDataSlotSize >= kSystemPointerSize);

constexpr int kExternalAllocationSoftLimit =
    internal::Internals::kExternalAllocationSoftLimit;

// Maximum object size that gets allocated into regular pages. Objects larger
// than that size are allocated in large object space and are never moved in
// memory. This also applies to new space allocation, since objects are never
// migrated from new space to large object space. Takes double alignment into
// account.
//
// Current value: half of the page size.
constexpr int kMaxRegularHeapObjectSize = (1 << (kPageSizeBits - 1));

constexpr int kBitsPerByte = 8;
constexpr int kBitsPerByteLog2 = 3;
constexpr int kBitsPerSystemPointer = kSystemPointerSize * kBitsPerByte;
constexpr int kBitsPerSystemPointerLog2 =
    kSystemPointerSizeLog2 + kBitsPerByteLog2;
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
using uc16 = uint16_t;
using uc32 = int32_t;
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
#if V8_HOST_ARCH_PPC &&                                            \
    (V8_OS_AIX || (V8_TARGET_ARCH_PPC64 && V8_TARGET_BIG_ENDIAN && \
                   (!defined(_CALL_ELF) || _CALL_ELF == 1)))
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

inline const char* LanguageMode2String(LanguageMode mode) {
  switch (mode) {
    case LanguageMode::kSloppy:
      return "sloppy";
    case LanguageMode::kStrict:
      return "strict";
  }
  UNREACHABLE();
}

inline std::ostream& operator<<(std::ostream& os, LanguageMode mode) {
  return os << LanguageMode2String(mode);
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

// A non-keyed store is of the form a.x = foo or a["x"] = foo whereas
// a keyed store is of the form a[expression] = foo.
enum class StoreOrigin { kMaybeKeyed, kNamed };

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

enum class IsolateAllocationMode {
  // Allocate Isolate in C++ heap using default new/delete operators.
  kInCppHeap,

  // Allocate Isolate in a committed region inside V8 heap reservation.
  kInV8Heap,

#ifdef V8_COMPRESS_POINTERS
  kDefault = kInV8Heap,
#else
  kDefault = kInCppHeap,
#endif
};

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

// Desired alignment for tagged pointers.
constexpr int kObjectAlignmentBits = kTaggedSizeLog2;
constexpr intptr_t kObjectAlignment = 1 << kObjectAlignmentBits;
constexpr intptr_t kObjectAlignmentMask = kObjectAlignment - 1;

// Desired alignment for system pointers.
constexpr intptr_t kPointerAlignment = (1 << kSystemPointerSizeLog2);
constexpr intptr_t kPointerAlignmentMask = kPointerAlignment - 1;

// Desired alignment for double values.
constexpr intptr_t kDoubleAlignment = 8;
constexpr intptr_t kDoubleAlignmentMask = kDoubleAlignment - 1;

// Desired alignment for generated code is 32 bytes (to improve cache line
// utilization).
constexpr int kCodeAlignmentBits = 5;
constexpr intptr_t kCodeAlignment = 1 << kCodeAlignmentBits;
constexpr intptr_t kCodeAlignmentMask = kCodeAlignment - 1;

const Address kWeakHeapObjectMask = 1 << 1;

// The lower 32 bits of the cleared weak reference value is always equal to
// the |kClearedWeakHeapObjectLower32| constant but on 64-bit architectures
// the value of the upper 32 bits part may be
// 1) zero when pointer compression is disabled,
// 2) upper 32 bits of the isolate root value when pointer compression is
//    enabled.
// This is necessary to make pointer decompression computation also suitable
// for cleared weak reference.
// Note, that real heap objects can't have lower 32 bits equal to 3 because
// this offset belongs to page header. So, in either case it's enough to
// compare only the lower 32 bits of a MaybeObject value in order to figure
// out if it's a cleared reference or not.
const uint32_t kClearedWeakHeapObjectLower32 = 3;

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

// Page constants.
static const intptr_t kPageAlignmentMask = (intptr_t{1} << kPageSizeBits) - 1;

// On Intel architecture, cache line size is 64 bytes.
// On ARM it may be less (32 bytes), but as far this constant is
// used for aligning data, it doesn't hurt to align on a greater value.
#define PROCESSOR_CACHE_LINE_SIZE 64

// Constants relevant to double precision floating point numbers.
// If looking only at the top 32 bits, the QNaN mask is bits 19 to 30.
constexpr uint32_t kQuietNaNHighBitsMask = 0xfff << (51 - 32);

enum class HeapObjectReferenceType {
  WEAK,
  STRONG,
};

// -----------------------------------------------------------------------------
// Forward declarations for frequently used classes

class AccessorInfo;
class Arguments;
class Assembler;
class ClassScope;
class Code;
class CodeSpace;
class Context;
class DeclarationScope;
class Debug;
class DebugInfo;
class Descriptor;
class DescriptorArray;
class TransitionArray;
class ExternalReference;
class FeedbackVector;
class FixedArray;
class Foreign;
class FreeStoreAllocationPolicy;
class FunctionTemplateInfo;
class GlobalDictionary;
template <typename T>
class Handle;
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
class MacroAssembler;
class Map;
class MapSpace;
class MarkCompactCollector;
template <typename T>
class MaybeHandle;
class MaybeObject;
class MemoryChunk;
class MessageLocation;
class ModuleScope;
class Name;
class NameDictionary;
class NativeContext;
class NewSpace;
class NewLargeObjectSpace;
class NumberDictionary;
class Object;
class OldLargeObjectSpace;
template <HeapObjectReferenceType kRefType, typename StorageType>
class TaggedImpl;
class StrongTaggedValue;
class TaggedValue;
class CompressedObjectSlot;
class CompressedMaybeObjectSlot;
class CompressedMapWordSlot;
class CompressedHeapObjectSlot;
class FullObjectSlot;
class FullMaybeObjectSlot;
class FullHeapObjectSlot;
class OldSpace;
class ReadOnlySpace;
class RelocInfo;
class Scope;
class ScopeInfo;
class Script;
class SimpleNumberDictionary;
class Smi;
template <typename Config, class Allocator = FreeStoreAllocationPolicy>
class SplayTree;
class String;
class StringStream;
class Struct;
class Symbol;
class Variable;

enum class SlotLocation { kOnHeap, kOffHeap };

template <SlotLocation slot_location>
struct SlotTraits;

// Off-heap slots are always full-pointer slots.
template <>
struct SlotTraits<SlotLocation::kOffHeap> {
  using TObjectSlot = FullObjectSlot;
  using TMaybeObjectSlot = FullMaybeObjectSlot;
  using THeapObjectSlot = FullHeapObjectSlot;
};

// On-heap slots are either full-pointer slots or compressed slots depending
// on whether the pointer compression is enabled or not.
template <>
struct SlotTraits<SlotLocation::kOnHeap> {
#ifdef V8_COMPRESS_POINTERS
  using TObjectSlot = CompressedObjectSlot;
  using TMaybeObjectSlot = CompressedMaybeObjectSlot;
  using THeapObjectSlot = CompressedHeapObjectSlot;
#else
  using TObjectSlot = FullObjectSlot;
  using TMaybeObjectSlot = FullMaybeObjectSlot;
  using THeapObjectSlot = FullHeapObjectSlot;
#endif
};

// An ObjectSlot instance describes a kTaggedSize-sized on-heap field ("slot")
// holding Object value (smi or strong heap object).
using ObjectSlot = SlotTraits<SlotLocation::kOnHeap>::TObjectSlot;

// A MaybeObjectSlot instance describes a kTaggedSize-sized on-heap field
// ("slot") holding MaybeObject (smi or weak heap object or strong heap object).
using MaybeObjectSlot = SlotTraits<SlotLocation::kOnHeap>::TMaybeObjectSlot;

// A HeapObjectSlot instance describes a kTaggedSize-sized field ("slot")
// holding a weak or strong pointer to a heap object (think:
// HeapObjectReference).
using HeapObjectSlot = SlotTraits<SlotLocation::kOnHeap>::THeapObjectSlot;

using WeakSlotCallback = bool (*)(FullObjectSlot pointer);

using WeakSlotCallbackWithHeap = bool (*)(Heap* heap, FullObjectSlot pointer);

// -----------------------------------------------------------------------------
// Miscellaneous

// NOTE: SpaceIterator depends on AllocationSpace enumeration values being
// consecutive.
enum AllocationSpace {
  RO_SPACE,    // Immortal, immovable and immutable objects,
  NEW_SPACE,   // Young generation semispaces for regular objects collected with
               // Scavenger.
  OLD_SPACE,   // Old generation regular object space.
  CODE_SPACE,  // Old generation code object space, marked executable.
  MAP_SPACE,   // Old generation map object space, non-movable.
  LO_SPACE,    // Old generation large object space.
  CODE_LO_SPACE,  // Old generation large code object space.
  NEW_LO_SPACE,   // Young generation large object space.

  FIRST_SPACE = RO_SPACE,
  LAST_SPACE = NEW_LO_SPACE,
  FIRST_MUTABLE_SPACE = NEW_SPACE,
  LAST_MUTABLE_SPACE = NEW_LO_SPACE,
  FIRST_GROWABLE_PAGED_SPACE = OLD_SPACE,
  LAST_GROWABLE_PAGED_SPACE = MAP_SPACE
};
constexpr int kSpaceTagSize = 4;
STATIC_ASSERT(FIRST_SPACE == 0);

enum class AllocationType : uint8_t {
  kYoung,    // Regular object allocated in NEW_SPACE or NEW_LO_SPACE
  kOld,      // Regular object allocated in OLD_SPACE or LO_SPACE
  kCode,     // Code object allocated in CODE_SPACE or CODE_LO_SPACE
  kMap,      // Map object allocated in MAP_SPACE
  kReadOnly  // Object allocated in RO_SPACE
};

inline size_t hash_value(AllocationType kind) {
  return static_cast<uint8_t>(kind);
}

inline std::ostream& operator<<(std::ostream& os, AllocationType kind) {
  switch (kind) {
    case AllocationType::kYoung:
      return os << "Young";
    case AllocationType::kOld:
      return os << "Old";
    case AllocationType::kCode:
      return os << "Code";
    case AllocationType::kMap:
      return os << "Map";
    case AllocationType::kReadOnly:
      return os << "ReadOnly";
  }
  UNREACHABLE();
}

// TODO(ishell): review and rename kWordAligned to kTaggedAligned.
enum AllocationAlignment {
  kWordAligned,
  kDoubleAligned,
  kDoubleUnaligned,
  kCodeAligned
};

enum class AccessMode { ATOMIC, NON_ATOMIC };

enum class AllowLargeObjects { kFalse, kTrue };

enum MinimumCapacity {
  USE_DEFAULT_MINIMUM_CAPACITY,
  USE_CUSTOM_MINIMUM_CAPACITY
};

enum GarbageCollector { SCAVENGER, MARK_COMPACTOR, MINOR_MARK_COMPACTOR };

enum class LocalSpaceKind {
  kNone,
  kOffThreadSpace,
  kCompactionSpaceForScavenge,
  kCompactionSpaceForMarkCompact,
  kCompactionSpaceForMinorMarkCompact,

  kFirstCompactionSpace = kCompactionSpaceForScavenge,
  kLastCompactionSpace = kCompactionSpaceForMinorMarkCompact,
};

enum Executability { NOT_EXECUTABLE, EXECUTABLE };

enum VisitMode {
  VISIT_ALL,
  VISIT_ALL_IN_MINOR_MC_MARK,
  VISIT_ALL_IN_MINOR_MC_UPDATE,
  VISIT_ALL_IN_SCAVENGE,
  VISIT_ALL_IN_SWEEP_NEWSPACE,
  VISIT_ONLY_STRONG,
  VISIT_ONLY_STRONG_IGNORE_STACK,
  VISIT_FOR_SERIALIZATION,
};

enum class BytecodeFlushMode {
  kDoNotFlushBytecode,
  kFlushBytecode,
  kStressFlushBytecode,
};

// Indicates whether a script should be parsed and compiled in REPL mode.
enum class REPLMode {
  kYes,
  kNo,
};

// Flag indicating whether code is built into the VM (one of the natives files).
enum NativesFlag { NOT_NATIVES_CODE, EXTENSION_CODE, INSPECTOR_CODE };

// ParseRestriction is used to restrict the set of valid statements in a
// unit of compilation.  Restriction violations cause a syntax error.
enum ParseRestriction {
  NO_PARSE_RESTRICTION,         // All expressions are allowed.
  ONLY_SINGLE_FUNCTION_LITERAL  // Only a single FunctionLiteral expression.
};

// State for inline cache call sites. Aliased as IC::State.
enum InlineCacheState {
  // No feedback will be collected.
  NO_FEEDBACK,
  // Has never been executed.
  UNINITIALIZED,
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

// Printing support.
inline const char* InlineCacheState2String(InlineCacheState state) {
  switch (state) {
    case NO_FEEDBACK:
      return "NOFEEDBACK";
    case UNINITIALIZED:
      return "UNINITIALIZED";
    case MONOMORPHIC:
      return "MONOMORPHIC";
    case RECOMPUTE_HANDLER:
      return "RECOMPUTE_HANDLER";
    case POLYMORPHIC:
      return "POLYMORPHIC";
    case MEGAMORPHIC:
      return "MEGAMORPHIC";
    case GENERIC:
      return "GENERIC";
  }
  UNREACHABLE();
}

enum WhereToStart { kStartAtReceiver, kStartAtPrototype };

enum ResultSentinel { kNotFound = -1, kUnsupported = -2 };

enum ShouldThrow {
  kThrowOnError = Internals::kThrowOnError,
  kDontThrow = Internals::kDontThrow
};

// The Store Buffer (GC).
enum StoreBufferEvent {
  kStoreBufferFullEvent,
  kStoreBufferStartScanningPagesEvent,
  kStoreBufferScanningPageEvent
};

using StoreBufferCallback = void (*)(Heap* heap, MemoryChunk* page,
                                     StoreBufferEvent event);

// Union used for customized checking of the IEEE double types
// inlined within v8 runtime, rather than going to the underlying
// platform headers and libraries
union IeeeDoubleLittleEndianArchType {
  double d;
  struct {
    unsigned int man_low : 32;
    unsigned int man_high : 20;
    unsigned int exp : 11;
    unsigned int sign : 1;
  } bits;
};

union IeeeDoubleBigEndianArchType {
  double d;
  struct {
    unsigned int sign : 1;
    unsigned int exp : 11;
    unsigned int man_high : 20;
    unsigned int man_low : 32;
  } bits;
};

#if V8_TARGET_LITTLE_ENDIAN
using IeeeDoubleArchType = IeeeDoubleLittleEndianArchType;
constexpr int kIeeeDoubleMantissaWordOffset = 0;
constexpr int kIeeeDoubleExponentWordOffset = 4;
#else
using IeeeDoubleArchType = IeeeDoubleBigEndianArchType;
constexpr int kIeeeDoubleMantissaWordOffset = 4;
constexpr int kIeeeDoubleExponentWordOffset = 0;
#endif

// -----------------------------------------------------------------------------
// Macros

// Testers for test.

#define HAS_SMI_TAG(value) \
  ((static_cast<i::Tagged_t>(value) & ::i::kSmiTagMask) == ::i::kSmiTag)

#define HAS_STRONG_HEAP_OBJECT_TAG(value)                          \
  (((static_cast<i::Tagged_t>(value) & ::i::kHeapObjectTagMask) == \
    ::i::kHeapObjectTag))

#define HAS_WEAK_HEAP_OBJECT_TAG(value)                            \
  (((static_cast<i::Tagged_t>(value) & ::i::kHeapObjectTagMask) == \
    ::i::kWeakHeapObjectTag))

// OBJECT_POINTER_ALIGN returns the value aligned as a HeapObject pointer
#define OBJECT_POINTER_ALIGN(value) \
  (((value) + ::i::kObjectAlignmentMask) & ~::i::kObjectAlignmentMask)

// OBJECT_POINTER_PADDING returns the padding size required to align value
// as a HeapObject pointer
#define OBJECT_POINTER_PADDING(value) (OBJECT_POINTER_ALIGN(value) - (value))

// POINTER_SIZE_ALIGN returns the value aligned as a system pointer.
#define POINTER_SIZE_ALIGN(value) \
  (((value) + ::i::kPointerAlignmentMask) & ~::i::kPointerAlignmentMask)

// POINTER_SIZE_PADDING returns the padding size required to align value
// as a system pointer.
#define POINTER_SIZE_PADDING(value) (POINTER_SIZE_ALIGN(value) - (value))

// CODE_POINTER_ALIGN returns the value aligned as a generated code segment.
#define CODE_POINTER_ALIGN(value) \
  (((value) + ::i::kCodeAlignmentMask) & ~::i::kCodeAlignmentMask)

// CODE_POINTER_PADDING returns the padding size required to align value
// as a generated code segment.
#define CODE_POINTER_PADDING(value) (CODE_POINTER_ALIGN(value) - (value))

// DOUBLE_POINTER_ALIGN returns the value algined for double pointers.
#define DOUBLE_POINTER_ALIGN(value) \
  (((value) + ::i::kDoubleAlignmentMask) & ~::i::kDoubleAlignmentMask)

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
  CLASS_SCOPE,     // The scope introduced by a class.
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
    case ScopeType::CLASS_SCOPE:
      return os << "CLASS_SCOPE";
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

enum class AllocationSiteUpdateMode { kUpdate, kCheckOnly };

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
constexpr uint64_t kMaxSafeIntegerUint64 = 9007199254740991;  // 2^53-1
constexpr double kMaxSafeInteger = static_cast<double>(kMaxSafeIntegerUint64);

constexpr double kMaxUInt32Double = double{kMaxUInt32};

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

  kDynamicLocal,  // requires dynamic lookup, but we know that the
                  // variable is local and where it is unless it
                  // has been shadowed by an eval-introduced
                  // variable

  // Variables for private methods or accessors whose access require
  // brand check. Declared only in class scopes by the compiler
  // and allocated only in class contexts:
  kPrivateMethod,  // Does not coexist with any other variable with the same
                   // name in the same scope.

  kPrivateSetterOnly,  // Incompatible with variables with the same name but
                       // any mode other than kPrivateGetterOnly. Transition to
                       // kPrivateGetterAndSetter if a later declaration for the
                       // same name with kPrivateGetterOnly is made.

  kPrivateGetterOnly,  // Incompatible with variables with the same name but
                       // any mode other than kPrivateSetterOnly. Transition to
                       // kPrivateGetterAndSetter if a later declaration for the
                       // same name with kPrivateSetterOnly is made.

  kPrivateGetterAndSetter,  // Does not coexist with any other variable with the
                            // same name in the same scope.

  kLastLexicalVariableMode = kConst,
};

// Printing support
#ifdef DEBUG
inline const char* VariableMode2String(VariableMode mode) {
  switch (mode) {
    case VariableMode::kVar:
      return "VAR";
    case VariableMode::kLet:
      return "LET";
    case VariableMode::kPrivateGetterOnly:
      return "PRIVATE_GETTER_ONLY";
    case VariableMode::kPrivateSetterOnly:
      return "PRIVATE_SETTER_ONLY";
    case VariableMode::kPrivateMethod:
      return "PRIVATE_METHOD";
    case VariableMode::kPrivateGetterAndSetter:
      return "PRIVATE_GETTER_AND_SETTER";
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
  PARAMETER_VARIABLE,
  THIS_VARIABLE,
  SLOPPY_BLOCK_FUNCTION_VARIABLE,
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

inline bool IsPrivateMethodOrAccessorVariableMode(VariableMode mode) {
  return mode >= VariableMode::kPrivateMethod &&
         mode <= VariableMode::kPrivateGetterAndSetter;
}

inline bool IsSerializableVariableMode(VariableMode mode) {
  return IsDeclaredVariableMode(mode) ||
         IsPrivateMethodOrAccessorVariableMode(mode);
}

inline bool IsConstVariableMode(VariableMode mode) {
  return mode == VariableMode::kConst ||
         IsPrivateMethodOrAccessorVariableMode(mode);
}

inline bool IsLexicalVariableMode(VariableMode mode) {
  STATIC_ASSERT(static_cast<uint8_t>(VariableMode::kLet) ==
                0);  // Implies that mode >= VariableMode::kLet.
  return mode <= VariableMode::kLastLexicalVariableMode;
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

  // An indexed slot in a script context. index() is the variable
  // index in the context object on the heap, starting at 0.
  // Important: REPL_GLOBAL variables from different scripts with the
  //            same name share a single script context slot. Every
  //            script context will reserve a slot, but only one will be used.
  // REPL_GLOBAL variables are stored in script contexts, but accessed like
  // globals, i.e. they always require a lookup at runtime to find the right
  // script context.
  REPL_GLOBAL,

  kLastVariableLocation = REPL_GLOBAL
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

// Static variables can only be used with the class in the closest
// class scope as receivers.
enum class IsStaticFlag : uint8_t { kNotStatic, kStatic };

enum MaybeAssignedFlag : uint8_t { kNotAssigned, kMaybeAssigned };

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
  return static_cast<uint32_t>(address >> kTaggedSizeLog2);
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
//   kSignedSmall -> kNumber             -> kNumberOrOddball           -> kAny
//                   kReceiver           -> kReceiverOrNullOrUndefined -> kAny
//                   kInternalizedString -> kString                    -> kAny
//                                          kSymbol                    -> kAny
//                                          kBigInt                    -> kAny
//
// This is distinct from BinaryOperationFeedback on purpose, because the
// feedback that matters differs greatly as well as the way it is consumed.
class CompareOperationFeedback {
 public:
  enum {
    kNone = 0x000,
    kSignedSmall = 0x001,
    kNumber = 0x003,
    kNumberOrOddball = 0x007,
    kInternalizedString = 0x008,
    kString = 0x018,
    kSymbol = 0x020,
    kBigInt = 0x040,
    kReceiver = 0x080,
    kReceiverOrNullOrUndefined = 0x180,
    kAny = 0x1ff
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
using DataPropertyInLiteralFlags = base::Flags<DataPropertyInLiteralFlag>;
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
  V(TrapUnalignedAccess)           \
  V(TrapDivByZero)                 \
  V(TrapDivUnrepresentable)        \
  V(TrapRemByZero)                 \
  V(TrapFloatUnrepresentable)      \
  V(TrapFuncInvalid)               \
  V(TrapFuncSigMismatch)           \
  V(TrapDataSegmentDropped)        \
  V(TrapElemSegmentDropped)        \
  V(TrapTableOutOfBounds)

enum KeyedAccessLoadMode {
  STANDARD_LOAD,
  LOAD_IGNORE_OUT_OF_BOUNDS,
};

enum KeyedAccessStoreMode {
  STANDARD_STORE,
  STORE_AND_GROW_HANDLE_COW,
  STORE_IGNORE_OUT_OF_BOUNDS,
  STORE_HANDLE_COW
};

enum MutableMode { MUTABLE, IMMUTABLE };

inline bool IsCOWHandlingStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode == STORE_HANDLE_COW ||
         store_mode == STORE_AND_GROW_HANDLE_COW;
}

inline bool IsGrowStoreMode(KeyedAccessStoreMode store_mode) {
  return store_mode == STORE_AND_GROW_HANDLE_COW;
}

enum IcCheckType { ELEMENT, PROPERTY };

// Helper stubs can be called in different ways depending on where the target
// code is located and how the call sequence is expected to look like:
//  - CodeObject: Call on-heap {Code} object via {RelocInfo::CODE_TARGET}.
//  - WasmRuntimeStub: Call native {WasmCode} stub via
//    {RelocInfo::WASM_STUB_CALL}.
//  - BuiltinPointer: Call a builtin based on a builtin pointer with dynamic
//    contents. If builtins are embedded, we call directly into off-heap code
//    without going through the on-heap Code trampoline.
enum class StubCallMode {
  kCallCodeObject,
  kCallWasmRuntimeStub,
  kCallBuiltinPointer,
};

constexpr int kFunctionLiteralIdInvalid = -1;
constexpr int kFunctionLiteralIdTopLevel = 0;

constexpr int kSmallOrderedHashSetMinCapacity = 4;
constexpr int kSmallOrderedHashMapMinCapacity = 4;

// Opaque data type for identifying stack frames. Used extensively
// by the debugger.
// ID_MIN_VALUE and ID_MAX_VALUE are specified to ensure that enumeration type
// has correct value range (see Issue 830 for more details).
enum StackFrameId { ID_MIN_VALUE = kMinInt, ID_MAX_VALUE = kMaxInt, NO_ID = 0 };

enum class ExceptionStatus : bool { kException = false, kSuccess = true };
V8_INLINE bool operator!(ExceptionStatus status) {
  return !static_cast<bool>(status);
}

enum class TraceRetainingPathMode { kEnabled, kDisabled };

// Used in the ScopeInfo flags fields for the function name variable for named
// function expressions, and for the receiver. Must be declared here so that it
// can be used in Torque.
enum class VariableAllocationInfo { NONE, STACK, CONTEXT, UNUSED };

}  // namespace internal
}  // namespace v8

namespace i = v8::internal;

#endif  // V8_COMMON_GLOBALS_H_
