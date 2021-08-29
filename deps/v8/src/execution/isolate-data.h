// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_DATA_H_
#define V8_EXECUTION_ISOLATE_DATA_H_

#include "src/builtins/builtins.h"
#include "src/codegen/constants-arch.h"
#include "src/codegen/external-reference-table.h"
#include "src/execution/external-pointer-table.h"
#include "src/execution/stack-guard.h"
#include "src/execution/thread-local-top.h"
#include "src/roots/roots.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class Isolate;

// This class contains a collection of data accessible from both C++ runtime
// and compiled code (including assembly stubs, builtins, interpreter bytecode
// handlers and optimized code).
// In particular, it contains pointer to the V8 heap roots table, external
// reference table and builtins array.
// The compiled code accesses the isolate data fields indirectly via the root
// register.
class IsolateData final {
 public:
  IsolateData(Isolate* isolate, Address cage_base)
      : cage_base_(cage_base), stack_guard_(isolate) {}

  IsolateData(const IsolateData&) = delete;
  IsolateData& operator=(const IsolateData&) = delete;

  static constexpr intptr_t kIsolateRootBias = kRootRegisterBias;

  // The value of kPointerCageBaseRegister
  Address cage_base() const {
    return COMPRESS_POINTERS_BOOL ? cage_base_ : kNullAddress;
  }

  // The value of the kRootRegister.
  Address isolate_root() const {
    return reinterpret_cast<Address>(this) + kIsolateRootBias;
  }

  // Root-register-relative offset of the roots table.
  static constexpr int roots_table_offset() {
    return kRootsTableOffset - kIsolateRootBias;
  }

  // Root-register-relative offset of the given root table entry.
  static constexpr int root_slot_offset(RootIndex root_index) {
    return roots_table_offset() + RootsTable::offset_of(root_index);
  }

  // Root-register-relative offset of the external reference table.
  static constexpr int external_reference_table_offset() {
    return kExternalReferenceTableOffset - kIsolateRootBias;
  }

  // Root-register-relative offset of the builtin entry table.
  static constexpr int builtin_entry_table_offset() {
    return kBuiltinEntryTableOffset - kIsolateRootBias;
  }
  static constexpr int builtin_entry_slot_offset(Builtin builtin) {
    DCHECK(Builtins::IsBuiltinId(builtin));
    return builtin_entry_table_offset() +
           static_cast<int>(builtin) * kSystemPointerSize;
  }

  // Root-register-relative offset of the builtins table.
  static constexpr int builtins_table_offset() {
    return kBuiltinsTableOffset - kIsolateRootBias;
  }

  // Root-register-relative offset of the external pointer table.
#ifdef V8_HEAP_SANDBOX
  static constexpr int external_pointer_table_offset() {
    return kExternalPointerTableOffset - kIsolateRootBias;
  }
#endif

  static constexpr int fast_c_call_caller_fp_offset() {
    return kFastCCallCallerFPOffset - kIsolateRootBias;
  }

  static constexpr int fast_c_call_caller_pc_offset() {
    return kFastCCallCallerPCOffset - kIsolateRootBias;
  }

  static constexpr int fast_api_call_target_offset() {
    return kFastApiCallTargetOffset - kIsolateRootBias;
  }

  static constexpr int cage_base_offset() {
    return kCageBaseOffset - kIsolateRootBias;
  }

  // Root-register-relative offset of the given builtin table entry.
  // TODO(ishell): remove in favour of typified id version.
  static int builtin_slot_offset(int builtin_index) {
    DCHECK(Builtins::IsBuiltinId(builtin_index));
    return builtins_table_offset() + builtin_index * kSystemPointerSize;
  }

  // Root-register-relative offset of the builtin table entry.
  static int builtin_slot_offset(Builtin id) {
    return builtins_table_offset() + static_cast<int>(id) * kSystemPointerSize;
  }

