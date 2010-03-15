// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_GLOBALS_H_
#define V8_GLOBALS_H_

namespace v8 {
namespace internal {

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define V8_HOST_ARCH_X64 1
#define V8_HOST_ARCH_64_BIT 1
#define V8_HOST_CAN_READ_UNALIGNED 1
#elif defined(_M_IX86) || defined(__i386__)
#define V8_HOST_ARCH_IA32 1
#define V8_HOST_ARCH_32_BIT 1
#define V8_HOST_CAN_READ_UNALIGNED 1
#elif defined(__ARMEL__)
#define V8_HOST_ARCH_ARM 1
#define V8_HOST_ARCH_32_BIT 1
#elif defined(_MIPS_ARCH_MIPS32R2)
#define V8_HOST_ARCH_MIPS 1
#define V8_HOST_ARCH_32_BIT 1
#else
#error Your host architecture was not detected as supported by v8
#endif

#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32)
#define V8_TARGET_CAN_READ_UNALIGNED 1
#elif V8_TARGET_ARCH_ARM
#elif V8_TARGET_ARCH_MIPS
#else
#error Your target architecture is not supported by v8
#endif

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

// Define our own macros for writing 64-bit constants.  This is less fragile
// than defining __STDC_CONSTANT_MACROS before including <stdint.h>, and it
// works on compilers that don't have it (like MSVC).
#if V8_HOST_ARCH_64_BIT
#ifdef _MSC_VER
#define V8_UINT64_C(x)  (x ## UI64)
#define V8_INT64_C(x)   (x ## I64)
#define V8_PTR_PREFIX "ll"
#else  // _MSC_VER
#define V8_UINT64_C(x)  (x ## UL)
#define V8_INT64_C(x)   (x ## L)
#define V8_PTR_PREFIX "l"
#endif  // _MSC_VER
#else  // V8_HOST_ARCH_64_BIT
#define V8_PTR_PREFIX ""
#endif  // V8_HOST_ARCH_64_BIT

// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write V8_2PART_UINT64_C(0x12345678,90123456);
#define V8_2PART_UINT64_C(a, b) (((static_cast<uint64_t>(a) << 32) + 0x##b##u))

#define V8PRIxPTR V8_PTR_PREFIX "x"
#define V8PRIdPTR V8_PTR_PREFIX "d"

// Fix for Mac OS X defining uintptr_t as "unsigned long":
#if defined(__APPLE__) && defined(__MACH__)
#undef V8PRIxPTR
#define V8PRIxPTR "lx"
#endif

#if defined(__APPLE__) && defined(__MACH__)
#define USING_MAC_ABI
#endif

// Code-point values in Unicode 4.0 are 21 bits wide.
typedef uint16_t uc16;
typedef int32_t uc32;

// -----------------------------------------------------------------------------
// Constants

const int KB = 1024;
const int MB = KB * KB;
const int GB = KB * KB * KB;
const int kMaxInt = 0x7FFFFFFF;
const int kMinInt = -kMaxInt - 1;

const uint32_t kMaxUInt32 = 0xFFFFFFFFu;

const int kCharSize     = sizeof(char);      // NOLINT
const int kShortSize    = sizeof(short);     // NOLINT
const int kIntSize      = sizeof(int);       // NOLINT
const int kDoubleSize   = sizeof(double);    // NOLINT
const int kPointerSize  = sizeof(void*);     // NOLINT
const int kIntptrSize   = sizeof(intptr_t);  // NOLINT

#if V8_HOST_ARCH_64_BIT
const int kPointerSizeLog2 = 3;
const intptr_t kIntptrSignBit = V8_INT64_C(0x8000000000000000);
#else
const int kPointerSizeLog2 = 2;
const intptr_t kIntptrSignBit = 0x80000000;
#endif

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

// Tag information for Failure.
const int kFailureTag = 3;
const int kFailureTagSize = 2;
const intptr_t kFailureTagMask = (1 << kFailureTagSize) - 1;


const int kBitsPerByte = 8;
const int kBitsPerByteLog2 = 3;
const int kBitsPerPointer = kPointerSize * kBitsPerByte;
const int kBitsPerInt = kIntSize * kBitsPerByte;


// Zap-value: The value used for zapping dead objects.
// Should be a recognizable hex value tagged as a heap object pointer.
#ifdef V8_HOST_ARCH_64_BIT
const Address kZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0xdeadbeedbeadbeed));
const Address kHandleZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1baddead0baddead));
const Address kFromSpaceZapValue =
    reinterpret_cast<Address>(V8_UINT64_C(0x1beefdad0beefdad));
#else
const Address kZapValue = reinterpret_cast<Address>(0xdeadbeed);
const Address kHandleZapValue = reinterpret_cast<Address>(0xbaddead);
const Address kFromSpaceZapValue = reinterpret_cast<Address>(0xbeefdad);
#endif


