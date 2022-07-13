// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_ROOTS_H_
#define V8_ROOTS_ROOTS_H_

#include "src/base/macros.h"
#include "src/builtins/accessors.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/init/heap-symbols.h"
#include "src/objects/objects-definitions.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

// Forward declarations.
enum ElementsKind : uint8_t;
template <typename T>
class Handle;
class Heap;
class Isolate;
class Map;
class PropertyCell;
class ReadOnlyHeap;
class RootVisitor;
class String;
class Symbol;

// Defines all the read-only roots in Heap.
#define STRONG_READ_ONLY_ROOT_LIST(V)                                          \
  /* Cluster the most popular ones in a few cache lines here at the top.    */ \
  /* The first 32 entries are most often used in the startup snapshot and   */ \
  /* can use a shorter representation in the serialization format.          */ \
  V(Map, free_space_map, FreeSpaceMap)                                         \
  V(Map, one_pointer_filler_map, OnePointerFillerMap)                          \
  V(Map, two_pointer_filler_map, TwoPointerFillerMap)                          \
  V(Oddball, uninitialized_value, UninitializedValue)                          \
  V(Oddball, undefined_value, UndefinedValue)                                  \
  V(Oddball, the_hole_value, TheHoleValue)                                     \
  V(Oddball, null_value, NullValue)                                            \
  V(Oddball, true_value, TrueValue)                                            \
  V(Oddball, false_value, FalseValue)                                          \
  V(String, empty_string, empty_string)                                        \
  V(Map, meta_map, MetaMap)                                                    \
  V(Map, byte_array_map, ByteArrayMap)                                         \
  V(Map, fixed_array_map, FixedArrayMap)                                       \
  V(Map, fixed_cow_array_map, FixedCOWArrayMap)                                \
  V(Map, hash_table_map, HashTableMap)                                         \
  V(Map, symbol_map, SymbolMap)                                                \
  V(Map, one_byte_string_map, OneByteStringMap)                                \
  V(Map, one_byte_internalized_string_map, OneByteInternalizedStringMap)       \
  V(Map, scope_info_map, ScopeInfoMap)                                         \
  V(Map, shared_function_info_map, SharedFunctionInfoMap)                      \
  V(Map, code_map, CodeMap)                                                    \
  V(Map, cell_map, CellMap)                                                    \
  V(Map, global_property_cell_map, GlobalPropertyCellMap)                      \
  V(Map, foreign_map, ForeignMap)                                              \
  V(Map, heap_number_map, HeapNumberMap)                                       \
  V(Map, transition_array_map, TransitionArrayMap)                             \
  V(Map, thin_one_byte_string_map, ThinOneByteStringMap)                       \
  /* TODO(mythria): Once lazy feedback lands, check if feedback vector map */  \
  /* is still a popular map */                                                 \
  V(Map, feedback_vector_map, FeedbackVectorMap)                               \
  V(ScopeInfo, empty_scope_info, EmptyScopeInfo)                               \
  V(FixedArray, empty_fixed_array, EmptyFixedArray)                            \
  V(DescriptorArray, empty_descriptor_array, EmptyDescriptorArray)             \
  /* Entries beyond the first 32                                            */ \
  /* Oddballs */                                                               \
  V(Oddball, arguments_marker, ArgumentsMarker)                                \
  V(Oddball, exception, Exception)                                             \
  V(Oddball, termination_exception, TerminationException)                      \
  V(Oddball, optimized_out, OptimizedOut)                                      \
  V(Oddball, stale_register, StaleRegister)                                    \
  /* Maps */                                                                   \
  V(Map, script_context_table_map, ScriptContextTableMap)                      \
  V(Map, closure_feedback_cell_array_map, ClosureFeedbackCellArrayMap)         \
  V(Map, feedback_metadata_map, FeedbackMetadataArrayMap)                      \
  V(Map, array_list_map, ArrayListMap)                                         \
  V(Map, bigint_map, BigIntMap)                                                \
  V(Map, object_boilerplate_description_map, ObjectBoilerplateDescriptionMap)  \
  V(Map, bytecode_array_map, BytecodeArrayMap)                                 \
  V(Map, code_data_container_map, CodeDataContainerMap)                        \
  V(Map, coverage_info_map, CoverageInfoMap)                                   \
  V(Map, fixed_double_array_map, FixedDoubleArrayMap)                          \
  V(Map, global_dictionary_map, GlobalDictionaryMap)                           \
  V(Map, many_closures_cell_map, ManyClosuresCellMap)                          \
  V(Map, mega_dom_handler_map, MegaDomHandlerMap)                              \
  V(Map, module_info_map, ModuleInfoMap)                                       \
  V(Map, name_dictionary_map, NameDictionaryMap)                               \
  V(Map, no_closures_cell_map, NoClosuresCellMap)                              \
  V(Map, number_dictionary_map, NumberDictionaryMap)                           \
  V(Map, one_closure_cell_map, OneClosureCellMap)                              \
  V(Map, ordered_hash_map_map, OrderedHashMapMap)                              \
  V(Map, ordered_hash_set_map, OrderedHashSetMap)                              \
  V(Map, name_to_index_hash_table_map, NameToIndexHashTableMap)                \
  V(Map, registered_symbol_table_map, RegisteredSymbolTableMap)                \
  V(Map, ordered_name_dictionary_map, OrderedNameDictionaryMap)                \
  V(Map, preparse_data_map, PreparseDataMap)                                   \
  V(Map, property_array_map, PropertyArrayMap)                                 \
  V(Map, side_effect_call_handler_info_map, SideEffectCallHandlerInfoMap)      \
  V(Map, side_effect_free_call_handler_info_map,                               \
    SideEffectFreeCallHandlerInfoMap)                                          \
  V(Map, next_call_side_effect_free_call_handler_info_map,                     \
    NextCallSideEffectFreeCallHandlerInfoMap)                                  \
  V(Map, simple_number_dictionary_map, SimpleNumberDictionaryMap)              \
  V(Map, small_ordered_hash_map_map, SmallOrderedHashMapMap)                   \
  V(Map, small_ordered_hash_set_map, SmallOrderedHashSetMap)                   \
  V(Map, small_ordered_name_dictionary_map, SmallOrderedNameDictionaryMap)     \
  V(Map, source_text_module_map, SourceTextModuleMap)                          \
  V(Map, swiss_name_dictionary_map, SwissNameDictionaryMap)                    \
  V(Map, synthetic_module_map, SyntheticModuleMap)                             \
  IF_WASM(V, Map, wasm_api_function_ref_map, WasmApiFunctionRefMap)            \
  IF_WASM(V, Map, wasm_capi_function_data_map, WasmCapiFunctionDataMap)        \
  IF_WASM(V, Map, wasm_exported_function_data_map,                             \
          WasmExportedFunctionDataMap)                                         \
  IF_WASM(V, Map, wasm_internal_function_map, WasmInternalFunctionMap)         \
  IF_WASM(V, Map, wasm_js_function_data_map, WasmJSFunctionDataMap)            \
  IF_WASM(V, Map, wasm_onfulfilled_data_map, WasmOnFulfilledDataMap)           \
  IF_WASM(V, Map, wasm_type_info_map, WasmTypeInfoMap)                         \
  V(Map, weak_fixed_array_map, WeakFixedArrayMap)                              \
  V(Map, weak_array_list_map, WeakArrayListMap)                                \
  V(Map, ephemeron_hash_table_map, EphemeronHashTableMap)                      \
  V(Map, embedder_data_array_map, EmbedderDataArrayMap)                        \
  V(Map, weak_cell_map, WeakCellMap)                                           \
  /* String maps */                                                            \
  V(Map, string_map, StringMap)                                                \
  V(Map, cons_one_byte_string_map, ConsOneByteStringMap)                       \
  V(Map, cons_string_map, ConsStringMap)                                       \
  V(Map, thin_string_map, ThinStringMap)                                       \
  V(Map, sliced_string_map, SlicedStringMap)                                   \
  V(Map, sliced_one_byte_string_map, SlicedOneByteStringMap)                   \
  V(Map, external_string_map, ExternalStringMap)                               \
  V(Map, external_one_byte_string_map, ExternalOneByteStringMap)               \
  V(Map, uncached_external_string_map, UncachedExternalStringMap)              \
  V(Map, internalized_string_map, InternalizedStringMap)                       \
  V(Map, external_internalized_string_map, ExternalInternalizedStringMap)      \
  V(Map, external_one_byte_internalized_string_map,                            \
    ExternalOneByteInternalizedStringMap)                                      \
  V(Map, uncached_external_internalized_string_map,                            \
    UncachedExternalInternalizedStringMap)                                     \
  V(Map, uncached_external_one_byte_internalized_string_map,                   \
    UncachedExternalOneByteInternalizedStringMap)                              \
  V(Map, uncached_external_one_byte_string_map,                                \
    UncachedExternalOneByteStringMap)                                          \
  V(Map, shared_one_byte_string_map, SharedOneByteStringMap)                   \
  V(Map, shared_string_map, SharedStringMap)                                   \
  V(Map, shared_thin_one_byte_string_map, SharedThinOneByteStringMap)          \
  V(Map, shared_thin_string_map, SharedThinStringMap)                          \
  V(Map, seq_string_migration_sentinel_map,                                    \
    TwoByteSeqStringMigrationSentinelMap)                                      \
  V(Map, one_byte_seq_string_migration_sentinel_map,                           \
    OneByteSeqStringMigrationSentinelMap)                                      \
  /* Oddball maps */                                                           \
  V(Map, undefined_map, UndefinedMap)                                          \
  V(Map, the_hole_map, TheHoleMap)                                             \
  V(Map, null_map, NullMap)                                                    \
  V(Map, boolean_map, BooleanMap)                                              \
  V(Map, uninitialized_map, UninitializedMap)                                  \
  V(Map, arguments_marker_map, ArgumentsMarkerMap)                             \
  V(Map, exception_map, ExceptionMap)                                          \
  V(Map, termination_exception_map, TerminationExceptionMap)                   \
  V(Map, optimized_out_map, OptimizedOutMap)                                   \
  V(Map, stale_register_map, StaleRegisterMap)                                 \
  V(Map, self_reference_marker_map, SelfReferenceMarkerMap)                    \
  V(Map, basic_block_counters_marker_map, BasicBlockCountersMarkerMap)         \
  /* Canonical empty values */                                                 \
  V(EnumCache, empty_enum_cache, EmptyEnumCache)                               \
  V(PropertyArray, empty_property_array, EmptyPropertyArray)                   \
  V(ByteArray, empty_byte_array, EmptyByteArray)                               \
  V(ObjectBoilerplateDescription, empty_object_boilerplate_description,        \
    EmptyObjectBoilerplateDescription)                                         \
  V(ArrayBoilerplateDescription, empty_array_boilerplate_description,          \
    EmptyArrayBoilerplateDescription)                                          \
  V(ClosureFeedbackCellArray, empty_closure_feedback_cell_array,               \
    EmptyClosureFeedbackCellArray)                                             \
  V(NumberDictionary, empty_slow_element_dictionary,                           \
    EmptySlowElementDictionary)                                                \
  V(OrderedHashMap, empty_ordered_hash_map, EmptyOrderedHashMap)               \
  V(OrderedHashSet, empty_ordered_hash_set, EmptyOrderedHashSet)               \
  V(FeedbackMetadata, empty_feedback_metadata, EmptyFeedbackMetadata)          \
  V(NameDictionary, empty_property_dictionary, EmptyPropertyDictionary)        \
  V(OrderedNameDictionary, empty_ordered_property_dictionary,                  \
    EmptyOrderedPropertyDictionary)                                            \
  V(SwissNameDictionary, empty_swiss_property_dictionary,                      \
    EmptySwissPropertyDictionary)                                              \
  V(InterceptorInfo, noop_interceptor_info, NoOpInterceptorInfo)               \
  V(ArrayList, empty_array_list, EmptyArrayList)                               \
  V(WeakFixedArray, empty_weak_fixed_array, EmptyWeakFixedArray)               \
  V(WeakArrayList, empty_weak_array_list, EmptyWeakArrayList)                  \
  /* Special numbers */                                                        \
  V(HeapNumber, nan_value, NanValue)                                           \
  V(HeapNumber, hole_nan_value, HoleNanValue)                                  \
  V(HeapNumber, infinity_value, InfinityValue)                                 \
  V(HeapNumber, minus_zero_value, MinusZeroValue)                              \
  V(HeapNumber, minus_infinity_value, MinusInfinityValue)                      \
  /* Marker for self-references during code-generation */                      \
  V(HeapObject, self_reference_marker, SelfReferenceMarker)                    \
  /* Marker for basic-block usage counters array during code-generation */     \
  V(Oddball, basic_block_counters_marker, BasicBlockCountersMarker)            \
  /* Canonical off-heap trampoline data */                                     \
  V(ByteArray, off_heap_trampoline_relocation_info,                            \
    OffHeapTrampolineRelocationInfo)                                           \
  V(HeapObject, trampoline_trivial_code_data_container,                        \
    TrampolineTrivialCodeDataContainer)                                        \
  V(HeapObject, trampoline_promise_rejection_code_data_container,              \
    TrampolinePromiseRejectionCodeDataContainer)                               \
  /* Canonical scope infos */                                                  \
  V(ScopeInfo, global_this_binding_scope_info, GlobalThisBindingScopeInfo)     \
  V(ScopeInfo, empty_function_scope_info, EmptyFunctionScopeInfo)              \
  V(ScopeInfo, native_scope_info, NativeScopeInfo)                             \
  /* Hash seed */                                                              \
  V(ByteArray, hash_seed, HashSeed)