  // The FP and PC that are saved right before TurboAssembler::CallCFunction.
  Address* fast_c_call_caller_fp_address() { return &fast_c_call_caller_fp_; }
  Address* fast_c_call_caller_pc_address() { return &fast_c_call_caller_pc_; }
  // The address of the fast API callback right before it's executed from
  // generated code.
  Address* fast_api_call_target_address() { return &fast_api_call_target_; }
  StackGuard* stack_guard() { return &stack_guard_; }
  uint8_t* stack_is_iterable_address() { return &stack_is_iterable_; }
  Address fast_c_call_caller_fp() { return fast_c_call_caller_fp_; }
  Address fast_c_call_caller_pc() { return fast_c_call_caller_pc_; }
  Address fast_api_call_target() { return fast_api_call_target_; }
  uint8_t stack_is_iterable() { return stack_is_iterable_; }

  // Returns true if this address points to data stored in this instance.
  // If it's the case then the value can be accessed indirectly through the
  // root register.
  bool contains(Address address) const {
    STATIC_ASSERT(std::is_unsigned<Address>::value);
    Address start = reinterpret_cast<Address>(this);
    return (address - start) < sizeof(*this);
  }

  ThreadLocalTop& thread_local_top() { return thread_local_top_; }
  ThreadLocalTop const& thread_local_top() const { return thread_local_top_; }

  RootsTable& roots() { return roots_; }
  const RootsTable& roots() const { return roots_; }

  ExternalReferenceTable* external_reference_table() {
    return &external_reference_table_;
  }

  Address* builtin_entry_table() { return builtin_entry_table_; }
  Address* builtins() { return builtins_; }

 private:
  // Static layout definition.
  //
  // Note: The location of fields within IsolateData is significant. The
  // closer they are to the value of kRootRegister (i.e.: isolate_root()), the
  // cheaper it is to access them. See also: https://crbug.com/993264.
  // The recommend guideline is to put frequently-accessed fields close to the
  // beginning of IsolateData.
#define FIELDS(V)                                                             \
  V(kEmbedderDataOffset, Internals::kNumIsolateDataSlots* kSystemPointerSize) \
  V(kFastCCallCallerFPOffset, kSystemPointerSize)                             \
  V(kFastCCallCallerPCOffset, kSystemPointerSize)                             \
  V(kFastApiCallTargetOffset, kSystemPointerSize)                             \
  V(kCageBaseOffset, kSystemPointerSize)                                      \
  V(kLongTaskStatsCounterOffset, kSizetSize)                                  \
  V(kStackGuardOffset, StackGuard::kSizeInBytes)                              \
  V(kRootsTableOffset, RootsTable::kEntriesCount* kSystemPointerSize)         \
  V(kExternalReferenceTableOffset, ExternalReferenceTable::kSizeInBytes)      \
  V(kThreadLocalTopOffset, ThreadLocalTop::kSizeInBytes)                      \
  V(kBuiltinEntryTableOffset, Builtins::kBuiltinCount* kSystemPointerSize)    \
  V(kBuiltinsTableOffset, Builtins::kBuiltinCount* kSystemPointerSize)        \
  FIELDS_HEAP_SANDBOX(V)                                                      \
  V(kStackIsIterableOffset, kUInt8Size)                                       \
  /* This padding aligns IsolateData size by 8 bytes. */                      \
  V(kPaddingOffset,                                                           \
    8 + RoundUp<8>(static_cast<int>(kPaddingOffset)) - kPaddingOffset)        \
  /* Total size. */                                                           \
  V(kSize, 0)

#ifdef V8_HEAP_SANDBOX
#define FIELDS_HEAP_SANDBOX(V) \
  V(kExternalPointerTableOffset, kSystemPointerSize * 3)
#else
#define FIELDS_HEAP_SANDBOX(V)
#endif  // V8_HEAP_SANDBOX

