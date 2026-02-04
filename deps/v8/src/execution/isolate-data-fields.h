// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_DATA_FIELDS_H_
#define V8_EXECUTION_ISOLATE_DATA_FIELDS_H_

#include "src/common/globals.h"
#include "src/utils/utils.h"

namespace v8::internal {

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
  V(HasLazyClosures, kUInt8Size, has_lazy_closures)                            \
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
  V(NewAllocationInfo, LinearAllocationArea::Size(), new_allocation_info)      \
  V(OldAllocationInfo, LinearAllocationArea::Size(), old_allocation_info)      \
  V(LastYoungAllocation, kSystemPointerSize, last_young_allocation)            \
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
  IF_WASM(V, ActiveStack, kSystemPointerSize, active_stack)                    \
  IF_WASM(V, ActiveSuspender, kSystemPointerSize, active_suspender)            \
  V(DateCacheStamp, kInt32Size, date_cache_stamp)                              \
  V(IsDateCacheUsed, kUInt8Size, is_date_cache_used)                           \
  /* This padding aligns next field to kDoubleSize bytes. */                   \
  PADDING_FIELD(kDoubleSize, V, RawArgumentsPadding, raw_arguments_padding)    \
  V(RawArguments, 2 * kDoubleSize, raw_arguments)                              \
  V(StressDeoptCount, kUInt64Size, stress_deopt_count)                         \
  ISOLATE_DATA_FIELDS_TIERING(V)

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

#if V8_STATIC_DISPATCH_HANDLES_BOOL
#define ISOLATE_DATA_FIELDS_TIERING(V)
#else
#define ISOLATE_DATA_FIELDS_TIERING(V)                                    \
  V(BuiltinDispatchTable,                                                 \
    (JSBuiltinDispatchHandleRoot::kTableSize) * sizeof(JSDispatchHandle), \
    builtin_dispatch_table)
#endif  //  !V8_STATIC_DISPATCH_HANDLES_BOOL

// Data stored in complex IsolateData field, defined as:
// V(CamelName, hacker_name, holder_field_name, FieldOffset)
#define ISOLATE_DATA_SUBFIELDS(V)                                              \
  V(IsolateAddress, isolate_address, isolate_data, 0)                          \
                                                                               \
  /* ThreadLocalTop fields. */                                                 \
  V(Context, context, thread_local_top, offsetof(ThreadLocalTop, context_))    \
  V(Exception, exception, thread_local_top,                                    \
    offsetof(ThreadLocalTop, exception_))                                      \
  V(TopmostScriptHavingContext, topmost_script_having_context,                 \
    thread_local_top,                                                          \
    offsetof(ThreadLocalTop, topmost_script_having_context_))                  \
  V(CEntryFP, c_entry_fp, thread_local_top,                                    \
    offsetof(ThreadLocalTop, c_entry_fp_))                                     \
  V(Handler, handler, thread_local_top, offsetof(ThreadLocalTop, handler_))    \
  V(CFunction, c_function, thread_local_top,                                   \
    offsetof(ThreadLocalTop, c_function_))                                     \
  V(PendingHandlerContext, pending_handler_context, thread_local_top,          \
    offsetof(ThreadLocalTop, pending_handler_context_))                        \
  V(PendingHandlerEntrypoint, pending_handler_entrypoint, thread_local_top,    \
    offsetof(ThreadLocalTop, pending_handler_entrypoint_))                     \
  V(PendingHandlerConstantPool, pending_handler_constant_pool,                 \
    thread_local_top,                                                          \
    offsetof(ThreadLocalTop, pending_handler_constant_pool_))                  \
  V(PendingHandlerFP, pending_handler_fp, thread_local_top,                    \
    offsetof(ThreadLocalTop, pending_handler_fp_))                             \
  V(PendingHandlerSP, pending_handler_sp, thread_local_top,                    \
    offsetof(ThreadLocalTop, pending_handler_sp_))                             \
  V(NumFramesAbovePendingHandler, num_frames_above_pending_handler,            \
    thread_local_top,                                                          \
    offsetof(ThreadLocalTop, num_frames_above_pending_handler_))               \
  V(IsOnCentralStackFlag, is_on_central_stack_flag, thread_local_top,          \
    offsetof(ThreadLocalTop, is_on_central_stack_flag_))                       \
  V(JSEntrySP, js_entry_sp, thread_local_top,                                  \
    offsetof(ThreadLocalTop, js_entry_sp_))                                    \
                                                                               \
  /* HandleScopeData fields. */                                                \
  V(HandleScopeLevel, handle_scope_level, handle_scope_data,                   \
    offsetof(HandleScopeData, level))                                          \
  V(HandleScopeNext, handle_scope_next, handle_scope_data,                     \
    offsetof(HandleScopeData, next))                                           \
  V(HandleScopeLimit, handle_scope_limit, handle_scope_data,                   \
    offsetof(HandleScopeData, limit))                                          \
                                                                               \
  /* NewAllocationInfo fields. */                                              \
  V(NewAllocationInfoStart, new_allocation_info_start, new_allocation_info,    \
    LinearAllocationArea::StartOffset())                                       \
  V(NewAllocationInfoTop, new_allocation_info_top, new_allocation_info,        \
    LinearAllocationArea::TopOffset())                                         \
  V(NewAllocationInfoLimit, new_allocation_info_limit, new_allocation_info,    \
    LinearAllocationArea::LimitOffset())                                       \
                                                                               \
  /* OldAllocationInfo fields. */                                              \
  V(OldAllocationInfoStart, old_allocation_info_start, old_allocation_info,    \
    LinearAllocationArea::StartOffset())                                       \
  V(OldAllocationInfoTop, old_allocation_info_top, old_allocation_info,        \
    LinearAllocationArea::TopOffset())                                         \
  V(OldAllocationInfoLimit, old_allocation_info_limit, old_allocation_info,    \
    LinearAllocationArea::LimitOffset())                                       \
                                                                               \
  /* StackGuard fields. */                                                     \
  V(JsLimit, jslimit, stack_guard, StackGuard::jslimit_offset())               \
  V(RealJsLimit, real_jslimit, stack_guard, StackGuard::real_jslimit_offset()) \
  V(NoHeapWriteInterruptRequest, no_heap_write_interrupt_request, stack_guard, \
    StackGuard::no_heap_write_interrupt_request_offset())

constexpr uint8_t kNumIsolateFieldIds = 0
#define PLUS_1(...) +1
    ISOLATE_DATA_FIELDS(PLUS_1)  //
    ISOLATE_DATA_SUBFIELDS(PLUS_1);
#undef PLUS_1

enum class IsolateFieldId : uint8_t {
#define ENUM(CamelName, ...) k##CamelName,
  ISOLATE_DATA_FIELDS(ENUM)     //
  ISOLATE_DATA_SUBFIELDS(ENUM)  //
#undef ENUM
};

}  // namespace v8::internal

#endif  // V8_EXECUTION_ISOLATE_DATA_FIELDS_H_