// Mutable roots that are known to be immortal immovable, for which we can
// safely skip write barriers.
#define STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(V)                                  \
  ACCESSOR_INFO_ROOT_LIST(V)                                                   \
  /* Maps */                                                                   \
  V(Map, external_map, ExternalMap)                                            \
  V(Map, message_object_map, JSMessageObjectMap)                               \
  /* Canonical empty values */                                                 \
  V(Script, empty_script, EmptyScript)                                         \
  V(FeedbackCell, many_closures_cell, ManyClosuresCell)                        \
  V(Cell, invalid_prototype_validity_cell, InvalidPrototypeValidityCell)       \
  /* Protectors */                                                             \
  V(PropertyCell, array_constructor_protector, ArrayConstructorProtector)      \
  V(PropertyCell, no_elements_protector, NoElementsProtector)                  \
  V(PropertyCell, mega_dom_protector, MegaDOMProtector)                        \
  V(PropertyCell, is_concat_spreadable_protector, IsConcatSpreadableProtector) \
  V(PropertyCell, array_species_protector, ArraySpeciesProtector)              \
  V(PropertyCell, typed_array_species_protector, TypedArraySpeciesProtector)   \
  V(PropertyCell, promise_species_protector, PromiseSpeciesProtector)          \
  V(PropertyCell, regexp_species_protector, RegExpSpeciesProtector)            \
  V(PropertyCell, string_length_protector, StringLengthProtector)              \
  V(PropertyCell, array_iterator_protector, ArrayIteratorProtector)            \
  V(PropertyCell, array_buffer_detaching_protector,                            \
    ArrayBufferDetachingProtector)                                             \
  V(PropertyCell, promise_hook_protector, PromiseHookProtector)                \
  V(PropertyCell, promise_resolve_protector, PromiseResolveProtector)          \
  V(PropertyCell, map_iterator_protector, MapIteratorProtector)                \
  V(PropertyCell, promise_then_protector, PromiseThenProtector)                \
  V(PropertyCell, set_iterator_protector, SetIteratorProtector)                \
  V(PropertyCell, string_iterator_protector, StringIteratorProtector)          \
  /* Caches */                                                                 \
  V(FixedArray, single_character_string_cache, SingleCharacterStringCache)     \
  V(FixedArray, string_split_cache, StringSplitCache)                          \
  V(FixedArray, regexp_multiple_cache, RegExpMultipleCache)                    \
  /* Indirection lists for isolate-independent builtins */                     \
  V(FixedArray, builtins_constants_table, BuiltinsConstantsTable)              \
  /* Internal SharedFunctionInfos */                                           \
  V(SharedFunctionInfo, async_function_await_reject_shared_fun,                \
    AsyncFunctionAwaitRejectSharedFun)                                         \
  V(SharedFunctionInfo, async_function_await_resolve_shared_fun,               \
    AsyncFunctionAwaitResolveSharedFun)                                        \
  V(SharedFunctionInfo, async_generator_await_reject_shared_fun,               \
    AsyncGeneratorAwaitRejectSharedFun)                                        \
  V(SharedFunctionInfo, async_generator_await_resolve_shared_fun,              \
    AsyncGeneratorAwaitResolveSharedFun)                                       \
  V(SharedFunctionInfo, async_generator_yield_resolve_shared_fun,              \
    AsyncGeneratorYieldResolveSharedFun)                                       \
  V(SharedFunctionInfo, async_generator_return_resolve_shared_fun,             \
    AsyncGeneratorReturnResolveSharedFun)                                      \
  V(SharedFunctionInfo, async_generator_return_closed_reject_shared_fun,       \
    AsyncGeneratorReturnClosedRejectSharedFun)                                 \
  V(SharedFunctionInfo, async_generator_return_closed_resolve_shared_fun,      \
    AsyncGeneratorReturnClosedResolveSharedFun)                                \
  V(SharedFunctionInfo, async_iterator_value_unwrap_shared_fun,                \
    AsyncIteratorValueUnwrapSharedFun)                                         \
  V(SharedFunctionInfo, promise_all_resolve_element_shared_fun,                \
    PromiseAllResolveElementSharedFun)                                         \
  V(SharedFunctionInfo, promise_all_settled_resolve_element_shared_fun,        \
    PromiseAllSettledResolveElementSharedFun)                                  \
  V(SharedFunctionInfo, promise_all_settled_reject_element_shared_fun,         \
    PromiseAllSettledRejectElementSharedFun)                                   \
  V(SharedFunctionInfo, promise_any_reject_element_shared_fun,                 \
    PromiseAnyRejectElementSharedFun)                                          \
  V(SharedFunctionInfo, promise_capability_default_reject_shared_fun,          \
    PromiseCapabilityDefaultRejectSharedFun)                                   \
  V(SharedFunctionInfo, promise_capability_default_resolve_shared_fun,         \
    PromiseCapabilityDefaultResolveSharedFun)                                  \
  V(SharedFunctionInfo, promise_catch_finally_shared_fun,                      \
    PromiseCatchFinallySharedFun)                                              \
  V(SharedFunctionInfo, promise_get_capabilities_executor_shared_fun,          \
    PromiseGetCapabilitiesExecutorSharedFun)                                   \
  V(SharedFunctionInfo, promise_then_finally_shared_fun,                       \
    PromiseThenFinallySharedFun)                                               \
  V(SharedFunctionInfo, promise_thrower_finally_shared_fun,                    \
    PromiseThrowerFinallySharedFun)                                            \
  V(SharedFunctionInfo, promise_value_thunk_finally_shared_fun,                \
    PromiseValueThunkFinallySharedFun)                                         \
  V(SharedFunctionInfo, proxy_revoke_shared_fun, ProxyRevokeSharedFun)