// Number of bits to represent the page size for paged spaces. The value of 13
// gives 8K bytes per page.
const int kPageSizeBits = 13;


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
class FreeStoreAllocationPolicy;
template <typename T> class Handle;
class Heap;
class HeapObject;
class IC;
class InterceptorInfo;
class IterationStatement;
class Array;
class JSArray;
class JSFunction;
class JSObject;
class LargeObjectSpace;
template <typename T, class P = FreeStoreAllocationPolicy> class List;
class LookupResult;
class MacroAssembler;
class Map;
class MapSpace;
class MarkCompactCollector;
class NewSpace;
class NodeVisitor;
class Object;
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
class ZoneScopeInfo;

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


enum InLoopFlag {
  NOT_IN_LOOP,
  IN_LOOP
};


enum CallFunctionFlags {
  NO_CALL_FUNCTION_FLAGS = 0,
  RECEIVER_MIGHT_BE_VALUE = 1 << 0  // Receiver might not be a JSObject.
};


// Type of properties.
// Order of properties is significant.
// Must fit in the BitField PropertyDetails::TypeField.
// A copy of this is in mirror-delay.js.
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
  FIRST_PHANTOM_PROPERTY_TYPE = MAP_TRANSITION
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


// AccessorCallback
struct AccessorDescriptor {
  Object* (*getter)(Object* object, void* data);
  Object* (*setter)(JSObject* object, Object* value, void* data);
  void* data;
};


// Logging and profiling.
// A StateTag represents a possible state of the VM.  When compiled with
// ENABLE_LOGGING_AND_PROFILING, the logger maintains a stack of these.
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

// OBJECT_SIZE_ALIGN returns the value aligned HeapObject size
#define OBJECT_SIZE_ALIGN(value)                                \
  (((value) + kObjectAlignmentMask) & ~kObjectAlignmentMask)

// POINTER_SIZE_ALIGN returns the value aligned as a pointer.
#define POINTER_SIZE_ALIGN(value)                               \
  (((value) + kPointerAlignmentMask) & ~kPointerAlignmentMask)

// MAP_SIZE_ALIGN returns the value aligned as a map pointer.
#define MAP_SIZE_ALIGN(value)                               \
  (((value) + kMapAlignmentMask) & ~kMapAlignmentMask)

// The expression OFFSET_OF(type, field) computes the byte-offset
// of the specified field relative to the containing type. This
// corresponds to 'offsetof' (in stddef.h), except that it doesn't
// use 0 or NULL, which causes a problem with the compiler warnings
// we have enabled (which is also why 'offsetof' doesn't seem to work).
// Here we simply use the non-zero value 4, which seems to work.
#define OFFSET_OF(type, field)                                          \
  (reinterpret_cast<intptr_t>(&(reinterpret_cast<type*>(4)->field)) - 4)


// The expression ARRAY_SIZE(a) is a compile-time constant of type
// size_t which represents the number of elements of the given
// array. You should only use ARRAY_SIZE on statically allocated
// arrays.
#define ARRAY_SIZE(a)                                   \
  ((sizeof(a) / sizeof(*(a))) /                         \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))


// The USE(x) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
template <typename T>
static inline void USE(T) { }


// FUNCTION_ADDR(f) gets the address of a C function f.
#define FUNCTION_ADDR(f)                                        \
  (reinterpret_cast<v8::internal::Address>(reinterpret_cast<intptr_t>(f)))


// FUNCTION_CAST<F>(addr) casts an address into a function
// of type F. Used to invoke generated code from within C.
template <typename F>
F FUNCTION_CAST(Address addr) {
  return reinterpret_cast<F>(reinterpret_cast<intptr_t>(addr));
}


// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName)      \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)


// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_COPY_AND_ASSIGN(TypeName)


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

// define used for helping GCC to make better inlining. Don't bother for debug
// builds. On GCC 3.4.5 using __attribute__((always_inline)) causes compilation
// errors in debug build.
#if defined(__GNUC__) && !defined(DEBUG)
#if (__GNUC__ >= 4)
#define INLINE(header) inline header  __attribute__((always_inline))
#else
#define INLINE(header) inline __attribute__((always_inline)) header
#endif
#else
#define INLINE(header) inline header
#endif

// Feature flags bit positions. They are mostly based on the CPUID spec.
// (We assign CPUID itself to one of the currently reserved bits --
// feel free to change this if needed.)
enum CpuFeature { SSE3 = 32,   // x86
                  SSE2 = 26,   // x86
                  CMOV = 15,   // x86
                  RDTSC = 4,   // x86
                  CPUID = 10,  // x86
                  VFP3 = 1,    // ARM
                  ARMv7 = 2,   // ARM
                  SAHF = 0};   // x86

} }  // namespace v8::internal

#endif  // V8_GLOBALS_H_