  DEFINE_FIELD_OFFSET_CONSTANTS(0, FIELDS)
#undef FIELDS

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8-internal.h) constants.
  // The layout consitency is verified in Isolate::CheckIsolateLayout() using
  // runtime checks.
  void* embedder_data_[Internals::kNumIsolateDataSlots] = {};

  // Stores the state of the caller for TurboAssembler::CallCFunction so that
  // the sampling CPU profiler can iterate the stack during such calls. These
  // are stored on IsolateData so that they can be stored to with only one move
  // instruction in compiled code.
  Address fast_c_call_caller_fp_ = kNullAddress;
  Address fast_c_call_caller_pc_ = kNullAddress;
  Address fast_api_call_target_ = kNullAddress;

  Address cage_base_ = kNullAddress;

  // Used for implementation of LongTaskStats. Counts the number of potential
  // long tasks.
  size_t long_task_stats_counter_ = 0;

  // Fields related to the system and JS stack. In particular, this contains
  // the stack limit used by stack checks in generated code.
  StackGuard stack_guard_;

  RootsTable roots_;

  ExternalReferenceTable external_reference_table_;

  ThreadLocalTop thread_local_top_;

  // The entry points for all builtins. This corresponds to
  // Code::InstructionStart() for each Code object in the builtins table below.
  // The entry table is in IsolateData for easy access through kRootRegister.
  Address builtin_entry_table_[Builtins::kBuiltinCount] = {};

  // The entries in this array are tagged pointers to Code objects.
  Address builtins_[Builtins::kBuiltinCount] = {};

  // Table containing pointers to external objects.
#ifdef V8_HEAP_SANDBOX
  ExternalPointerTable external_pointer_table_;
#endif

  // Whether the SafeStackFrameIterator can successfully iterate the current
  // stack. Only valid values are 0 or 1.
  uint8_t stack_is_iterable_ = 1;

  // Ensure the size is 8-byte aligned in order to make alignment of the field
  // following the IsolateData field predictable. This solves the issue with
  // C++ compilers for 32-bit platforms which are not consistent at aligning
  // int64_t fields.
  // In order to avoid dealing with zero-size arrays the padding size is always
  // in the range [8, 15).
  STATIC_ASSERT(kPaddingOffsetEnd + 1 - kPaddingOffset >= 8);
  char padding_[kPaddingOffsetEnd + 1 - kPaddingOffset];

  V8_INLINE static void AssertPredictableLayout();

  friend class Isolate;
  friend class Heap;
  FRIEND_TEST(HeapTest, ExternalLimitDefault);
  FRIEND_TEST(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling);
};

// IsolateData object must have "predictable" layout which does not change when
// cross-compiling to another platform. Otherwise there may be compatibility
// issues because of different compilers used for snapshot generator and
// actual V8 code.
void IsolateData::AssertPredictableLayout() {
  STATIC_ASSERT(std::is_standard_layout<RootsTable>::value);
  STATIC_ASSERT(std::is_standard_layout<ThreadLocalTop>::value);
  STATIC_ASSERT(std::is_standard_layout<ExternalReferenceTable>::value);
  STATIC_ASSERT(std::is_standard_layout<IsolateData>::value);
  STATIC_ASSERT(offsetof(IsolateData, roots_) == kRootsTableOffset);
  STATIC_ASSERT(offsetof(IsolateData, external_reference_table_) ==
                kExternalReferenceTableOffset);
  STATIC_ASSERT(offsetof(IsolateData, thread_local_top_) ==
                kThreadLocalTopOffset);
  STATIC_ASSERT(offsetof(IsolateData, builtins_) == kBuiltinsTableOffset);
  STATIC_ASSERT(offsetof(IsolateData, fast_c_call_caller_fp_) ==
                kFastCCallCallerFPOffset);
  STATIC_ASSERT(offsetof(IsolateData, fast_c_call_caller_pc_) ==
                kFastCCallCallerPCOffset);
  STATIC_ASSERT(offsetof(IsolateData, fast_api_call_target_) ==
                kFastApiCallTargetOffset);
  STATIC_ASSERT(offsetof(IsolateData, cage_base_) == kCageBaseOffset);
  STATIC_ASSERT(offsetof(IsolateData, long_task_stats_counter_) ==
                kLongTaskStatsCounterOffset);
  STATIC_ASSERT(offsetof(IsolateData, stack_guard_) == kStackGuardOffset);
#ifdef V8_HEAP_SANDBOX
  STATIC_ASSERT(offsetof(IsolateData, external_pointer_table_) ==
                kExternalPointerTableOffset);
#endif
  STATIC_ASSERT(offsetof(IsolateData, stack_is_iterable_) ==
                kStackIsIterableOffset);
  STATIC_ASSERT(sizeof(IsolateData) == IsolateData::kSize);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_DATA_H_