// These root references can be updated by the mutator.
#define STRONG_MUTABLE_MOVABLE_ROOT_LIST(V)                                 \
  /* Caches */                                                              \
  V(FixedArray, number_string_cache, NumberStringCache)                     \
  /* Lists and dictionaries */                                              \
  V(RegisteredSymbolTable, public_symbol_table, PublicSymbolTable)          \
  V(RegisteredSymbolTable, api_symbol_table, ApiSymbolTable)                \
  V(RegisteredSymbolTable, api_private_symbol_table, ApiPrivateSymbolTable) \
  V(WeakArrayList, script_list, ScriptList)                                 \
  V(FixedArray, materialized_objects, MaterializedObjects)                  \
  V(WeakArrayList, detached_contexts, DetachedContexts)                     \
  V(WeakArrayList, retaining_path_targets, RetainingPathTargets)            \
  /* Feedback vectors that we need for code coverage or type profile */     \
  V(Object, feedback_vectors_for_profiling_tools,                           \
    FeedbackVectorsForProfilingTools)                                       \
  V(FixedArray, serialized_objects, SerializedObjects)                      \
  V(FixedArray, serialized_global_proxy_sizes, SerializedGlobalProxySizes)  \
  V(TemplateList, message_listeners, MessageListeners)                      \
  /* Support for async stack traces */                                      \
  V(HeapObject, current_microtask, CurrentMicrotask)                        \
  /* KeepDuringJob set for JS WeakRefs */                                   \
  V(HeapObject, weak_refs_keep_during_job, WeakRefsKeepDuringJob)           \
  V(HeapObject, interpreter_entry_trampoline_for_profiling,                 \
    InterpreterEntryTrampolineForProfiling)                                 \
  V(Object, pending_optimize_for_test_bytecode,                             \
    PendingOptimizeForTestBytecode)                                         \
  V(ArrayList, basic_block_profiling_data, BasicBlockProfilingData)         \
  V(WeakArrayList, shared_wasm_memories, SharedWasmMemories)                \
  IF_WASM(V, HeapObject, active_continuation, ActiveContinuation)           \
  IF_WASM(V, HeapObject, active_suspender, ActiveSuspender)                 \
  IF_WASM(V, WeakArrayList, wasm_canonical_rtts, WasmCanonicalRtts)

