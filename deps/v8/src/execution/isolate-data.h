// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_DATA_H_
#define V8_EXECUTION_ISOLATE_DATA_H_

#include "src/builtins/builtins.h"
#include "src/codegen/constants-arch.h"
#include "src/codegen/external-reference-table.h"
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

#if V8_HOST_ARCH_64_BIT
// In kSystemPointerSize.
static constexpr int kFastCCallAlignmentPaddingCount = 5;
#else
static constexpr int kFastCCallAlignmentPaddingCount = 1;
#endif

#if V8_HOST_ARCH_64_BIT
#define ISOLATE_DATA_FAST_C_CALL_PADDING(V)              \
  V(kFastCCallAlignmentPaddingOffset,                    \
    kFastCCallAlignmentPaddingCount* kSystemPointerSize, \
    fast_c_call_alignment_padding)
#else
#define ISOLATE_DATA_FAST_C_CALL_PADDING(V)              \
  V(kFastCCallAlignmentPaddingOffset,                    \
    kFastCCallAlignmentPaddingCount* kSystemPointerSize, \
    fast_c_call_alignment_padding)
#endif  // V8_HOST_ARCH_64_BIT

#ifdef V8_ENABLE_LEAPTIERING

#define BUILTINS_WITH_DISPATCH_ADAPTER(V, CamelName, underscore_name, ...) \
  V(CamelName, CamelName##SharedFun)

// If we have predictable builtins then dispatch handles of builtins are
// stored in the read only segment of the JSDispatchTable. Otherwise,
// we need a table of per-isolate dispatch handles of builtins.
#if V8_ENABLE_LEAPTIERING_BOOL
#if V8_STATIC_ROOTS_BOOL
#define V8_STATIC_DISPATCH_HANDLES_BOOL true
#else
#define V8_STATIC_DISPATCH_HANDLES_BOOL false
#endif  // V8_STATIC_ROOTS_BOOL
#endif  // V8_ENABLE_LEAPTIERING_BOOL

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

#endif  // V8_ENABLE_LEAPTIERING

// IsolateData fields, defined as: V(CamelName, Size, hacker_name)
#define ISOLATE_DATA_FIELDS(V)                                                 \
  /* Misc. fields. */                                                          \
  V(CageBase, kSystemPointerSize, cage_base)                                   \
  V(StackGuard, StackGuard::kSizeInBytes, stack_guard)                         \
  V(IsMarkingFlag, kUInt8Size, is_marking_flag)                                \
  V(IsMinorMarkingFlag, kUInt8Size, is_minor_marking_flag)                     \
  V(IsSharedSpaceIsolateFlag, kUInt8Size, is_shared_space_isolate_flag)        \
  V(UsesSharedHeapFlag, kUInt8Size, uses_shared_heap_flag)                     \
  V(ExecutionMode, kUInt8Size, execution_mode)                                 \
  V(StackIsIterable, kUInt8Size, stack_is_iterable)                            \
  V(ErrorMessageParam, kUInt8Size, error_message_param)                        \
  /* This padding aligns next field to kSystemPointerSize bytes. */            \
  PADDING_FIELD(kSystemPointerSize, V, TablesAlignmentPadding,                 \
                tables_alignment_padding)                                      \
  V(RegExpStaticResultOffsetsVector, kSystemPointerSize,                       \
    regexp_static_result_offsets_vector)                                       \
  /* Tier 0 tables (small but fast access). */                                 \
  V(BuiltinTier0EntryTable, Builtins::kBuiltinTier0Count* kSystemPointerSize,  \
    builtin_tier0_entry_table)                                                 \
  V(BuiltinsTier0Table, Builtins::kBuiltinTier0Count* kSystemPointerSize,      \
    builtin_tier0_table)                                                       \
  /* Misc. fields. */                                                          \
  V(NewAllocationInfo, LinearAllocationArea::kSize, new_allocation_info)       \
  V(OldAllocationInfo, LinearAllocationArea::kSize, old_allocation_info)       \
  ISOLATE_DATA_FAST_C_CALL_PADDING(V)                                          \
  V(FastCCallCallerPC, kSystemPointerSize, fast_c_call_caller_pc)              \
  V(FastCCallCallerFP, kSystemPointerSize, fast_c_call_caller_fp)              \
  V(FastApiCallTarget, kSystemPointerSize, fast_api_call_target)               \
  V(LongTaskStatsCounter, kSizetSize, long_task_stats_counter)                 \
  V(ThreadLocalTop, ThreadLocalTop::kSizeInBytes, thread_local_top)            \
  V(HandleScopeData, HandleScopeData::kSizeInBytes, handle_scope_data)         \
  V(EmbedderData, Internals::kNumIsolateDataSlots* kSystemPointerSize,         \
    embedder_data)                                                             \
  ISOLATE_DATA_FIELDS_POINTER_COMPRESSION(V)                                   \
  ISOLATE_DATA_FIELDS_SANDBOX(V)                                               \
  V(ApiCallbackThunkArgument, kSystemPointerSize, api_callback_thunk_argument) \
  /* Because some architectures have a rather small offset in reg+offset  */   \
  /* addressing this field should be near the start.                      */   \
  /* Soon leaptiering will be standard, but in the mean time we already   */   \
  /* include this field so that the isolate layout is not dependent on    */   \
  /* an internal ifdef.                                                   */   \
  /* This would otherwise break node, which has a list of external ifdefs */   \
  /* in its common.gypi file that does not include V8_ENABLE_LEAPTIERING. */   \
  V(JSDispatchTable, kSystemPointerSize, js_dispatch_table_base)               \
  V(RegexpExecVectorArgument, kSystemPointerSize, regexp_exec_vector_argument) \
  V(ContinuationPreservedEmbedderData, kSystemPointerSize,                     \
    continuation_preserved_embedder_data)                                      \
  /* Full tables (arbitrary size, potentially slower access). */               \
  V(RootsTable, RootsTable::kEntriesCount* kSystemPointerSize, roots_table)    \
  V(ExternalReferenceTable, ExternalReferenceTable::kSizeInBytes,              \
    external_reference_table)                                                  \
  V(BuiltinEntryTable, Builtins::kBuiltinCount* kSystemPointerSize,            \
    builtin_entry_table)                                                       \
  V(BuiltinTable, Builtins::kBuiltinCount* kSystemPointerSize, builtin_table)  \
  V(ActiveStack, kSystemPointerSize, active_stack)                             \
  V(DateCacheStamp, kInt32Size, date_cache_stamp)                              \
  V(IsDateCacheUsed, kUInt8Size, is_date_cache_used)                           \
  /* This padding aligns next field to kDoubleSize bytes. */                   \
  PADDING_FIELD(kDoubleSize, V, RawArgumentsPadding, raw_arguments_padding)    \
  V(RawArguments, 2 * kDoubleSize, raw_arguments)                              \
  ISOLATE_DATA_FIELDS_LEAPTIERING(V)

#ifdef V8_COMPRESS_POINTERS
#define ISOLATE_DATA_FIELDS_POINTER_COMPRESSION(V)      \
  V(ExternalPointerTable, sizeof(ExternalPointerTable), \
    external_pointer_table)                             \
  V(SharedExternalPointerTable, kSystemPointerSize,     \
    shared_external_pointer_table)                      \
  V(CppHeapPointerTable, sizeof(CppHeapPointerTable), cpp_heap_pointer_table)
#else
#define ISOLATE_DATA_FIELDS_POINTER_COMPRESSION(V)
#endif  // V8_COMPRESS_POINTERS

#ifdef V8_ENABLE_SANDBOX
#define ISOLATE_DATA_FIELDS_SANDBOX(V)                                       \
  V(TrustedCageBase, kSystemPointerSize, trusted_cage_base)                  \
  V(TrustedPointerTable, sizeof(TrustedPointerTable), trusted_pointer_table) \
  V(SharedTrustedPointerTable, kSystemPointerSize,                           \
    shared_trusted_pointer_table)                                            \
  V(TrustedPointerPublishingScope, kSystemPointerSize,                       \
    trusted_pointer_publishing_scope)                                        \
  V(CodePointerTableBaseAddress, kSystemPointerSize,                         \
    code_pointer_table_base_address)
#else
#define ISOLATE_DATA_FIELDS_SANDBOX(V)
#endif  // V8_ENABLE_SANDBOX

#if V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL

#define ISOLATE_DATA_FIELDS_LEAPTIERING(V)                                \
  V(BuiltinDispatchTable,                                                 \
    (JSBuiltinDispatchHandleRoot::kTableSize) * sizeof(JSDispatchHandle), \
    builtin_dispatch_table)

#else

#define ISOLATE_DATA_FIELDS_LEAPTIERING(V)

#endif  // V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL

#define EXTERNAL_REFERENCE_LIST_ISOLATE_FIELDS(V)       \
  V(isolate_address, "isolate address", IsolateAddress) \
  V(jslimit_address, "jslimit address", JsLimitAddress)

constexpr uint8_t kNumIsolateFieldIds = 0
#define PLUS_1(...) +1
    EXTERNAL_REFERENCE_LIST_ISOLATE_FIELDS(PLUS_1) ISOLATE_DATA_FIELDS(PLUS_1);
#undef PLUS_1

enum class IsolateFieldId : uint8_t {
  kUnknown = 0,
#define ENUM(name, comment, CamelName) k##CamelName,
  EXTERNAL_REFERENCE_LIST_ISOLATE_FIELDS(ENUM)
#undef ENUM
#define ENUM(CamelName, ...) k##CamelName,
      ISOLATE_DATA_FIELDS(ENUM)
#undef ENUM
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
#if V8_ENABLE_LEAPTIERING_BOOL
        ,
        js_dispatch_table_base_(group->js_dispatch_table()->base_address())
#endif
  {
  }

  IsolateData(const IsolateData&) = delete;
  IsolateData& operator=(const IsolateData&) = delete;

  static constexpr intptr_t kIsolateRootBias = kRootRegisterBias;

  // The value of the kRootRegister.
  Address isolate_root() const {
    return reinterpret_cast<Address>(this) + kIsolateRootBias;
  }

  // Root-register-relative offsets.

#define V(CamelName, Size, hacker_name)             \
  static constexpr int hacker_name##_offset() {     \
    return k##CamelName##Offset - kIsolateRootBias; \
  }
  ISOLATE_DATA_FIELDS(V)
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

  static constexpr int jslimit_offset() {
    return stack_guard_offset() + StackGuard::jslimit_offset();
  }

  static constexpr int real_jslimit_offset() {
    return stack_guard_offset() + StackGuard::real_jslimit_offset();
  }

#define V(CamelName, Size, name) \
  Address name##_address() const { return reinterpret_cast<Address>(&name##_); }
  ISOLATE_DATA_FIELDS(V)
#undef V

  Address fast_c_call_caller_fp() const { return fast_c_call_caller_fp_; }
  Address fast_c_call_caller_pc() const { return fast_c_call_caller_pc_; }
  Address fast_api_call_target() const { return fast_api_call_target_; }

  static constexpr int exception_offset() {
    return thread_local_top_offset() + ThreadLocalTop::exception_offset();
  }

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
  wasm::StackMemory* active_stack() { return active_stack_; }
  void set_active_stack(wasm::StackMemory* stack) { active_stack_ = stack; }
#if V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL
  JSDispatchHandle builtin_dispatch_handle(Builtin builtin) {
    return builtin_dispatch_table_[JSBuiltinDispatchHandleRoot::to_idx(
        builtin)];
  }
#endif  // V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL

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

// Offset of a ThreadLocalTop member from {isolate_root()}.
#define THREAD_LOCAL_TOP_MEMBER_OFFSET(Name)                              \
  static constexpr uint32_t Name##_offset() {                             \
    return static_cast<uint32_t>(IsolateData::thread_local_top_offset() + \
                                 OFFSET_OF(ThreadLocalTop, Name##_));     \
  }

  THREAD_LOCAL_TOP_MEMBER_OFFSET(topmost_script_having_context)
  THREAD_LOCAL_TOP_MEMBER_OFFSET(is_on_central_stack_flag)
  THREAD_LOCAL_TOP_MEMBER_OFFSET(context)
#undef THREAD_LOCAL_TOP_MEMBER_OFFSET

  static constexpr intptr_t GetOffset(IsolateFieldId id) {
    switch (id) {
      case IsolateFieldId::kUnknown:
        UNREACHABLE();
      case IsolateFieldId::kIsolateAddress:
        return -kIsolateRootBias;
      case IsolateFieldId::kJsLimitAddress:
        return IsolateData::jslimit_offset();
#define CASE(CamelName, size, name)  \
  case IsolateFieldId::k##CamelName: \
    return IsolateData::name##_offset();
        ISOLATE_DATA_FIELDS(CASE)
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

  wasm::StackMemory* active_stack_ = nullptr;

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

#if V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL
  // The entries in this array are dispatch handles for builtins with SFI's.
  JSDispatchHandle* builtin_dispatch_table() { return builtin_dispatch_table_; }
  JSDispatchHandle
      builtin_dispatch_table_[JSBuiltinDispatchHandleRoot::kTableSize] = {};
#endif  // V8_ENABLE_LEAPTIERING_BOOL && !V8_STATIC_DISPATCH_HANDLES_BOOL

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
