// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_DATA_H_
#define V8_EXECUTION_ISOLATE_DATA_H_

#include "src/builtins/builtins.h"
#include "src/codegen/constants-arch.h"
#include "src/codegen/external-reference-table.h"
#include "src/execution/isolate-data-fields.h"
#include "src/execution/stack-guard.h"
#include "src/execution/thread-local-top.h"
#include "src/heap/linear-allocation-area.h"
#include "src/init/isolate-group.h"
#include "src/roots/roots.h"
#include "src/sandbox/code-pointer-table.h"
#include "src/sandbox/cppheap-pointer-table.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/sandbox/trusted-pointer-table.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class Isolate;
class TrustedPointerPublishingScope;

namespace wasm {
class StackMemory;
}

#define BUILTINS_WITH_DISPATCH_ADAPTER(V, CamelName, underscore_name, ...) \
  V(CamelName, CamelName##SharedFun)

#define BUILTINS_WITH_DISPATCH_LIST(V) \
  BUILTINS_WITH_SFI_LIST_GENERATOR(BUILTINS_WITH_DISPATCH_ADAPTER, V)

struct JSBuiltinDispatchHandleRoot {
  enum Idx {
#define CASE(builtin_name, ...) k##builtin_name,
    BUILTINS_WITH_DISPATCH_LIST(CASE)

        kCount,
    kFirst = 0
#undef CASE
  };
  static constexpr size_t kPadding = Idx::kCount * sizeof(JSDispatchHandle) %
                                     kSystemPointerSize /
                                     sizeof(JSDispatchHandle);
  static constexpr size_t kTableSize = Idx::kCount + kPadding;

  static inline Builtin to_builtin(Idx idx) {
#define CASE(builtin_name, ...) Builtin::k##builtin_name,
    return std::array<Builtin, Idx::kCount>{
        BUILTINS_WITH_DISPATCH_LIST(CASE)}[idx];
#undef CASE
  }
  static inline Idx to_idx(Builtin builtin) {
    switch (builtin) {
#define CASE(builtin_name, ...)  \
  case Builtin::k##builtin_name: \
    return Idx::k##builtin_name;
      BUILTINS_WITH_DISPATCH_LIST(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
  }

  static inline Idx to_idx(RootIndex root_idx) {
    switch (root_idx) {
#define CASE(builtin_name, shared_fun_name, ...) \
  case RootIndex::k##shared_fun_name:            \
    return Idx::k##builtin_name;
      BUILTINS_WITH_DISPATCH_LIST(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
  }
};

// This class contains a collection of data accessible from both C++ runtime
// and compiled code (including builtins, interpreter bytecode handlers and
// optimized code). The compiled code accesses the isolate data fields
// indirectly via the root register.
class IsolateData final {
 public:
  IsolateData(Isolate* isolate, IsolateGroup* group)
      :
#ifdef V8_COMPRESS_POINTERS
        cage_base_(group->GetPtrComprCageBase()),
#endif
        stack_guard_(isolate)
#ifdef V8_ENABLE_SANDBOX
        ,
        trusted_cage_base_(group->GetTrustedPtrComprCageBase()),
        code_pointer_table_base_address_(
            group->code_pointer_table()->base_address())
#endif
        ,
        js_dispatch_table_base_(group->js_dispatch_table()->base_address()) {
  }

  IsolateData(const IsolateData&) = delete;
  IsolateData& operator=(const IsolateData&) = delete;

  static constexpr intptr_t kIsolateRootBias = kRootRegisterBias;

  // The value of the kRootRegister.
  Address isolate_root() const {
    return reinterpret_cast<Address>(this) + kIsolateRootBias;
  }

  // Root-register-relative offsets.
  static constexpr int isolate_data_offset() { return -kIsolateRootBias; }
  Address isolate_data_address() const {
    return reinterpret_cast<Address>(this);
  }

#define V(CamelName, Size, hacker_name)                \
  static constexpr int hacker_name##_offset() {        \
    return k##CamelName##Offset - kIsolateRootBias;    \
  }                                                    \
  Address hacker_name##_address() const {              \
    return reinterpret_cast<Address>(&hacker_name##_); \
  }
  ISOLATE_DATA_FIELDS(V)
#undef V

#define V(CamelName, hacker_name, holder_field_name, FieldOffset) \
  static constexpr int hacker_name##_offset() {                   \
    return holder_field_name##_offset() + FieldOffset;            \
  }                                                               \
  Address hacker_name##_address() const {                         \
    return holder_field_name##_address() + FieldOffset;           \
  }
  ISOLATE_DATA_SUBFIELDS(V)
#undef V

  static constexpr int root_slot_offset(RootIndex root_index) {
    return roots_table_offset() + RootsTable::offset_of(root_index);
  }

  static constexpr int BuiltinEntrySlotOffset(Builtin id) {
    DCHECK(Builtins::IsBuiltinId(id));
    return (Builtins::IsTier0(id) ? builtin_tier0_entry_table_offset()
                                  : builtin_entry_table_offset()) +
           Builtins::ToInt(id) * kSystemPointerSize;
  }
  // TODO(ishell): remove in favour of typified id version.
  static constexpr int builtin_slot_offset(int builtin_index) {
    return BuiltinSlotOffset(Builtins::FromInt(builtin_index));
  }
  static constexpr int BuiltinSlotOffset(Builtin id) {
    return (Builtins::IsTier0(id) ? builtin_tier0_table_offset()
                                  : builtin_table_offset()) +
           Builtins::ToInt(id) * kSystemPointerSize;
  }

  LinearAllocationArea& new_allocation_info() { return new_allocation_info_; }
  LinearAllocationArea& old_allocation_info() { return old_allocation_info_; }

  Address fast_c_call_caller_fp() const { return fast_c_call_caller_fp_; }
  Address fast_c_call_caller_pc() const { return fast_c_call_caller_pc_; }
  Address fast_api_call_target() const { return fast_api_call_target_; }

  // The value of kPointerCageBaseRegister.
  Address cage_base() const { return cage_base_; }
  StackGuard* stack_guard() { return &stack_guard_; }
  int32_t* regexp_static_result_offsets_vector() const {
    return regexp_static_result_offsets_vector_;
  }
  void set_regexp_static_result_offsets_vector(int32_t* value) {
    regexp_static_result_offsets_vector_ = value;
  }
  Address* builtin_tier0_entry_table() { return builtin_tier0_entry_table_; }
  Address* builtin_tier0_table() { return builtin_tier0_table_; }
  RootsTable& roots() { return roots_table_; }
  Address api_callback_thunk_argument() const {
    return api_callback_thunk_argument_;
  }
  Address regexp_exec_vector_argument() const {
    return regexp_exec_vector_argument_;
  }
  Tagged<Object> continuation_preserved_embedder_data() const {
    return continuation_preserved_embedder_data_;
  }
  void set_continuation_preserved_embedder_data(Tagged<Object> data) {
    continuation_preserved_embedder_data_ = data;
  }
  const RootsTable& roots() const { return roots_table_; }
  ExternalReferenceTable* external_reference_table() {
    return &external_reference_table_;
  }
  ThreadLocalTop& thread_local_top() { return thread_local_top_; }
  ThreadLocalTop const& thread_local_top() const { return thread_local_top_; }
  Address* builtin_entry_table() { return builtin_entry_table_; }
  Address* builtin_table() { return builtin_table_; }
#if V8_ENABLE_WEBASSEMBLY
  wasm::StackMemory* active_stack() { return active_stack_; }
  void set_active_stack(wasm::StackMemory* stack) { active_stack_ = stack; }
  Tagged<WasmSuspenderObject> active_suspender() { return active_suspender_; }
  void set_active_suspender(Tagged<WasmSuspenderObject> v) {
    active_suspender_ = v;
  }
#endif
#if !V8_STATIC_DISPATCH_HANDLES_BOOL
  JSDispatchHandle builtin_dispatch_handle(Builtin builtin) {
    return builtin_dispatch_table_[JSBuiltinDispatchHandleRoot::to_idx(
        builtin)];
  }
#endif  // !V8_STATIC_DISPATCH_HANDLES_BOOL

  // Accessors for storage of raw arguments for runtime functions.
  template <typename T>
    requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
  T GetRawArgument(uint32_t index) const {
    static_assert(sizeof(T) <= kDoubleSize);
    DCHECK_LT(index, GetRawArgumentCount());
    return *reinterpret_cast<const T*>(&raw_arguments_[index].storage_);
  }
  static constexpr uint32_t GetRawArgumentCount() {
    return arraysize(raw_arguments_);
  }

  bool stack_is_iterable() const {
    DCHECK(stack_is_iterable_ == 0 || stack_is_iterable_ == 1);
    return stack_is_iterable_ != 0;
  }
  bool is_marking() const { return is_marking_flag_; }

  // Returns true if this address points to data stored in this instance. If
  // it's the case then the value can be accessed indirectly through the root
  // register.
  bool contains(Address address) const {
    static_assert(std::is_unsigned_v<Address>);
    Address start = reinterpret_cast<Address>(this);
    return (address - start) < sizeof(*this);
  }

  static constexpr int32_t GetOffset(IsolateFieldId id) {
    switch (id) {
#define CASE(CamelName, size, name)  \
  case IsolateFieldId::k##CamelName: \
    return name##_offset();
      ISOLATE_DATA_FIELDS(CASE)
#undef CASE
#define CASE(CamelName, hacker_name, ...) \
  case IsolateFieldId::k##CamelName:      \
    return hacker_name##_offset();
      ISOLATE_DATA_SUBFIELDS(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
  }

  Address GetAddress(IsolateFieldId id) const {
    switch (id) {
#define CASE(CamelName, size, name)  \
  case IsolateFieldId::k##CamelName: \
    return name##_address();
      ISOLATE_DATA_FIELDS(CASE)
#undef CASE
#define CASE(CamelName, hacker_name, ...) \
  case IsolateFieldId::k##CamelName:      \
    return hacker_name##_address();
      ISOLATE_DATA_SUBFIELDS(CASE)
#undef CASE
      default:
        UNREACHABLE();
    }
  }

 private:
  // Static layout definition.
  //
  // Note: The location of fields within IsolateData is significant. The
  // closer they are to the value of kRootRegister (i.e.: isolate_root()), the
  // cheaper it is to access them. See also: https://crbug.com/993264.
  // The recommended guideline is to put frequently-accessed fields close to
  // the beginning of IsolateData.
#define FIELDS(V)                                        \
  ISOLATE_DATA_FIELDS(V)                                 \
  /* This padding aligns IsolateData size by 8 bytes. */ \
  PADDING_FIELD(8, V, TrailingPadding, trailing_padding) \
  /* Total size. */                                      \
  V(Size, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS_WITH_PURE_NAME(0, FIELDS)
#undef FIELDS

  const Address cage_base_ = kNullAddress;

  // Fields related to the system and JS stack. In particular, this contains
  // the stack limit used by stack checks in generated code.
  StackGuard stack_guard_;

  //
  // Hot flags that are regularly checked.
  //

  // These flags are regularly checked by write barriers.
  // Only valid values are 0 or 1.
  uint8_t is_marking_flag_ = false;
  uint8_t is_minor_marking_flag_ = false;
  uint8_t is_shared_space_isolate_flag_ = false;
  uint8_t uses_shared_heap_flag_ = false;

  // Storage for is_profiling and should_check_side_effects booleans.
  // This value is checked on every API callback/getter call.
  base::Flags<IsolateExecutionModeFlag, uint8_t, std::atomic<uint8_t>>
      execution_mode_ = {IsolateExecutionModeFlag::kNoFlags};
  static_assert(sizeof(execution_mode_) == 1);

  //
  // Not super hot flags, which are put here because we have to align the
  // builtin entry table to kSystemPointerSize anyway.
  //

  // Whether the StackFrameIteratorForProfiler can successfully iterate the
  // current stack. The only valid values are 0 or 1.
  uint8_t stack_is_iterable_ = 1;

  // Field to pass value for error throwing builtins. Currently, it is used to
  // pass the type of the `Dataview` operation to print out operation's name in
  // case of an error.
  uint8_t error_message_param_;

  // Whether we are using SetPrototypeProperties together with LazyClosures.
  uint8_t has_lazy_closures_ = 0;

  // Ensure the following tables are kSystemPointerSize-byte aligned.
  V8_NO_UNIQUE_ADDRESS uint8_t
      tables_alignment_padding_[kTablesAlignmentPaddingSize];

  // A pointer to the static offsets vector (used to pass results from the
  // irregexp engine to the rest of V8), or nullptr if the static offsets
  // vector is currently in use.
  int32_t* regexp_static_result_offsets_vector_ = nullptr;

  // Tier 0 tables. See also builtin_entry_table_ and builtin_table_.
  Address builtin_tier0_entry_table_[Builtins::kBuiltinTier0Count] = {};
  Address builtin_tier0_table_[Builtins::kBuiltinTier0Count] = {};

  LinearAllocationArea new_allocation_info_;
  LinearAllocationArea old_allocation_info_;

  Address last_young_allocation_;

  // Aligns fast_c_call_XXX fields so that they stay in the same CPU cache line.
  Address fast_c_call_alignment_padding_[kFastCCallAlignmentPaddingCount];

  // Stores the state of the caller for MacroAssembler::CallCFunction so that
  // the sampling CPU profiler can iterate the stack during such calls. These
  // are stored on IsolateData so that they can be stored to with only one move
  // instruction in compiled code.
  // Note that the PC field is right before FP. This is necessary for simulator
  // builds for ARM64. This ensures that the PC is written before the FP with
  // the stp instruction.
  struct {
    // The FP and PC that are saved right before MacroAssembler::CallCFunction.
    Address fast_c_call_caller_pc_ = kNullAddress;
    Address fast_c_call_caller_fp_ = kNullAddress;
  };
  // The address of the fast API callback right before it's executed from
  // generated code.
  Address fast_api_call_target_ = kNullAddress;

  // Used for implementation of LongTaskStats. Counts the number of potential
  // long tasks.
  size_t long_task_stats_counter_ = 0;

  ThreadLocalTop thread_local_top_;
  HandleScopeData handle_scope_data_;

  // These fields are accessed through the API, offsets must be kept in sync
  // with v8::internal::Internals (in include/v8-internal.h) constants. The
  // layout consistency is verified in Isolate::CheckIsolateLayout() using
  // runtime checks.
  void* embedder_data_[Internals::kNumIsolateDataSlots] = {};

  // Tables containing pointers to objects outside of the V8 sandbox.
#ifdef V8_COMPRESS_POINTERS
  ExternalPointerTable external_pointer_table_;
  ExternalPointerTable* shared_external_pointer_table_ = nullptr;
  CppHeapPointerTable cpp_heap_pointer_table_;
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_ENABLE_SANDBOX
  const Address trusted_cage_base_;

  TrustedPointerTable trusted_pointer_table_;
  TrustedPointerTable* shared_trusted_pointer_table_ = nullptr;
  TrustedPointerPublishingScope* trusted_pointer_publishing_scope_ = nullptr;

  const Address code_pointer_table_base_address_;
#endif  // V8_ENABLE_SANDBOX

  // This is a storage for an additional argument for the Api callback thunk
  // functions, see InvokeAccessorGetterCallback and InvokeFunctionCallback.
  Address api_callback_thunk_argument_ = kNullAddress;

  Address js_dispatch_table_base_ = kNullAddress;

  // Storage for an additional (untagged) argument for
  // Runtime::kRegExpExecInternal2, required since runtime functions only
  // accept tagged arguments.
  Address regexp_exec_vector_argument_ = kNullAddress;

  // This is data that should be preserved on newly created continuations.
  Tagged<Object> continuation_preserved_embedder_data_ = Smi::zero();

  RootsTable roots_table_;
  ExternalReferenceTable external_reference_table_;

  // The entry points for builtins. This corresponds to
  // InstructionStream::InstructionStart() for each InstructionStream object in
  // the builtins table below. The entry table is in IsolateData for easy access
  // through kRootRegister.
  Address builtin_entry_table_[Builtins::kBuiltinCount] = {};

  // The entries in this array are tagged pointers to Code objects.
  Address builtin_table_[Builtins::kBuiltinCount] = {};

#if V8_ENABLE_WEBASSEMBLY
  wasm::StackMemory* active_stack_ = nullptr;
  Tagged<WasmSuspenderObject> active_suspender_;
#endif

  // Stamp value which is increased on every
  // v8::Isolate::DateTimeConfigurationChangeNotification(..).
  int32_t date_cache_stamp_ = 0;
  // Boolean value indicating that DateCache is used (i.e. JSDate instances
  // were created in this Isolate).
  uint8_t is_date_cache_used_ = false;

  // Padding for aligning raw_arguments_.
  V8_NO_UNIQUE_ADDRESS uint8_t raw_arguments_padding_[kRawArgumentsPaddingSize];

  // Storage for raw values passed from CSA/Torque to runtime functions.
  struct RawArgument {
    uint8_t storage_[kDoubleSize];
  } raw_arguments_[2] = {};

  // Counts deopt points if deopt_every_n_times is enabled.
  uint64_t stress_deopt_count_ = 0;

#if !V8_STATIC_DISPATCH_HANDLES_BOOL
  // The entries in this array are dispatch handles for builtins with SFI's.
  JSDispatchHandle* builtin_dispatch_table() { return builtin_dispatch_table_; }
  JSDispatchHandle
      builtin_dispatch_table_[JSBuiltinDispatchHandleRoot::kTableSize] = {};
#endif  // !V8_STATIC_DISPATCH_HANDLES_BOOL

  // Ensure the size is 8-byte aligned in order to make alignment of the field
  // following the IsolateData field predictable. This solves the issue with
  // C++ compilers for 32-bit platforms which are not consistent at aligning
  // int64_t fields.
  V8_NO_UNIQUE_ADDRESS uint8_t trailing_padding_[kTrailingPaddingSize];

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
  static_assert(std::is_standard_layout_v<StackGuard>);
  static_assert(std::is_standard_layout_v<RootsTable>);
  static_assert(std::is_standard_layout_v<ThreadLocalTop>);
  static_assert(std::is_standard_layout_v<ExternalReferenceTable>);
  static_assert(std::is_standard_layout_v<IsolateData>);
  static_assert(std::is_standard_layout_v<LinearAllocationArea>);
#define V(PureName, Size, Name)                                             \
  static_assert(std::is_standard_layout_v<decltype(IsolateData::Name##_)>); \
  static_assert(offsetof(IsolateData, Name##_) == k##PureName##Offset);
  ISOLATE_DATA_FIELDS(V)
#undef V
  static_assert(sizeof(IsolateData) == IsolateData::kSizeOffset);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_DATA_H_