// Entries in this list are limited to Smis and are not visited during GC.
#define SMI_ROOT_LIST(V)                                                       \
  V(Smi, last_script_id, LastScriptId)                                         \
  V(Smi, last_debugging_id, LastDebuggingId)                                   \
  /* To distinguish the function templates, so that we can find them in the */ \
  /* function cache of the native context. */                                  \
  V(Smi, next_template_serial_number, NextTemplateSerialNumber)                \
  V(Smi, construct_stub_create_deopt_pc_offset,                                \
    ConstructStubCreateDeoptPCOffset)                                          \
  V(Smi, construct_stub_invoke_deopt_pc_offset,                                \
    ConstructStubInvokeDeoptPCOffset)                                          \
  V(Smi, interpreter_entry_return_pc_offset, InterpreterEntryReturnPCOffset)

// Adapts one INTERNALIZED_STRING_LIST_GENERATOR entry to
// the ROOT_LIST-compatible entry
#define INTERNALIZED_STRING_LIST_ADAPTER(V, name, ...) V(String, name, name)

// Produces (String, name, CamelCase) entries
#define INTERNALIZED_STRING_ROOT_LIST(V) \
  INTERNALIZED_STRING_LIST_GENERATOR(INTERNALIZED_STRING_LIST_ADAPTER, V)

// Adapts one XXX_SYMBOL_LIST_GENERATOR entry to the ROOT_LIST-compatible entry
#define SYMBOL_ROOT_LIST_ADAPTER(V, name, ...) V(Symbol, name, name)

// Produces (Symbol, name, CamelCase) entries
#define PRIVATE_SYMBOL_ROOT_LIST(V) \
  PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)
#define PUBLIC_SYMBOL_ROOT_LIST(V) \
  PUBLIC_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)
#define WELL_KNOWN_SYMBOL_ROOT_LIST(V) \
  WELL_KNOWN_SYMBOL_LIST_GENERATOR(SYMBOL_ROOT_LIST_ADAPTER, V)

// Adapts one ACCESSOR_INFO_LIST_GENERATOR entry to the ROOT_LIST-compatible
// entry
#define ACCESSOR_INFO_ROOT_LIST_ADAPTER(V, name, CamelName, ...) \
  V(AccessorInfo, name##_accessor, CamelName##Accessor)

// Produces (AccessorInfo, name, CamelCase) entries
#define ACCESSOR_INFO_ROOT_LIST(V) \
  ACCESSOR_INFO_LIST_GENERATOR(ACCESSOR_INFO_ROOT_LIST_ADAPTER, V)

#define READ_ONLY_ROOT_LIST(V)     \
  STRONG_READ_ONLY_ROOT_LIST(V)    \
  INTERNALIZED_STRING_ROOT_LIST(V) \
  PRIVATE_SYMBOL_ROOT_LIST(V)      \
  PUBLIC_SYMBOL_ROOT_LIST(V)       \
  WELL_KNOWN_SYMBOL_ROOT_LIST(V)   \
  STRUCT_MAPS_LIST(V)              \
  TORQUE_DEFINED_MAP_ROOT_LIST(V)  \
  ALLOCATION_SITE_MAPS_LIST(V)     \
  DATA_HANDLER_MAPS_LIST(V)

#define MUTABLE_ROOT_LIST(V)                \
  STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(V)     \
  STRONG_MUTABLE_MOVABLE_ROOT_LIST(V)       \
  SMI_ROOT_LIST(V)

#define ROOT_LIST(V)     \
  READ_ONLY_ROOT_LIST(V) \
  MUTABLE_ROOT_LIST(V)

// Declare all the root indices.  This defines the root list order.
// clang-format off
enum class RootIndex : uint16_t {
#define DECL(type, name, CamelName) k##CamelName,
  ROOT_LIST(DECL)
#undef DECL

  kRootListLength,

  // Helper aliases for inclusive regions of root indices.
  kFirstRoot = 0,
  kLastRoot = kRootListLength - 1,

#define ROOT(...) +1
  kReadOnlyRootsCount = 0 READ_ONLY_ROOT_LIST(ROOT),
  kImmortalImmovableRootsCount =
      kReadOnlyRootsCount STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(ROOT),
#undef ROOT
  kFirstReadOnlyRoot = kFirstRoot,
  kLastReadOnlyRoot = kFirstReadOnlyRoot + kReadOnlyRootsCount - 1,

  // The strong roots visited by the garbage collector (not including read-only
  // roots).
#define ROOT(...) +1
  kMutableRootsCount = 0
      STRONG_MUTABLE_IMMOVABLE_ROOT_LIST(ROOT)
      STRONG_MUTABLE_MOVABLE_ROOT_LIST(ROOT),
#undef ROOT
  kFirstStrongRoot = kLastReadOnlyRoot + 1,
  kLastStrongRoot = kFirstStrongRoot + kMutableRootsCount - 1,

  // All of the strong roots plus the read-only roots.
  kFirstStrongOrReadOnlyRoot = kFirstRoot,
  kLastStrongOrReadOnlyRoot = kLastStrongRoot,

  // All immortal immovable roots including read only ones.
  kFirstImmortalImmovableRoot = kFirstReadOnlyRoot,
  kLastImmortalImmovableRoot =
      kFirstImmortalImmovableRoot + kImmortalImmovableRootsCount - 1,

  kFirstSmiRoot = kLastStrongRoot + 1,
  kLastSmiRoot = kLastRoot
};
// clang-format on

// Represents a storage of V8 heap roots.
class RootsTable {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kRootListLength);

  RootsTable() : roots_{} {}

  inline bool IsRootHandleLocation(Address* handle_location,
                                   RootIndex* index) const;

  template <typename T>
  bool IsRootHandle(Handle<T> handle, RootIndex* index) const;

  Address const& operator[](RootIndex root_index) const {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return roots_[index];
  }

  FullObjectSlot slot(RootIndex root_index) {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return FullObjectSlot(&roots_[index]);
  }

  static const char* name(RootIndex root_index) {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return root_names_[index];
  }

  static constexpr int offset_of(RootIndex root_index) {
    return static_cast<int>(root_index) * kSystemPointerSize;
  }

  // Immortal immovable root objects are allocated in OLD space and GC never
  // moves them and the root table entries are guaranteed to not be modified
  // after initialization. Note, however, that contents of those root objects
  // that are allocated in writable space can still be modified after
  // initialization.
  // Generated code can treat direct references to these roots as constants.
  static constexpr bool IsImmortalImmovable(RootIndex root_index) {
    STATIC_ASSERT(static_cast<int>(RootIndex::kFirstImmortalImmovableRoot) ==
                  0);
    return static_cast<unsigned>(root_index) <=
           static_cast<unsigned>(RootIndex::kLastImmortalImmovableRoot);
  }

 private:
  FullObjectSlot begin() {
    return FullObjectSlot(&roots_[static_cast<size_t>(RootIndex::kFirstRoot)]);
  }
  FullObjectSlot end() {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastRoot) + 1]);
  }

  // Used for iterating over all of the read-only and mutable strong roots.
  FullObjectSlot strong_or_read_only_roots_begin() const {
    STATIC_ASSERT(static_cast<size_t>(RootIndex::kLastReadOnlyRoot) ==
                  static_cast<size_t>(RootIndex::kFirstStrongRoot) - 1);
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstStrongOrReadOnlyRoot)]);
  }
  FullObjectSlot strong_or_read_only_roots_end() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastStrongOrReadOnlyRoot) + 1]);
  }

  // The read-only, strong and Smi roots as defined by these accessors are all
  // disjoint.
  FullObjectSlot read_only_roots_begin() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstReadOnlyRoot)]);
  }
  FullObjectSlot read_only_roots_end() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastReadOnlyRoot) + 1]);
  }

  FullObjectSlot strong_roots_begin() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstStrongRoot)]);
  }
  FullObjectSlot strong_roots_end() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastStrongRoot) + 1]);
  }

  FullObjectSlot smi_roots_begin() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kFirstSmiRoot)]);
  }
  FullObjectSlot smi_roots_end() const {
    return FullObjectSlot(
        &roots_[static_cast<size_t>(RootIndex::kLastSmiRoot) + 1]);
  }

  Address& operator[](RootIndex root_index) {
    size_t index = static_cast<size_t>(root_index);
    DCHECK_LT(index, kEntriesCount);
    return roots_[index];
  }

  Address roots_[kEntriesCount];
  static const char* root_names_[kEntriesCount];

  friend class Isolate;
  friend class Heap;
  friend class Factory;
  friend class PointerCompressedReadOnlyArtifacts;
  friend class ReadOnlyHeap;
  friend class ReadOnlyRoots;
  friend class RootsSerializer;
  friend class SoleReadOnlyHeap;
};

class ReadOnlyRoots {
 public:
  static constexpr size_t kEntriesCount =
      static_cast<size_t>(RootIndex::kReadOnlyRootsCount);

  V8_INLINE explicit ReadOnlyRoots(Heap* heap);
  V8_INLINE explicit ReadOnlyRoots(Isolate* isolate);
  V8_INLINE explicit ReadOnlyRoots(LocalIsolate* isolate);

  // For `v8_enable_map_packing=true`, this will return a packed (also untagged)
  // map-word instead of a tagged heap pointer.
  MapWord one_pointer_filler_map_word();

#define ROOT_ACCESSOR(Type, name, CamelName)     \
  V8_INLINE class Type name() const;             \
  V8_INLINE class Type unchecked_##name() const; \
  V8_INLINE Handle<Type> name##_handle() const;

  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // Get the address of a given read-only root index, without type checks.
  V8_INLINE Address at(RootIndex root_index) const;

  // Iterate over all the read-only roots. This is not necessary for garbage
  // collection and is usually only performed as part of (de)serialization or
  // heap verification.
  void Iterate(RootVisitor* visitor);

 private:
#ifdef DEBUG
#define ROOT_TYPE_CHECK(Type, name, CamelName) \
  V8_EXPORT_PRIVATE bool CheckType_##name() const;

  READ_ONLY_ROOT_LIST(ROOT_TYPE_CHECK)
#undef ROOT_TYPE_CHECK
#endif

  V8_INLINE explicit ReadOnlyRoots(Address* ro_roots)
      : read_only_roots_(ro_roots) {}

  V8_INLINE Address* GetLocation(RootIndex root_index) const;

  Address* read_only_roots_;

  friend class ReadOnlyHeap;
  friend class DeserializerAllocator;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ROOTS_ROOTS_H_
