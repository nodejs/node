// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_H_
#define V8_HEAP_HEAP_H_

#include <cmath>
#include <map>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/atomic-utils.h"
#include "src/globals.h"
// TODO(mstarzinger): Two more includes to kill!
#include "src/heap/spaces.h"
#include "src/heap/store-buffer.h"
#include "src/list.h"

namespace v8 {
namespace internal {

// Defines all the roots in Heap.
#define STRONG_ROOT_LIST(V)                                                    \
  V(Map, byte_array_map, ByteArrayMap)                                         \
  V(Map, free_space_map, FreeSpaceMap)                                         \
  V(Map, one_pointer_filler_map, OnePointerFillerMap)                          \
  V(Map, two_pointer_filler_map, TwoPointerFillerMap)                          \
  /* Cluster the most popular ones in a few cache lines here at the top.    */ \
  V(Smi, store_buffer_top, StoreBufferTop)                                     \
  V(Oddball, undefined_value, UndefinedValue)                                  \
  V(Oddball, the_hole_value, TheHoleValue)                                     \
  V(Oddball, null_value, NullValue)                                            \
  V(Oddball, true_value, TrueValue)                                            \
  V(Oddball, false_value, FalseValue)                                          \
  V(String, empty_string, empty_string)                                        \
  V(String, hidden_string, hidden_string)                                      \
  V(Oddball, uninitialized_value, UninitializedValue)                          \
  V(Map, cell_map, CellMap)                                                    \
  V(Map, global_property_cell_map, GlobalPropertyCellMap)                      \
  V(Map, shared_function_info_map, SharedFunctionInfoMap)                      \
  V(Map, meta_map, MetaMap)                                                    \
  V(Map, heap_number_map, HeapNumberMap)                                       \
  V(Map, mutable_heap_number_map, MutableHeapNumberMap)                        \
  V(Map, float32x4_map, Float32x4Map)                                          \
  V(Map, int32x4_map, Int32x4Map)                                              \
  V(Map, uint32x4_map, Uint32x4Map)                                            \
  V(Map, bool32x4_map, Bool32x4Map)                                            \
  V(Map, int16x8_map, Int16x8Map)                                              \
  V(Map, uint16x8_map, Uint16x8Map)                                            \
  V(Map, bool16x8_map, Bool16x8Map)                                            \
  V(Map, int8x16_map, Int8x16Map)                                              \
  V(Map, uint8x16_map, Uint8x16Map)                                            \
  V(Map, bool8x16_map, Bool8x16Map)                                            \
  V(Map, native_context_map, NativeContextMap)                                 \
  V(Map, fixed_array_map, FixedArrayMap)                                       \
  V(Map, code_map, CodeMap)                                                    \
  V(Map, scope_info_map, ScopeInfoMap)                                         \
  V(Map, fixed_cow_array_map, FixedCOWArrayMap)                                \
  V(Map, fixed_double_array_map, FixedDoubleArrayMap)                          \
  V(Map, weak_cell_map, WeakCellMap)                                           \
  V(Map, transition_array_map, TransitionArrayMap)                             \
  V(Map, one_byte_string_map, OneByteStringMap)                                \
  V(Map, one_byte_internalized_string_map, OneByteInternalizedStringMap)       \
  V(Map, function_context_map, FunctionContextMap)                             \
  V(FixedArray, empty_fixed_array, EmptyFixedArray)                            \
  V(ByteArray, empty_byte_array, EmptyByteArray)                               \
  V(DescriptorArray, empty_descriptor_array, EmptyDescriptorArray)             \
  /* The roots above this line should be boring from a GC point of view.    */ \
  /* This means they are never in new space and never on a page that is     */ \
  /* being compacted.                                                       */ \
  V(Oddball, no_interceptor_result_sentinel, NoInterceptorResultSentinel)      \
  V(Oddball, arguments_marker, ArgumentsMarker)                                \
  V(Oddball, exception, Exception)                                             \
  V(Oddball, termination_exception, TerminationException)                      \
  V(FixedArray, number_string_cache, NumberStringCache)                        \
  V(Object, instanceof_cache_function, InstanceofCacheFunction)                \
  V(Object, instanceof_cache_map, InstanceofCacheMap)                          \
  V(Object, instanceof_cache_answer, InstanceofCacheAnswer)                    \
  V(FixedArray, single_character_string_cache, SingleCharacterStringCache)     \
  V(FixedArray, string_split_cache, StringSplitCache)                          \
  V(FixedArray, regexp_multiple_cache, RegExpMultipleCache)                    \
  V(Smi, hash_seed, HashSeed)                                                  \
  V(Map, hash_table_map, HashTableMap)                                         \
  V(Map, ordered_hash_table_map, OrderedHashTableMap)                          \
  V(Map, symbol_map, SymbolMap)                                                \
  V(Map, string_map, StringMap)                                                \
  V(Map, cons_one_byte_string_map, ConsOneByteStringMap)                       \
  V(Map, cons_string_map, ConsStringMap)                                       \
  V(Map, sliced_string_map, SlicedStringMap)                                   \
  V(Map, sliced_one_byte_string_map, SlicedOneByteStringMap)                   \
  V(Map, external_string_map, ExternalStringMap)                               \
  V(Map, external_string_with_one_byte_data_map,                               \
    ExternalStringWithOneByteDataMap)                                          \
  V(Map, external_one_byte_string_map, ExternalOneByteStringMap)               \
  V(Map, native_source_string_map, NativeSourceStringMap)                      \
  V(Map, short_external_string_map, ShortExternalStringMap)                    \
  V(Map, short_external_string_with_one_byte_data_map,                         \
    ShortExternalStringWithOneByteDataMap)                                     \
  V(Map, internalized_string_map, InternalizedStringMap)                       \
  V(Map, external_internalized_string_map, ExternalInternalizedStringMap)      \
  V(Map, external_internalized_string_with_one_byte_data_map,                  \
    ExternalInternalizedStringWithOneByteDataMap)                              \
  V(Map, external_one_byte_internalized_string_map,                            \
    ExternalOneByteInternalizedStringMap)                                      \
  V(Map, short_external_internalized_string_map,                               \
    ShortExternalInternalizedStringMap)                                        \
  V(Map, short_external_internalized_string_with_one_byte_data_map,            \
    ShortExternalInternalizedStringWithOneByteDataMap)                         \
  V(Map, short_external_one_byte_internalized_string_map,                      \
    ShortExternalOneByteInternalizedStringMap)                                 \
  V(Map, short_external_one_byte_string_map, ShortExternalOneByteStringMap)    \
  V(Map, fixed_uint8_array_map, FixedUint8ArrayMap)                            \
  V(Map, fixed_int8_array_map, FixedInt8ArrayMap)                              \
  V(Map, fixed_uint16_array_map, FixedUint16ArrayMap)                          \
  V(Map, fixed_int16_array_map, FixedInt16ArrayMap)                            \
  V(Map, fixed_uint32_array_map, FixedUint32ArrayMap)                          \
  V(Map, fixed_int32_array_map, FixedInt32ArrayMap)                            \
  V(Map, fixed_float32_array_map, FixedFloat32ArrayMap)                        \
  V(Map, fixed_float64_array_map, FixedFloat64ArrayMap)                        \
  V(Map, fixed_uint8_clamped_array_map, FixedUint8ClampedArrayMap)             \
  V(FixedTypedArrayBase, empty_fixed_uint8_array, EmptyFixedUint8Array)        \
  V(FixedTypedArrayBase, empty_fixed_int8_array, EmptyFixedInt8Array)          \
  V(FixedTypedArrayBase, empty_fixed_uint16_array, EmptyFixedUint16Array)      \
  V(FixedTypedArrayBase, empty_fixed_int16_array, EmptyFixedInt16Array)        \
  V(FixedTypedArrayBase, empty_fixed_uint32_array, EmptyFixedUint32Array)      \
  V(FixedTypedArrayBase, empty_fixed_int32_array, EmptyFixedInt32Array)        \
  V(FixedTypedArrayBase, empty_fixed_float32_array, EmptyFixedFloat32Array)    \
  V(FixedTypedArrayBase, empty_fixed_float64_array, EmptyFixedFloat64Array)    \
  V(FixedTypedArrayBase, empty_fixed_uint8_clamped_array,                      \
    EmptyFixedUint8ClampedArray)                                               \
  V(Map, sloppy_arguments_elements_map, SloppyArgumentsElementsMap)            \
  V(Map, catch_context_map, CatchContextMap)                                   \
  V(Map, with_context_map, WithContextMap)                                     \
  V(Map, block_context_map, BlockContextMap)                                   \
  V(Map, module_context_map, ModuleContextMap)                                 \
  V(Map, script_context_map, ScriptContextMap)                                 \
  V(Map, script_context_table_map, ScriptContextTableMap)                      \
  V(Map, undefined_map, UndefinedMap)                                          \
  V(Map, the_hole_map, TheHoleMap)                                             \
  V(Map, null_map, NullMap)                                                    \
  V(Map, boolean_map, BooleanMap)                                              \
  V(Map, uninitialized_map, UninitializedMap)                                  \
  V(Map, arguments_marker_map, ArgumentsMarkerMap)                             \
  V(Map, no_interceptor_result_sentinel_map, NoInterceptorResultSentinelMap)   \
  V(Map, exception_map, ExceptionMap)                                          \
  V(Map, termination_exception_map, TerminationExceptionMap)                   \
  V(Map, message_object_map, JSMessageObjectMap)                               \
  V(Map, foreign_map, ForeignMap)                                              \
  V(Map, neander_map, NeanderMap)                                              \
  V(Map, external_map, ExternalMap)                                            \
  V(HeapNumber, nan_value, NanValue)                                           \
  V(HeapNumber, infinity_value, InfinityValue)                                 \
  V(HeapNumber, minus_zero_value, MinusZeroValue)                              \
  V(HeapNumber, minus_infinity_value, MinusInfinityValue)                      \
  V(JSObject, message_listeners, MessageListeners)                             \
  V(UnseededNumberDictionary, code_stubs, CodeStubs)                           \
  V(UnseededNumberDictionary, non_monomorphic_cache, NonMonomorphicCache)      \
  V(PolymorphicCodeCache, polymorphic_code_cache, PolymorphicCodeCache)        \
  V(Code, js_entry_code, JsEntryCode)                                          \
  V(Code, js_construct_entry_code, JsConstructEntryCode)                       \
  V(FixedArray, natives_source_cache, NativesSourceCache)                      \
  V(FixedArray, experimental_natives_source_cache,                             \
    ExperimentalNativesSourceCache)                                            \
  V(FixedArray, extra_natives_source_cache, ExtraNativesSourceCache)           \
  V(FixedArray, experimental_extra_natives_source_cache,                       \
    ExperimentalExtraNativesSourceCache)                                       \
  V(Script, empty_script, EmptyScript)                                         \
  V(NameDictionary, intrinsic_function_names, IntrinsicFunctionNames)          \
  V(NameDictionary, empty_properties_dictionary, EmptyPropertiesDictionary)    \
  V(Cell, undefined_cell, UndefinedCell)                                       \
  V(JSObject, observation_state, ObservationState)                             \
  V(Object, symbol_registry, SymbolRegistry)                                   \
  V(Object, script_list, ScriptList)                                           \
  V(SeededNumberDictionary, empty_slow_element_dictionary,                     \
    EmptySlowElementDictionary)                                                \
  V(FixedArray, materialized_objects, MaterializedObjects)                     \
  V(FixedArray, microtask_queue, MicrotaskQueue)                               \
  V(TypeFeedbackVector, dummy_vector, DummyVector)                             \
  V(FixedArray, cleared_optimized_code_map, ClearedOptimizedCodeMap)           \
  V(FixedArray, detached_contexts, DetachedContexts)                           \
  V(ArrayList, retained_maps, RetainedMaps)                                    \
  V(WeakHashTable, weak_object_to_code_table, WeakObjectToCodeTable)           \
  V(PropertyCell, array_protector, ArrayProtector)                             \
  V(PropertyCell, empty_property_cell, EmptyPropertyCell)                      \
  V(Object, weak_stack_trace_list, WeakStackTraceList)                         \
  V(Object, noscript_shared_function_infos, NoScriptSharedFunctionInfos)       \
  V(FixedArray, interpreter_table, InterpreterTable)                           \
  V(Map, bytecode_array_map, BytecodeArrayMap)                                 \
  V(WeakCell, empty_weak_cell, EmptyWeakCell)                                  \
  V(BytecodeArray, empty_bytecode_array, EmptyBytecodeArray)


// Entries in this list are limited to Smis and are not visited during GC.
#define SMI_ROOT_LIST(V)                                                   \
  V(Smi, stack_limit, StackLimit)                                          \
  V(Smi, real_stack_limit, RealStackLimit)                                 \
  V(Smi, last_script_id, LastScriptId)                                     \
  V(Smi, arguments_adaptor_deopt_pc_offset, ArgumentsAdaptorDeoptPCOffset) \
  V(Smi, construct_stub_deopt_pc_offset, ConstructStubDeoptPCOffset)       \
  V(Smi, getter_stub_deopt_pc_offset, GetterStubDeoptPCOffset)             \
  V(Smi, setter_stub_deopt_pc_offset, SetterStubDeoptPCOffset)


#define ROOT_LIST(V)  \
  STRONG_ROOT_LIST(V) \
  SMI_ROOT_LIST(V)    \
  V(StringTable, string_table, StringTable)

#define INTERNALIZED_STRING_LIST(V)                              \
  V(anonymous_string, "anonymous")                               \
  V(apply_string, "apply")                                       \
  V(assign_string, "assign")                                     \
  V(arguments_string, "arguments")                               \
  V(Arguments_string, "Arguments")                               \
  V(Array_string, "Array")                                       \
  V(bind_string, "bind")                                         \
  V(bool16x8_string, "bool16x8")                                 \
  V(Bool16x8_string, "Bool16x8")                                 \
  V(bool32x4_string, "bool32x4")                                 \
  V(Bool32x4_string, "Bool32x4")                                 \
  V(bool8x16_string, "bool8x16")                                 \
  V(Bool8x16_string, "Bool8x16")                                 \
  V(boolean_string, "boolean")                                   \
  V(Boolean_string, "Boolean")                                   \
  V(bound__string, "bound ")                                     \
  V(byte_length_string, "byteLength")                            \
  V(byte_offset_string, "byteOffset")                            \
  V(call_string, "call")                                         \
  V(callee_string, "callee")                                     \
  V(caller_string, "caller")                                     \
  V(cell_value_string, "%cell_value")                            \
  V(char_at_string, "CharAt")                                    \
  V(closure_string, "(closure)")                                 \
  V(compare_ic_string, "==")                                     \
  V(configurable_string, "configurable")                         \
  V(constructor_string, "constructor")                           \
  V(construct_string, "construct")                               \
  V(create_string, "create")                                     \
  V(Date_string, "Date")                                         \
  V(default_string, "default")                                   \
  V(defineProperty_string, "defineProperty")                     \
  V(deleteProperty_string, "deleteProperty")                     \
  V(display_name_string, "displayName")                          \
  V(done_string, "done")                                         \
  V(dot_result_string, ".result")                                \
  V(dot_string, ".")                                             \
  V(enumerable_string, "enumerable")                             \
  V(enumerate_string, "enumerate")                               \
  V(Error_string, "Error")                                       \
  V(eval_string, "eval")                                         \
  V(false_string, "false")                                       \
  V(float32x4_string, "float32x4")                               \
  V(Float32x4_string, "Float32x4")                               \
  V(for_api_string, "for_api")                                   \
  V(for_string, "for")                                           \
  V(function_string, "function")                                 \
  V(Function_string, "Function")                                 \
  V(Generator_string, "Generator")                               \
  V(getOwnPropertyDescriptor_string, "getOwnPropertyDescriptor") \
  V(getPrototypeOf_string, "getPrototypeOf")                     \
  V(get_string, "get")                                           \
  V(global_string, "global")                                     \
  V(has_string, "has")                                           \
  V(illegal_access_string, "illegal access")                     \
  V(illegal_argument_string, "illegal argument")                 \
  V(index_string, "index")                                       \
  V(infinity_string, "Infinity")                                 \
  V(input_string, "input")                                       \
  V(int16x8_string, "int16x8")                                   \
  V(Int16x8_string, "Int16x8")                                   \
  V(int32x4_string, "int32x4")                                   \
  V(Int32x4_string, "Int32x4")                                   \
  V(int8x16_string, "int8x16")                                   \
  V(Int8x16_string, "Int8x16")                                   \
  V(isExtensible_string, "isExtensible")                         \
  V(isView_string, "isView")                                     \
  V(KeyedLoadMonomorphic_string, "KeyedLoadMonomorphic")         \
  V(KeyedStoreMonomorphic_string, "KeyedStoreMonomorphic")       \
  V(last_index_string, "lastIndex")                              \
  V(length_string, "length")                                     \
  V(Map_string, "Map")                                           \
  V(minus_infinity_string, "-Infinity")                          \
  V(minus_zero_string, "-0")                                     \
  V(name_string, "name")                                         \
  V(nan_string, "NaN")                                           \
  V(next_string, "next")                                         \
  V(null_string, "null")                                         \
  V(null_to_string, "[object Null]")                             \
  V(number_string, "number")                                     \
  V(Number_string, "Number")                                     \
  V(object_string, "object")                                     \
  V(Object_string, "Object")                                     \
  V(ownKeys_string, "ownKeys")                                   \
  V(preventExtensions_string, "preventExtensions")               \
  V(private_api_string, "private_api")                           \
  V(Promise_string, "Promise")                                   \
  V(proto_string, "__proto__")                                   \
  V(prototype_string, "prototype")                               \
  V(Proxy_string, "Proxy")                                       \
  V(query_colon_string, "(?:)")                                  \
  V(RegExp_string, "RegExp")                                     \
  V(setPrototypeOf_string, "setPrototypeOf")                     \
  V(set_string, "set")                                           \
  V(Set_string, "Set")                                           \
  V(source_mapping_url_string, "source_mapping_url")             \
  V(source_string, "source")                                     \
  V(source_url_string, "source_url")                             \
  V(stack_string, "stack")                                       \
  V(strict_compare_ic_string, "===")                             \
  V(string_string, "string")                                     \
  V(String_string, "String")                                     \
  V(symbol_string, "symbol")                                     \
  V(Symbol_string, "Symbol")                                     \
  V(this_string, "this")                                         \
  V(throw_string, "throw")                                       \
  V(toJSON_string, "toJSON")                                     \
  V(toString_string, "toString")                                 \
  V(true_string, "true")                                         \
  V(uint16x8_string, "uint16x8")                                 \
  V(Uint16x8_string, "Uint16x8")                                 \
  V(uint32x4_string, "uint32x4")                                 \
  V(Uint32x4_string, "Uint32x4")                                 \
  V(uint8x16_string, "uint8x16")                                 \
  V(Uint8x16_string, "Uint8x16")                                 \
  V(undefined_string, "undefined")                               \
  V(undefined_to_string, "[object Undefined]")                   \
  V(valueOf_string, "valueOf")                                   \
  V(value_string, "value")                                       \
  V(WeakMap_string, "WeakMap")                                   \
  V(WeakSet_string, "WeakSet")                                   \
  V(writable_string, "writable")

#define PRIVATE_SYMBOL_LIST(V)              \
  V(array_iteration_kind_symbol)            \
  V(array_iterator_next_symbol)             \
  V(array_iterator_object_symbol)           \
  V(call_site_function_symbol)              \
  V(call_site_position_symbol)              \
  V(call_site_receiver_symbol)              \
  V(call_site_strict_symbol)                \
  V(class_end_position_symbol)              \
  V(class_start_position_symbol)            \
  V(detailed_stack_trace_symbol)            \
  V(elements_transition_symbol)             \
  V(error_end_pos_symbol)                   \
  V(error_script_symbol)                    \
  V(error_start_pos_symbol)                 \
  V(formatted_stack_trace_symbol)           \
  V(frozen_symbol)                          \
  V(hash_code_symbol)                       \
  V(home_object_symbol)                     \
  V(internal_error_symbol)                  \
  V(intl_impl_object_symbol)                \
  V(intl_initialized_marker_symbol)         \
  V(intl_pattern_symbol)                    \
  V(intl_resolved_symbol)                   \
  V(megamorphic_symbol)                     \
  V(native_context_index_symbol)            \
  V(nonexistent_symbol)                     \
  V(nonextensible_symbol)                   \
  V(normal_ic_symbol)                       \
  V(not_mapped_symbol)                      \
  V(observed_symbol)                        \
  V(premonomorphic_symbol)                  \
  V(promise_combined_deferred_symbol)       \
  V(promise_debug_marker_symbol)            \
  V(promise_has_handler_symbol)             \
  V(promise_on_resolve_symbol)              \
  V(promise_on_reject_symbol)               \
  V(promise_raw_symbol)                     \
  V(promise_status_symbol)                  \
  V(promise_value_symbol)                   \
  V(sealed_symbol)                          \
  V(stack_trace_symbol)                     \
  V(strict_function_transition_symbol)      \
  V(string_iterator_iterated_string_symbol) \
  V(string_iterator_next_index_symbol)      \
  V(strong_function_transition_symbol)      \
  V(uninitialized_symbol)

#define PUBLIC_SYMBOL_LIST(V)                \
  V(has_instance_symbol, Symbol.hasInstance) \
  V(iterator_symbol, Symbol.iterator)        \
  V(match_symbol, Symbol.match)              \
  V(replace_symbol, Symbol.replace)          \
  V(search_symbol, Symbol.search)            \
  V(species_symbol, Symbol.species)          \
  V(split_symbol, Symbol.split)              \
  V(to_primitive_symbol, Symbol.toPrimitive) \
  V(unscopables_symbol, Symbol.unscopables)

// Well-Known Symbols are "Public" symbols, which have a bit set which causes
// them to produce an undefined value when a load results in a failed access
// check. Because this behaviour is not specified properly as of yet, it only
// applies to a subset of spec-defined Well-Known Symbols.
#define WELL_KNOWN_SYMBOL_LIST(V)                           \
  V(is_concat_spreadable_symbol, Symbol.isConcatSpreadable) \
  V(to_string_tag_symbol, Symbol.toStringTag)

// Heap roots that are known to be immortal immovable, for which we can safely
// skip write barriers. This list is not complete and has omissions.
#define IMMORTAL_IMMOVABLE_ROOT_LIST(V) \
  V(ByteArrayMap)                       \
  V(BytecodeArrayMap)                   \
  V(FreeSpaceMap)                       \
  V(OnePointerFillerMap)                \
  V(TwoPointerFillerMap)                \
  V(UndefinedValue)                     \
  V(TheHoleValue)                       \
  V(NullValue)                          \
  V(TrueValue)                          \
  V(FalseValue)                         \
  V(UninitializedValue)                 \
  V(CellMap)                            \
  V(GlobalPropertyCellMap)              \
  V(SharedFunctionInfoMap)              \
  V(MetaMap)                            \
  V(HeapNumberMap)                      \
  V(MutableHeapNumberMap)               \
  V(Float32x4Map)                       \
  V(Int32x4Map)                         \
  V(Uint32x4Map)                        \
  V(Bool32x4Map)                        \
  V(Int16x8Map)                         \
  V(Uint16x8Map)                        \
  V(Bool16x8Map)                        \
  V(Int8x16Map)                         \
  V(Uint8x16Map)                        \
  V(Bool8x16Map)                        \
  V(NativeContextMap)                   \
  V(FixedArrayMap)                      \
  V(CodeMap)                            \
  V(ScopeInfoMap)                       \
  V(FixedCOWArrayMap)                   \
  V(FixedDoubleArrayMap)                \
  V(WeakCellMap)                        \
  V(TransitionArrayMap)                 \
  V(NoInterceptorResultSentinel)        \
  V(HashTableMap)                       \
  V(OrderedHashTableMap)                \
  V(EmptyFixedArray)                    \
  V(EmptyByteArray)                     \
  V(EmptyBytecodeArray)                 \
  V(EmptyDescriptorArray)               \
  V(ArgumentsMarker)                    \
  V(SymbolMap)                          \
  V(SloppyArgumentsElementsMap)         \
  V(FunctionContextMap)                 \
  V(CatchContextMap)                    \
  V(WithContextMap)                     \
  V(BlockContextMap)                    \
  V(ModuleContextMap)                   \
  V(ScriptContextMap)                   \
  V(UndefinedMap)                       \
  V(TheHoleMap)                         \
  V(NullMap)                            \
  V(BooleanMap)                         \
  V(UninitializedMap)                   \
  V(ArgumentsMarkerMap)                 \
  V(JSMessageObjectMap)                 \
  V(ForeignMap)                         \
  V(NeanderMap)                         \
  V(EmptyWeakCell)                      \
  V(empty_string)                       \
  PRIVATE_SYMBOL_LIST(V)

// Forward declarations.
class ArrayBufferTracker;
class GCIdleTimeAction;
class GCIdleTimeHandler;
class GCIdleTimeHeapState;
class GCTracer;
class HeapObjectsFilter;
class HeapStats;
class HistogramTimer;
class Isolate;
class MemoryReducer;
class ObjectStats;
class Scavenger;
class ScavengeJob;
class WeakObjectRetainer;


// A queue of objects promoted during scavenge. Each object is accompanied
// by it's size to avoid dereferencing a map pointer for scanning.
// The last page in to-space is used for the promotion queue. On conflict
// during scavenge, the promotion queue is allocated externally and all
// entries are copied to the external queue.
class PromotionQueue {
 public:
  explicit PromotionQueue(Heap* heap)
      : front_(NULL),
        rear_(NULL),
        limit_(NULL),
        emergency_stack_(0),
        heap_(heap) {}

  void Initialize();

  void Destroy() {
    DCHECK(is_empty());
    delete emergency_stack_;
    emergency_stack_ = NULL;
  }

  Page* GetHeadPage() {
    return Page::FromAllocationTop(reinterpret_cast<Address>(rear_));
  }

  void SetNewLimit(Address limit) {
    // If we are already using an emergency stack, we can ignore it.
    if (emergency_stack_) return;

    // If the limit is not on the same page, we can ignore it.
    if (Page::FromAllocationTop(limit) != GetHeadPage()) return;

    limit_ = reinterpret_cast<intptr_t*>(limit);

    if (limit_ <= rear_) {
      return;
    }

    RelocateQueueHead();
  }

  bool IsBelowPromotionQueue(Address to_space_top) {
    // If an emergency stack is used, the to-space address cannot interfere
    // with the promotion queue.
    if (emergency_stack_) return true;

    // If the given to-space top pointer and the head of the promotion queue
    // are not on the same page, then the to-space objects are below the
    // promotion queue.
    if (GetHeadPage() != Page::FromAddress(to_space_top)) {
      return true;
    }
    // If the to space top pointer is smaller or equal than the promotion
    // queue head, then the to-space objects are below the promotion queue.
    return reinterpret_cast<intptr_t*>(to_space_top) <= rear_;
  }

  bool is_empty() {
    return (front_ == rear_) &&
           (emergency_stack_ == NULL || emergency_stack_->length() == 0);
  }

  inline void insert(HeapObject* target, int size);

  void remove(HeapObject** target, int* size) {
    DCHECK(!is_empty());
    if (front_ == rear_) {
      Entry e = emergency_stack_->RemoveLast();
      *target = e.obj_;
      *size = e.size_;
      return;
    }

    *target = reinterpret_cast<HeapObject*>(*(--front_));
    *size = static_cast<int>(*(--front_));
    // Assert no underflow.
    SemiSpace::AssertValidRange(reinterpret_cast<Address>(rear_),
                                reinterpret_cast<Address>(front_));
  }

 private:
  // The front of the queue is higher in the memory page chain than the rear.
  intptr_t* front_;
  intptr_t* rear_;
  intptr_t* limit_;

  static const int kEntrySizeInWords = 2;

  struct Entry {
    Entry(HeapObject* obj, int size) : obj_(obj), size_(size) {}

    HeapObject* obj_;
    int size_;
  };
  List<Entry>* emergency_stack_;

  Heap* heap_;

  void RelocateQueueHead();

  DISALLOW_COPY_AND_ASSIGN(PromotionQueue);
};


enum ArrayStorageAllocationMode {
  DONT_INITIALIZE_ARRAY_ELEMENTS,
  INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
};


class Heap {
 public:
  // Declare all the root indices.  This defines the root list order.
  enum RootListIndex {
#define ROOT_INDEX_DECLARATION(type, name, camel_name) k##camel_name##RootIndex,
    STRONG_ROOT_LIST(ROOT_INDEX_DECLARATION)
#undef ROOT_INDEX_DECLARATION

#define STRING_INDEX_DECLARATION(name, str) k##name##RootIndex,
        INTERNALIZED_STRING_LIST(STRING_INDEX_DECLARATION)
#undef STRING_DECLARATION

#define SYMBOL_INDEX_DECLARATION(name) k##name##RootIndex,
            PRIVATE_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
#undef SYMBOL_INDEX_DECLARATION

#define SYMBOL_INDEX_DECLARATION(name, description) k##name##RootIndex,
                PUBLIC_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
                    WELL_KNOWN_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
#undef SYMBOL_INDEX_DECLARATION

// Utility type maps
#define DECLARE_STRUCT_MAP(NAME, Name, name) k##Name##MapRootIndex,
                        STRUCT_LIST(DECLARE_STRUCT_MAP)
#undef DECLARE_STRUCT_MAP
                            kStringTableRootIndex,

#define ROOT_INDEX_DECLARATION(type, name, camel_name) k##camel_name##RootIndex,
    SMI_ROOT_LIST(ROOT_INDEX_DECLARATION)
#undef ROOT_INDEX_DECLARATION
        kRootListLength,
    kStrongRootListLength = kStringTableRootIndex,
    kSmiRootsStart = kStringTableRootIndex + 1
  };

  // Indicates whether live bytes adjustment is triggered
  // - from within the GC code before sweeping started (SEQUENTIAL_TO_SWEEPER),
  // - or from within GC (CONCURRENT_TO_SWEEPER),
  // - or mutator code (CONCURRENT_TO_SWEEPER).
  enum InvocationMode { SEQUENTIAL_TO_SWEEPER, CONCURRENT_TO_SWEEPER };

  enum PretenuringFeedbackInsertionMode { kCached, kGlobal };

  enum HeapState { NOT_IN_GC, SCAVENGE, MARK_COMPACT };

  // Taking this lock prevents the GC from entering a phase that relocates
  // object references.
  class RelocationLock {
   public:
    explicit RelocationLock(Heap* heap) : heap_(heap) {
      heap_->relocation_mutex_.Lock();
    }

    ~RelocationLock() { heap_->relocation_mutex_.Unlock(); }

   private:
    Heap* heap_;
  };

  // Support for partial snapshots.  After calling this we have a linear
  // space to write objects in each space.
  struct Chunk {
    uint32_t size;
    Address start;
    Address end;
  };
  typedef List<Chunk> Reservation;

  static const intptr_t kMinimumOldGenerationAllocationLimit =
      8 * (Page::kPageSize > MB ? Page::kPageSize : MB);

  static const int kInitalOldGenerationLimitFactor = 2;

#if V8_OS_ANDROID
  // Don't apply pointer multiplier on Android since it has no swap space and
  // should instead adapt it's heap size based on available physical memory.
  static const int kPointerMultiplier = 1;
#else
  static const int kPointerMultiplier = i::kPointerSize / 4;
#endif

  // The new space size has to be a power of 2. Sizes are in MB.
  static const int kMaxSemiSpaceSizeLowMemoryDevice = 1 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeMediumMemoryDevice = 4 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeHighMemoryDevice = 8 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeHugeMemoryDevice = 8 * kPointerMultiplier;

  // The old space size has to be a multiple of Page::kPageSize.
  // Sizes are in MB.
  static const int kMaxOldSpaceSizeLowMemoryDevice = 128 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeMediumMemoryDevice =
      256 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeHighMemoryDevice = 512 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeHugeMemoryDevice = 700 * kPointerMultiplier;

  // The executable size has to be a multiple of Page::kPageSize.
  // Sizes are in MB.
  static const int kMaxExecutableSizeLowMemoryDevice = 96 * kPointerMultiplier;
  static const int kMaxExecutableSizeMediumMemoryDevice =
      192 * kPointerMultiplier;
  static const int kMaxExecutableSizeHighMemoryDevice =
      256 * kPointerMultiplier;
  static const int kMaxExecutableSizeHugeMemoryDevice =
      256 * kPointerMultiplier;

  static const int kTraceRingBufferSize = 512;
  static const int kStacktraceBufferSize = 512;

  static const double kMinHeapGrowingFactor;
  static const double kMaxHeapGrowingFactor;
  static const double kMaxHeapGrowingFactorMemoryConstrained;
  static const double kMaxHeapGrowingFactorIdle;
  static const double kTargetMutatorUtilization;

  // Sloppy mode arguments object size.
  static const int kSloppyArgumentsObjectSize =
      JSObject::kHeaderSize + 2 * kPointerSize;

  // Strict mode arguments has no callee so it is smaller.
  static const int kStrictArgumentsObjectSize =
      JSObject::kHeaderSize + 1 * kPointerSize;

  // Indicies for direct access into argument objects.
  static const int kArgumentsLengthIndex = 0;

  // callee is only valid in sloppy mode.
  static const int kArgumentsCalleeIndex = 1;

  static const int kNoGCFlags = 0;
  static const int kReduceMemoryFootprintMask = 1;
  static const int kAbortIncrementalMarkingMask = 2;
  static const int kFinalizeIncrementalMarkingMask = 4;

  // Making the heap iterable requires us to abort incremental marking.
  static const int kMakeHeapIterableMask = kAbortIncrementalMarkingMask;

  // The roots that have an index less than this are always in old space.
  static const int kOldSpaceRoots = 0x20;

  // The minimum size of a HeapObject on the heap.
  static const int kMinObjectSizeInWords = 2;

  STATIC_ASSERT(kUndefinedValueRootIndex ==
                Internals::kUndefinedValueRootIndex);
  STATIC_ASSERT(kNullValueRootIndex == Internals::kNullValueRootIndex);
  STATIC_ASSERT(kTrueValueRootIndex == Internals::kTrueValueRootIndex);
  STATIC_ASSERT(kFalseValueRootIndex == Internals::kFalseValueRootIndex);
  STATIC_ASSERT(kempty_stringRootIndex == Internals::kEmptyStringRootIndex);

  // Calculates the maximum amount of filler that could be required by the
  // given alignment.
  static int GetMaximumFillToAlign(AllocationAlignment alignment);
  // Calculates the actual amount of filler required for a given address at the
  // given alignment.
  static int GetFillToAlign(Address address, AllocationAlignment alignment);

  template <typename T>
  static inline bool IsOneByte(T t, int chars);

  static void FatalProcessOutOfMemory(const char* location,
                                      bool take_snapshot = false);

  static bool RootIsImmortalImmovable(int root_index);

  // Checks whether the space is valid.
  static bool IsValidAllocationSpace(AllocationSpace space);

  // Generated code can embed direct references to non-writable roots if
  // they are in new space.
  static bool RootCanBeWrittenAfterInitialization(RootListIndex root_index);

  // Zapping is needed for verify heap, and always done in debug builds.
  static inline bool ShouldZapGarbage() {
#ifdef DEBUG
    return true;
#else
#ifdef VERIFY_HEAP
    return FLAG_verify_heap;
#else
    return false;
#endif
#endif
  }

  static double HeapGrowingFactor(double gc_speed, double mutator_speed);

  // Copy block of memory from src to dst. Size of block should be aligned
  // by pointer size.
  static inline void CopyBlock(Address dst, Address src, int byte_size);

  // Optimized version of memmove for blocks with pointer size aligned sizes and
  // pointer size aligned addresses.
  static inline void MoveBlock(Address dst, Address src, int byte_size);

  // Determines a static visitor id based on the given {map} that can then be
  // stored on the map to facilitate fast dispatch for {StaticVisitorBase}.
  static int GetStaticVisitorIdForMap(Map* map);

  // Notifies the heap that is ok to start marking or other activities that
  // should not happen during deserialization.
  void NotifyDeserializationComplete();

  intptr_t old_generation_allocation_limit() const {
    return old_generation_allocation_limit_;
  }

  bool always_allocate() { return always_allocate_scope_count_.Value() != 0; }

  Address* NewSpaceAllocationTopAddress() {
    return new_space_.allocation_top_address();
  }
  Address* NewSpaceAllocationLimitAddress() {
    return new_space_.allocation_limit_address();
  }

  Address* OldSpaceAllocationTopAddress() {
    return old_space_->allocation_top_address();
  }
  Address* OldSpaceAllocationLimitAddress() {
    return old_space_->allocation_limit_address();
  }

  // TODO(hpayer): There is still a missmatch between capacity and actual
  // committed memory size.
  bool CanExpandOldGeneration(int size = 0) {
    if (force_oom_) return false;
    return (CommittedOldGenerationMemory() + size) < MaxOldGenerationSize();
  }

  // Clear the Instanceof cache (used when a prototype changes).
  inline void ClearInstanceofCache();

  // FreeSpace objects have a null map after deserialization. Update the map.
  void RepairFreeListsAfterDeserialization();

  // Move len elements within a given array from src_index index to dst_index
  // index.
  void MoveElements(FixedArray* array, int dst_index, int src_index, int len);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages.
  void CreateFillerObjectAt(Address addr, int size);

  bool CanMoveObjectStart(HeapObject* object);

  // Maintain consistency of live bytes during incremental marking.
  void AdjustLiveBytes(HeapObject* object, int by, InvocationMode mode);

  // Trim the given array from the left. Note that this relocates the object
  // start and hence is only valid if there is only a single reference to it.
  FixedArrayBase* LeftTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Trim the given array from the right.
  template<Heap::InvocationMode mode>
  void RightTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Converts the given boolean condition to JavaScript boolean value.
  inline Object* ToBoolean(bool condition);

  // Check whether the heap is currently iterable.
  bool IsHeapIterable();

  // Notify the heap that a context has been disposed.
  int NotifyContextDisposed(bool dependant_context);

  inline void increment_scan_on_scavenge_pages() {
    scan_on_scavenge_pages_++;
    if (FLAG_gc_verbose) {
      PrintF("Scan-on-scavenge pages: %d\n", scan_on_scavenge_pages_);
    }
  }

  inline void decrement_scan_on_scavenge_pages() {
    scan_on_scavenge_pages_--;
    if (FLAG_gc_verbose) {
      PrintF("Scan-on-scavenge pages: %d\n", scan_on_scavenge_pages_);
    }
  }

  void set_native_contexts_list(Object* object) {
    native_contexts_list_ = object;
  }
  Object* native_contexts_list() const { return native_contexts_list_; }

  void set_allocation_sites_list(Object* object) {
    allocation_sites_list_ = object;
  }
  Object* allocation_sites_list() { return allocation_sites_list_; }

  // Used in CreateAllocationSiteStub and the (de)serializer.
  Object** allocation_sites_list_address() { return &allocation_sites_list_; }

  void set_encountered_weak_collections(Object* weak_collection) {
    encountered_weak_collections_ = weak_collection;
  }
  Object* encountered_weak_collections() const {
    return encountered_weak_collections_;
  }

  void set_encountered_weak_cells(Object* weak_cell) {
    encountered_weak_cells_ = weak_cell;
  }
  Object* encountered_weak_cells() const { return encountered_weak_cells_; }

  void set_encountered_transition_arrays(Object* transition_array) {
    encountered_transition_arrays_ = transition_array;
  }
  Object* encountered_transition_arrays() const {
    return encountered_transition_arrays_;
  }

  // Number of mark-sweeps.
  int ms_count() const { return ms_count_; }

  // Checks whether the given object is allowed to be migrated from it's
  // current space into the given destination space. Used for debugging.
  inline bool AllowedToBeMigrated(HeapObject* object, AllocationSpace dest);

  void CheckHandleCount();

  // Number of "runtime allocations" done so far.
  uint32_t allocations_count() { return allocations_count_; }

  // Print short heap statistics.
  void PrintShortHeapStatistics();

  inline HeapState gc_state() { return gc_state_; }

  inline bool IsInGCPostProcessing() { return gc_post_processing_depth_ > 0; }

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return NULL;
  inline AllocationMemento* FindAllocationMemento(HeapObject* object);

  // Returns false if not able to reserve.
  bool ReserveSpace(Reservation* reservations);

  //
  // Support for the API.
  //

  void CreateApiObjects();

  // Implements the corresponding V8 API function.
  bool IdleNotification(double deadline_in_seconds);
  bool IdleNotification(int idle_time_in_ms);

  double MonotonicallyIncreasingTimeInMs();

  void RecordStats(HeapStats* stats, bool take_snapshot = false);

  // Check new space expansion criteria and expand semispaces if it was hit.
  void CheckNewSpaceExpansionCriteria();

  inline bool HeapIsFullEnoughToStartIncrementalMarking(intptr_t limit) {
    if (FLAG_stress_compaction && (gc_count_ & 1) != 0) return true;

    intptr_t adjusted_allocation_limit = limit - new_space_.Capacity();

    if (PromotedTotalSize() >= adjusted_allocation_limit) return true;

    return false;
  }

  void VisitExternalResources(v8::ExternalResourceVisitor* visitor);

  // An object should be promoted if the object has survived a
  // scavenge operation.
  inline bool ShouldBePromoted(Address old_address, int object_size);

  void ClearNormalizedMapCaches();

  void IncrementDeferredCount(v8::Isolate::UseCounterFeature feature);

  inline bool OldGenerationAllocationLimitReached();

  void QueueMemoryChunkForFree(MemoryChunk* chunk);
  void FilterStoreBufferEntriesOnAboutToBeFreedPages();
  void FreeQueuedChunks(MemoryChunk* list_head);
  void FreeQueuedChunks();
  void WaitUntilUnmappingOfFreeChunksCompleted();

  // Completely clear the Instanceof cache (to stop it keeping objects alive
  // around a GC).
  inline void CompletelyClearInstanceofCache();

  inline uint32_t HashSeed();

  inline int NextScriptId();

  inline void SetArgumentsAdaptorDeoptPCOffset(int pc_offset);
  inline void SetConstructStubDeoptPCOffset(int pc_offset);
  inline void SetGetterStubDeoptPCOffset(int pc_offset);
  inline void SetSetterStubDeoptPCOffset(int pc_offset);

  // For post mortem debugging.
  void RememberUnmappedPage(Address page, bool compacted);

  // Global inline caching age: it is incremented on some GCs after context
  // disposal. We use it to flush inline caches.
  int global_ic_age() { return global_ic_age_; }

  void AgeInlineCaches() {
    global_ic_age_ = (global_ic_age_ + 1) & SharedFunctionInfo::ICAgeBits::kMax;
  }

  int64_t amount_of_external_allocated_memory() {
    return amount_of_external_allocated_memory_;
  }

  void update_amount_of_external_allocated_memory(int64_t delta) {
    amount_of_external_allocated_memory_ += delta;
  }

  void DeoptMarkedAllocationSites();

  bool DeoptMaybeTenuredAllocationSites() {
    return new_space_.IsAtMaximumCapacity() && maximum_size_scavenges_ == 0;
  }

  void AddWeakObjectToCodeDependency(Handle<HeapObject> obj,
                                     Handle<DependentCode> dep);

  DependentCode* LookupWeakObjectToCodeDependency(Handle<HeapObject> obj);

  void AddRetainedMap(Handle<Map> map);

  // This event is triggered after successful allocation of a new object made
  // by runtime. Allocations of target space for object evacuation do not
  // trigger the event. In order to track ALL allocations one must turn off
  // FLAG_inline_new and FLAG_use_allocation_folding.
  inline void OnAllocationEvent(HeapObject* object, int size_in_bytes);

  // This event is triggered after object is moved to a new place.
  inline void OnMoveEvent(HeapObject* target, HeapObject* source,
                          int size_in_bytes);

  bool deserialization_complete() const { return deserialization_complete_; }

  bool HasLowAllocationRate();
  bool HasHighFragmentation();
  bool HasHighFragmentation(intptr_t used, intptr_t committed);

  void SetOptimizeForLatency() { optimize_for_memory_usage_ = false; }
  void SetOptimizeForMemoryUsage() { optimize_for_memory_usage_ = true; }
  bool ShouldOptimizeForMemoryUsage() { return optimize_for_memory_usage_; }

  // ===========================================================================
  // Initialization. ===========================================================
  // ===========================================================================

  // Configure heap size in MB before setup. Return false if the heap has been
  // set up already.
  bool ConfigureHeap(int max_semi_space_size, int max_old_space_size,
                     int max_executable_size, size_t code_range_size);
  bool ConfigureHeapDefault();

  // Prepares the heap, setting up memory areas that are needed in the isolate
  // without actually creating any objects.
  bool SetUp();

  // Bootstraps the object heap with the core set of objects required to run.
  // Returns whether it succeeded.
  bool CreateHeapObjects();

  // Destroys all memory allocated by the heap.
  void TearDown();

  // Returns whether SetUp has been called.
  bool HasBeenSetUp();

  // ===========================================================================
  // Getters for spaces. =======================================================
  // ===========================================================================

  // Return the starting address and a mask for the new space.  And-masking an
  // address with the mask will result in the start address of the new space
  // for all addresses in either semispace.
  Address NewSpaceStart() { return new_space_.start(); }
  uintptr_t NewSpaceMask() { return new_space_.mask(); }
  Address NewSpaceTop() { return new_space_.top(); }

  NewSpace* new_space() { return &new_space_; }
  OldSpace* old_space() { return old_space_; }
  OldSpace* code_space() { return code_space_; }
  MapSpace* map_space() { return map_space_; }
  LargeObjectSpace* lo_space() { return lo_space_; }

  PagedSpace* paged_space(int idx) {
    switch (idx) {
      case OLD_SPACE:
        return old_space();
      case MAP_SPACE:
        return map_space();
      case CODE_SPACE:
        return code_space();
      case NEW_SPACE:
      case LO_SPACE:
        UNREACHABLE();
    }
    return NULL;
  }

  Space* space(int idx) {
    switch (idx) {
      case NEW_SPACE:
        return new_space();
      case LO_SPACE:
        return lo_space();
      default:
        return paged_space(idx);
    }
  }

  // Returns name of the space.
  const char* GetSpaceName(int idx);

  // ===========================================================================
  // Getters to other components. ==============================================
  // ===========================================================================

  GCTracer* tracer() { return tracer_; }

  PromotionQueue* promotion_queue() { return &promotion_queue_; }

  inline Isolate* isolate();

  MarkCompactCollector* mark_compact_collector() {
    return mark_compact_collector_;
  }

  // ===========================================================================
  // Root set access. ==========================================================
  // ===========================================================================

  // Heap root getters.
#define ROOT_ACCESSOR(type, name, camel_name) inline type* name();
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // Utility type maps.
#define STRUCT_MAP_ACCESSOR(NAME, Name, name) inline Map* name##_map();
  STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) inline String* name();
  INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) inline Symbol* name();
  PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) inline Symbol* name();
  PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
  WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

  Object* root(RootListIndex index) { return roots_[index]; }
  Handle<Object> root_handle(RootListIndex index) {
    return Handle<Object>(&roots_[index]);
  }

  // Generated code can embed this address to get access to the roots.
  Object** roots_array_start() { return roots_; }

  // Sets the stub_cache_ (only used when expanding the dictionary).
  void SetRootCodeStubs(UnseededNumberDictionary* value) {
    roots_[kCodeStubsRootIndex] = value;
  }

  // Sets the non_monomorphic_cache_ (only used when expanding the dictionary).
  void SetRootNonMonomorphicCache(UnseededNumberDictionary* value) {
    roots_[kNonMonomorphicCacheRootIndex] = value;
  }

  void SetRootMaterializedObjects(FixedArray* objects) {
    roots_[kMaterializedObjectsRootIndex] = objects;
  }

  void SetRootScriptList(Object* value) {
    roots_[kScriptListRootIndex] = value;
  }

  void SetRootStringTable(StringTable* value) {
    roots_[kStringTableRootIndex] = value;
  }

  void SetRootNoScriptSharedFunctionInfos(Object* value) {
    roots_[kNoScriptSharedFunctionInfosRootIndex] = value;
  }

  // Set the stack limit in the roots_ array.  Some architectures generate
  // code that looks here, because it is faster than loading from the static
  // jslimit_/real_jslimit_ variable in the StackGuard.
  void SetStackLimits();

  // Generated code can treat direct references to this root as constant.
  bool RootCanBeTreatedAsConstant(RootListIndex root_index);

  Map* MapForFixedTypedArray(ExternalArrayType array_type);
  RootListIndex RootIndexForFixedTypedArray(ExternalArrayType array_type);

  RootListIndex RootIndexForEmptyFixedTypedArray(ElementsKind kind);
  FixedTypedArrayBase* EmptyFixedTypedArrayForMap(Map* map);

  void RegisterStrongRoots(Object** start, Object** end);
  void UnregisterStrongRoots(Object** start);

  // ===========================================================================
  // Inline allocation. ========================================================
  // ===========================================================================

  // Indicates whether inline bump-pointer allocation has been disabled.
  bool inline_allocation_disabled() { return inline_allocation_disabled_; }

  // Switch whether inline bump-pointer allocation should be used.
  void EnableInlineAllocation();
  void DisableInlineAllocation();

  // ===========================================================================
  // Methods triggering GCs. ===================================================
  // ===========================================================================

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  inline bool CollectGarbage(
      AllocationSpace space, const char* gc_reason = NULL,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs a full garbage collection.  If (flags & kMakeHeapIterableMask) is
  // non-zero, then the slower precise sweeper is used, which leaves the heap
  // in a state where we can iterate over the heap visiting all objects.
  void CollectAllGarbage(
      int flags = kFinalizeIncrementalMarkingMask, const char* gc_reason = NULL,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Last hope GC, should try to squeeze as much as possible.
  void CollectAllAvailableGarbage(const char* gc_reason = NULL);

  // Reports and external memory pressure event, either performs a major GC or
  // completes incremental marking in order to free external resources.
  void ReportExternalMemoryPressure(const char* gc_reason = NULL);

  // Invoked when GC was requested via the stack guard.
  void HandleGCRequest();

  // ===========================================================================
  // Iterators. ================================================================
  // ===========================================================================

  // Iterates over all roots in the heap.
  void IterateRoots(ObjectVisitor* v, VisitMode mode);
  // Iterates over all strong roots in the heap.
  void IterateStrongRoots(ObjectVisitor* v, VisitMode mode);
  // Iterates over entries in the smi roots list.  Only interesting to the
  // serializer/deserializer, since GC does not care about smis.
  void IterateSmiRoots(ObjectVisitor* v);
  // Iterates over all the other roots in the heap.
  void IterateWeakRoots(ObjectVisitor* v, VisitMode mode);

  // Iterate pointers to from semispace of new space found in memory interval
  // from start to end within |object|.
  void IteratePointersToFromSpace(HeapObject* target, int size,
                                  ObjectSlotCallback callback);

  void IterateAndMarkPointersToFromSpace(HeapObject* object, Address start,
                                         Address end, bool record_slots,
                                         ObjectSlotCallback callback);

  // ===========================================================================
  // Store buffer API. =========================================================
  // ===========================================================================

  // Write barrier support for address[offset] = o.
  INLINE(void RecordWrite(Address address, int offset));

  // Write barrier support for address[start : start + len[ = o.
  INLINE(void RecordWrites(Address address, int start, int len));

  Address* store_buffer_top_address() {
    return reinterpret_cast<Address*>(&roots_[kStoreBufferTopRootIndex]);
  }

  // ===========================================================================
  // Incremental marking API. ==================================================
  // ===========================================================================

  // Start incremental marking and ensure that idle time handler can perform
  // incremental steps.
  void StartIdleIncrementalMarking();

  // Starts incremental marking assuming incremental marking is currently
  // stopped.
  void StartIncrementalMarking(int gc_flags = kNoGCFlags,
                               const GCCallbackFlags gc_callback_flags =
                                   GCCallbackFlags::kNoGCCallbackFlags,
                               const char* reason = nullptr);

  void FinalizeIncrementalMarkingIfComplete(const char* comment);

  bool TryFinalizeIdleIncrementalMarking(double idle_time_in_ms);

  IncrementalMarking* incremental_marking() { return incremental_marking_; }

  // ===========================================================================
  // External string table API. ================================================
  // ===========================================================================

  // Registers an external string.
  inline void RegisterExternalString(String* string);

  // Finalizes an external string by deleting the associated external
  // data and clearing the resource pointer.
  inline void FinalizeExternalString(String* string);

  // ===========================================================================
  // Methods checking/returning the space of a given object/address. ===========
  // ===========================================================================

  // Returns whether the object resides in new space.
  inline bool InNewSpace(Object* object);
  inline bool InNewSpace(Address address);
  inline bool InNewSpacePage(Address address);
  inline bool InFromSpace(Object* object);
  inline bool InToSpace(Object* object);

  // Returns whether the object resides in old space.
  inline bool InOldSpace(Address address);
  inline bool InOldSpace(Object* object);

  // Checks whether an address/object in the heap (including auxiliary
  // area and unused area).
  bool Contains(Address addr);
  bool Contains(HeapObject* value);

  // Checks whether an address/object in a space.
  // Currently used by tests, serialization and heap verification only.
  bool InSpace(Address addr, AllocationSpace space);
  bool InSpace(HeapObject* value, AllocationSpace space);

  // ===========================================================================
  // Object statistics tracking. ===============================================
  // ===========================================================================

  // Returns the number of buckets used by object statistics tracking during a
  // major GC. Note that the following methods fail gracefully when the bounds
  // are exceeded though.
  size_t NumberOfTrackedHeapObjectTypes();

  // Returns object statistics about count and size at the last major GC.
  // Objects are being grouped into buckets that roughly resemble existing
  // instance types.
  size_t ObjectCountAtLastGC(size_t index);
  size_t ObjectSizeAtLastGC(size_t index);

  // Retrieves names of buckets used by object statistics tracking.
  bool GetObjectTypeName(size_t index, const char** object_type,
                         const char** object_sub_type);

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  // Returns the maximum amount of memory reserved for the heap.  For
  // the young generation, we reserve 4 times the amount needed for a
  // semi space.  The young generation consists of two semi spaces and
  // we reserve twice the amount needed for those in order to ensure
  // that new space can be aligned to its size.
  intptr_t MaxReserved() {
    return 4 * reserved_semispace_size_ + max_old_generation_size_;
  }
  int MaxSemiSpaceSize() { return max_semi_space_size_; }
  int ReservedSemiSpaceSize() { return reserved_semispace_size_; }
  int InitialSemiSpaceSize() { return initial_semispace_size_; }
  int TargetSemiSpaceSize() { return target_semispace_size_; }
  intptr_t MaxOldGenerationSize() { return max_old_generation_size_; }
  intptr_t MaxExecutableSize() { return max_executable_size_; }

  // Returns the capacity of the heap in bytes w/o growing. Heap grows when
  // more spaces are needed until it reaches the limit.
  intptr_t Capacity();

  // Returns the amount of memory currently committed for the heap.
  intptr_t CommittedMemory();

  // Returns the amount of memory currently committed for the old space.
  intptr_t CommittedOldGenerationMemory();

  // Returns the amount of executable memory currently committed for the heap.
  intptr_t CommittedMemoryExecutable();

  // Returns the amount of phyical memory currently committed for the heap.
  size_t CommittedPhysicalMemory();

  // Returns the maximum amount of memory ever committed for the heap.
  intptr_t MaximumCommittedMemory() { return maximum_committed_; }

  // Updates the maximum committed memory for the heap. Should be called
  // whenever a space grows.
  void UpdateMaximumCommitted();

  // Returns the available bytes in space w/o growing.
  // Heap doesn't guarantee that it can allocate an object that requires
  // all available bytes. Check MaxHeapObjectSize() instead.
  intptr_t Available();

  // Returns of size of all objects residing in the heap.
  intptr_t SizeOfObjects();

  void UpdateSurvivalStatistics(int start_new_space_size);

  inline void IncrementPromotedObjectsSize(int object_size) {
    DCHECK_GE(object_size, 0);
    promoted_objects_size_ += object_size;
  }
  inline intptr_t promoted_objects_size() { return promoted_objects_size_; }

  inline void IncrementSemiSpaceCopiedObjectSize(int object_size) {
    DCHECK_GE(object_size, 0);
    semi_space_copied_object_size_ += object_size;
  }
  inline intptr_t semi_space_copied_object_size() {
    return semi_space_copied_object_size_;
  }

  inline intptr_t SurvivedNewSpaceObjectSize() {
    return promoted_objects_size_ + semi_space_copied_object_size_;
  }

  inline void IncrementNodesDiedInNewSpace() { nodes_died_in_new_space_++; }

  inline void IncrementNodesCopiedInNewSpace() { nodes_copied_in_new_space_++; }

  inline void IncrementNodesPromoted() { nodes_promoted_++; }

  inline void IncrementYoungSurvivorsCounter(int survived) {
    DCHECK(survived >= 0);
    survived_last_scavenge_ = survived;
    survived_since_last_expansion_ += survived;
  }

  inline intptr_t PromotedTotalSize() {
    int64_t total = PromotedSpaceSizeOfObjects() + PromotedExternalMemorySize();
    if (total > std::numeric_limits<intptr_t>::max()) {
      // TODO(erikcorry): Use uintptr_t everywhere we do heap size calculations.
      return std::numeric_limits<intptr_t>::max();
    }
    if (total < 0) return 0;
    return static_cast<intptr_t>(total);
  }

  void UpdateNewSpaceAllocationCounter() {
    new_space_allocation_counter_ = NewSpaceAllocationCounter();
  }

  size_t NewSpaceAllocationCounter() {
    return new_space_allocation_counter_ + new_space()->AllocatedSinceLastGC();
  }

  // This should be used only for testing.
  void set_new_space_allocation_counter(size_t new_value) {
    new_space_allocation_counter_ = new_value;
  }

  void UpdateOldGenerationAllocationCounter() {
    old_generation_allocation_counter_ = OldGenerationAllocationCounter();
  }

  size_t OldGenerationAllocationCounter() {
    return old_generation_allocation_counter_ + PromotedSinceLastGC();
  }

  // This should be used only for testing.
  void set_old_generation_allocation_counter(size_t new_value) {
    old_generation_allocation_counter_ = new_value;
  }

  size_t PromotedSinceLastGC() {
    return PromotedSpaceSizeOfObjects() - old_generation_size_at_last_gc_;
  }

  int gc_count() const { return gc_count_; }

  // Returns the size of objects residing in non new spaces.
  intptr_t PromotedSpaceSizeOfObjects();

  double total_regexp_code_generated() { return total_regexp_code_generated_; }
  void IncreaseTotalRegexpCodeGenerated(int size) {
    total_regexp_code_generated_ += size;
  }

  void IncrementCodeGeneratedBytes(bool is_crankshafted, int size) {
    if (is_crankshafted) {
      crankshaft_codegen_bytes_generated_ += size;
    } else {
      full_codegen_bytes_generated_ += size;
    }
  }

  // ===========================================================================
  // Prologue/epilogue callback methods.========================================
  // ===========================================================================

  void AddGCPrologueCallback(v8::Isolate::GCCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCPrologueCallback(v8::Isolate::GCCallback callback);

  void AddGCEpilogueCallback(v8::Isolate::GCCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCEpilogueCallback(v8::Isolate::GCCallback callback);

  void CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags);
  void CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags);

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Creates a filler object and returns a heap object immediately after it.
  MUST_USE_RESULT HeapObject* PrecedeWithFiller(HeapObject* object,
                                                int filler_size);

  // Creates a filler object if needed for alignment and returns a heap object
  // immediately after it. If any space is left after the returned object,
  // another filler object is created so the over allocated memory is iterable.
  MUST_USE_RESULT HeapObject* AlignWithFiller(HeapObject* object,
                                              int object_size,
                                              int allocation_size,
                                              AllocationAlignment alignment);

  // ===========================================================================
  // ArrayBuffer tracking. =====================================================
  // ===========================================================================

  void RegisterNewArrayBuffer(JSArrayBuffer* buffer);
  void UnregisterArrayBuffer(JSArrayBuffer* buffer);

  inline ArrayBufferTracker* array_buffer_tracker() {
    return array_buffer_tracker_;
  }

  // ===========================================================================
  // Allocation site tracking. =================================================
  // ===========================================================================

  // Updates the AllocationSite of a given {object}. If the global prenuring
  // storage is passed as {pretenuring_feedback} the memento found count on
  // the corresponding allocation site is immediately updated and an entry
  // in the hash map is created. Otherwise the entry (including a the count
  // value) is cached on the local pretenuring feedback.
  inline void UpdateAllocationSite(HeapObject* object,
                                   HashMap* pretenuring_feedback);

  // Removes an entry from the global pretenuring storage.
  inline void RemoveAllocationSitePretenuringFeedback(AllocationSite* site);

  // Merges local pretenuring feedback into the global one. Note that this
  // method needs to be called after evacuation, as allocation sites may be
  // evacuated and this method resolves forward pointers accordingly.
  void MergeAllocationSitePretenuringFeedback(
      const HashMap& local_pretenuring_feedback);

// =============================================================================

#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  void Verify();
#endif

#ifdef DEBUG
  void set_allocation_timeout(int timeout) { allocation_timeout_ = timeout; }

  void TracePathToObjectFrom(Object* target, Object* root);
  void TracePathToObject(Object* target);
  void TracePathToGlobal();

  void Print();
  void PrintHandles();

  // Report heap statistics.
  void ReportHeapStatistics(const char* title);
  void ReportCodeStatistics(const char* title);
#endif

 private:
  class PretenuringScope;
  class UnmapFreeMemoryTask;

  // External strings table is a place where all external strings are
  // registered.  We need to keep track of such strings to properly
  // finalize them.
  class ExternalStringTable {
   public:
    // Registers an external string.
    inline void AddString(String* string);

    inline void Iterate(ObjectVisitor* v);

    // Restores internal invariant and gets rid of collected strings.
    // Must be called after each Iterate() that modified the strings.
    void CleanUp();

    // Destroys all allocated memory.
    void TearDown();

   private:
    explicit ExternalStringTable(Heap* heap) : heap_(heap) {}

    inline void Verify();

    inline void AddOldString(String* string);

    // Notifies the table that only a prefix of the new list is valid.
    inline void ShrinkNewStrings(int position);

    // To speed up scavenge collections new space string are kept
    // separate from old space strings.
    List<Object*> new_space_strings_;
    List<Object*> old_space_strings_;

    Heap* heap_;

    friend class Heap;

    DISALLOW_COPY_AND_ASSIGN(ExternalStringTable);
  };

  struct StrongRootsList;

  struct StringTypeTable {
    InstanceType type;
    int size;
    RootListIndex index;
  };

  struct ConstantStringTable {
    const char* contents;
    RootListIndex index;
  };

  struct StructTable {
    InstanceType type;
    int size;
    RootListIndex index;
  };

  struct GCCallbackPair {
    GCCallbackPair(v8::Isolate::GCCallback callback, GCType gc_type,
                   bool pass_isolate)
        : callback(callback), gc_type(gc_type), pass_isolate(pass_isolate) {}

    bool operator==(const GCCallbackPair& other) const {
      return other.callback == callback;
    }

    v8::Isolate::GCCallback callback;
    GCType gc_type;
    bool pass_isolate;
  };

  typedef String* (*ExternalStringTableUpdaterCallback)(Heap* heap,
                                                        Object** pointer);

  static const int kInitialStringTableSize = 2048;
  static const int kInitialEvalCacheSize = 64;
  static const int kInitialNumberStringCacheSize = 256;

  static const int kRememberedUnmappedPages = 128;

  static const StringTypeTable string_type_table[];
  static const ConstantStringTable constant_string_table[];
  static const StructTable struct_table[];

  static const int kYoungSurvivalRateHighThreshold = 90;
  static const int kYoungSurvivalRateAllowedDeviation = 15;
  static const int kOldSurvivalRateLowThreshold = 10;

  static const int kMaxMarkCompactsInIdleRound = 7;
  static const int kIdleScavengeThreshold = 5;

  static const int kInitialFeedbackCapacity = 256;

  Heap();

  static String* UpdateNewSpaceReferenceInExternalStringTableEntry(
      Heap* heap, Object** pointer);

  static void ScavengeStoreBufferCallback(Heap* heap, MemoryChunk* page,
                                          StoreBufferEvent event);

  // Selects the proper allocation space based on the pretenuring decision.
  static AllocationSpace SelectSpace(PretenureFlag pretenure) {
    return (pretenure == TENURED) ? OLD_SPACE : NEW_SPACE;
  }

#define ROOT_ACCESSOR(type, name, camel_name) \
  inline void set_##name(type* value);
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  StoreBuffer* store_buffer() { return &store_buffer_; }

  void set_current_gc_flags(int flags) {
    current_gc_flags_ = flags;
    DCHECK(!ShouldFinalizeIncrementalMarking() ||
           !ShouldAbortIncrementalMarking());
  }

  inline bool ShouldReduceMemory() const {
    return current_gc_flags_ & kReduceMemoryFootprintMask;
  }

  inline bool ShouldAbortIncrementalMarking() const {
    return current_gc_flags_ & kAbortIncrementalMarkingMask;
  }

  inline bool ShouldFinalizeIncrementalMarking() const {
    return current_gc_flags_ & kFinalizeIncrementalMarkingMask;
  }

  void PreprocessStackTraces();

  // Checks whether a global GC is necessary
  GarbageCollector SelectGarbageCollector(AllocationSpace space,
                                          const char** reason);

  // Make sure there is a filler value behind the top of the new space
  // so that the GC does not confuse some unintialized/stale memory
  // with the allocation memento of the object at the top
  void EnsureFillerObjectAtTop();

  // Ensure that we have swept all spaces in such a way that we can iterate
  // over all objects.  May cause a GC.
  void MakeHeapIterable();

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  bool CollectGarbage(
      GarbageCollector collector, const char* gc_reason,
      const char* collector_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs garbage collection
  // Returns whether there is a chance another major GC could
  // collect more garbage.
  bool PerformGarbageCollection(
      GarbageCollector collector,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  inline void UpdateOldSpaceLimits();

  // Initializes a JSObject based on its map.
  void InitializeJSObjectFromMap(JSObject* obj, FixedArray* properties,
                                 Map* map);

  // Initializes JSObject body starting at given offset.
  void InitializeJSObjectBody(JSObject* obj, Map* map, int start_offset);

  void InitializeAllocationMemento(AllocationMemento* memento,
                                   AllocationSite* allocation_site);

  bool CreateInitialMaps();
  void CreateInitialObjects();

  // These five Create*EntryStub functions are here and forced to not be inlined
  // because of a gcc-4.4 bug that assigns wrong vtable entries.
  NO_INLINE(void CreateJSEntryStub());
  NO_INLINE(void CreateJSConstructEntryStub());

  void CreateFixedStubs();

  HeapObject* DoubleAlignForDeserialization(HeapObject* object, int size);

  // Commits from space if it is uncommitted.
  void EnsureFromSpaceIsCommitted();

  // Uncommit unused semi space.
  bool UncommitFromSpace() { return new_space_.UncommitFromSpace(); }

  // Fill in bogus values in from space
  void ZapFromSpace();

  // Deopts all code that contains allocation instruction which are tenured or
  // not tenured. Moreover it clears the pretenuring allocation site statistics.
  void ResetAllAllocationSitesDependentCode(PretenureFlag flag);

  // Evaluates local pretenuring for the old space and calls
  // ResetAllTenuredAllocationSitesDependentCode if too many objects died in
  // the old space.
  void EvaluateOldSpaceLocalPretenuring(uint64_t size_of_objects_before_gc);

  // Record statistics before and after garbage collection.
  void ReportStatisticsBeforeGC();
  void ReportStatisticsAfterGC();

  // Creates and installs the full-sized number string cache.
  int FullSizeNumberStringCacheLength();
  // Flush the number to string cache.
  void FlushNumberStringCache();

  // TODO(hpayer): Allocation site pretenuring may make this method obsolete.
  // Re-visit incremental marking heuristics.
  bool IsHighSurvivalRate() { return high_survival_rate_period_length_ > 0; }

  void ConfigureInitialOldGenerationSize();

  bool HasLowYoungGenerationAllocationRate();
  bool HasLowOldGenerationAllocationRate();
  double YoungGenerationMutatorUtilization();
  double OldGenerationMutatorUtilization();

  void ReduceNewSpaceSize();

  bool TryFinalizeIdleIncrementalMarking(
      double idle_time_in_ms, size_t size_of_objects,
      size_t mark_compact_speed_in_bytes_per_ms);

  GCIdleTimeHeapState ComputeHeapState();

  bool PerformIdleTimeAction(GCIdleTimeAction action,
                             GCIdleTimeHeapState heap_state,
                             double deadline_in_ms);

  void IdleNotificationEpilogue(GCIdleTimeAction action,
                                GCIdleTimeHeapState heap_state, double start_ms,
                                double deadline_in_ms);

  inline void UpdateAllocationsHash(HeapObject* object);
  inline void UpdateAllocationsHash(uint32_t value);
  void PrintAlloctionsHash();

  void AddToRingBuffer(const char* string);
  void GetFromRingBuffer(char* buffer);

  void CompactRetainedMaps(ArrayList* retained_maps);

  // Attempt to over-approximate the weak closure by marking object groups and
  // implicit references from global handles, but don't atomically complete
  // marking. If we continue to mark incrementally, we might have marked
  // objects that die later.
  void FinalizeIncrementalMarking(const char* gc_reason);

  // Returns the timer used for a given GC type.
  // - GCScavenger: young generation GC
  // - GCCompactor: full GC
  // - GCFinalzeMC: finalization of incremental full GC
  // - GCFinalizeMCReduceMemory: finalization of incremental full GC with
  // memory reduction
  HistogramTimer* GCTypeTimer(GarbageCollector collector);

  // ===========================================================================
  // Pretenuring. ==============================================================
  // ===========================================================================

  // Pretenuring decisions are made based on feedback collected during new space
  // evacuation. Note that between feedback collection and calling this method
  // object in old space must not move.
  void ProcessPretenuringFeedback();

  // ===========================================================================
  // Actual GC. ================================================================
  // ===========================================================================

  // Code that should be run before and after each GC.  Includes some
  // reporting/verification activities when compiled with DEBUG set.
  void GarbageCollectionPrologue();
  void GarbageCollectionEpilogue();

  // Performs a major collection in the whole heap.
  void MarkCompact();

  // Code to be run before and after mark-compact.
  void MarkCompactPrologue();
  void MarkCompactEpilogue();

  // Performs a minor collection in new generation.
  void Scavenge();

  Address DoScavenge(ObjectVisitor* scavenge_visitor, Address new_space_front);

  void UpdateNewSpaceReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void UpdateReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void ProcessAllWeakReferences(WeakObjectRetainer* retainer);
  void ProcessYoungWeakReferences(WeakObjectRetainer* retainer);
  void ProcessNativeContexts(WeakObjectRetainer* retainer);
  void ProcessAllocationSites(WeakObjectRetainer* retainer);

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  inline intptr_t OldGenerationSpaceAvailable() {
    return old_generation_allocation_limit_ - PromotedTotalSize();
  }

  // Returns maximum GC pause.
  double get_max_gc_pause() { return max_gc_pause_; }

  // Returns maximum size of objects alive after GC.
  intptr_t get_max_alive_after_gc() { return max_alive_after_gc_; }

  // Returns minimal interval between two subsequent collections.
  double get_min_in_mutator() { return min_in_mutator_; }

  // Update GC statistics that are tracked on the Heap.
  void UpdateCumulativeGCStatistics(double duration, double spent_in_mutator,
                                    double marking_time);

  bool MaximumSizeScavenge() { return maximum_size_scavenges_ > 0; }

  // ===========================================================================
  // Growing strategy. =========================================================
  // ===========================================================================

  // Decrease the allocation limit if the new limit based on the given
  // parameters is lower than the current limit.
  void DampenOldGenerationAllocationLimit(intptr_t old_gen_size,
                                          double gc_speed,
                                          double mutator_speed);


  // Calculates the allocation limit based on a given growing factor and a
  // given old generation size.
  intptr_t CalculateOldGenerationAllocationLimit(double factor,
                                                 intptr_t old_gen_size);

  // Sets the allocation limit to trigger the next full garbage collection.
  void SetOldGenerationAllocationLimit(intptr_t old_gen_size, double gc_speed,
                                       double mutator_speed);

  // ===========================================================================
  // Idle notification. ========================================================
  // ===========================================================================

  bool RecentIdleNotificationHappened();
  void ScheduleIdleScavengeIfNeeded(int bytes_allocated);

  // ===========================================================================
  // HeapIterator helpers. =====================================================
  // ===========================================================================

  void heap_iterator_start() { heap_iterator_depth_++; }

  void heap_iterator_end() { heap_iterator_depth_--; }

  bool in_heap_iterator() { return heap_iterator_depth_ > 0; }

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Returns a deep copy of the JavaScript object.
  // Properties and elements are copied too.
  // Optionally takes an AllocationSite to be appended in an AllocationMemento.
  MUST_USE_RESULT AllocationResult CopyJSObject(JSObject* source,
                                                AllocationSite* site = NULL);

  // Allocates a JS Map in the heap.
  MUST_USE_RESULT AllocationResult
  AllocateMap(InstanceType instance_type, int instance_size,
              ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND);

  // Allocates and initializes a new JavaScript object based on a
  // constructor.
  // If allocation_site is non-null, then a memento is emitted after the object
  // that points to the site.
  MUST_USE_RESULT AllocationResult AllocateJSObject(
      JSFunction* constructor, PretenureFlag pretenure = NOT_TENURED,
      AllocationSite* allocation_site = NULL);

  // Allocates and initializes a new JavaScript object based on a map.
  // Passing an allocation site means that a memento will be created that
  // points to the site.
  MUST_USE_RESULT AllocationResult
  AllocateJSObjectFromMap(Map* map, PretenureFlag pretenure = NOT_TENURED,
                          AllocationSite* allocation_site = NULL);

  // Allocates a HeapNumber from value.
  MUST_USE_RESULT AllocationResult
  AllocateHeapNumber(double value, MutableMode mode = IMMUTABLE,
                     PretenureFlag pretenure = NOT_TENURED);

// Allocates SIMD values from the given lane values.
#define SIMD_ALLOCATE_DECLARATION(TYPE, Type, type, lane_count, lane_type) \
  AllocationResult Allocate##Type(lane_type lanes[lane_count],             \
                                  PretenureFlag pretenure = NOT_TENURED);
  SIMD128_TYPES(SIMD_ALLOCATE_DECLARATION)
#undef SIMD_ALLOCATE_DECLARATION

  // Allocates a byte array of the specified length
  MUST_USE_RESULT AllocationResult
  AllocateByteArray(int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocates a bytecode array with given contents.
  MUST_USE_RESULT AllocationResult
  AllocateBytecodeArray(int length, const byte* raw_bytecodes, int frame_size,
                        int parameter_count, FixedArray* constant_pool);

  // Copy the code and scope info part of the code object, but insert
  // the provided data as the relocation information.
  MUST_USE_RESULT AllocationResult CopyCode(Code* code,
                                            Vector<byte> reloc_info);

  MUST_USE_RESULT AllocationResult CopyCode(Code* code);

  // Allocates a fixed array initialized with undefined values
  MUST_USE_RESULT AllocationResult
  AllocateFixedArray(int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocate an uninitialized object.  The memory is non-executable if the
  // hardware and OS allow.  This is the single choke-point for allocations
  // performed by the runtime and should not be bypassed (to extend this to
  // inlined allocations, use the Heap::DisableInlineAllocation() support).
  MUST_USE_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationSpace space,
      AllocationAlignment aligment = kWordAligned);

  // Allocates a heap object based on the map.
  MUST_USE_RESULT AllocationResult
      Allocate(Map* map, AllocationSpace space,
               AllocationSite* allocation_site = NULL);

  // Allocates a partial map for bootstrapping.
  MUST_USE_RESULT AllocationResult
      AllocatePartialMap(InstanceType instance_type, int instance_size);

  // Allocate a block of memory in the given space (filled with a filler).
  // Used as a fall-back for generated code when the space is full.
  MUST_USE_RESULT AllocationResult
      AllocateFillerObject(int size, bool double_align, AllocationSpace space);

  // Allocate an uninitialized fixed array.
  MUST_USE_RESULT AllocationResult
      AllocateRawFixedArray(int length, PretenureFlag pretenure);

  // Allocate an uninitialized fixed double array.
  MUST_USE_RESULT AllocationResult
      AllocateRawFixedDoubleArray(int length, PretenureFlag pretenure);

  // Allocate an initialized fixed array with the given filler value.
  MUST_USE_RESULT AllocationResult
      AllocateFixedArrayWithFiller(int length, PretenureFlag pretenure,
                                   Object* filler);

  // Allocate and partially initializes a String.  There are two String
  // encodings: one-byte and two-byte.  These functions allocate a string of
  // the given length and set its map and length fields.  The characters of
  // the string are uninitialized.
  MUST_USE_RESULT AllocationResult
      AllocateRawOneByteString(int length, PretenureFlag pretenure);
  MUST_USE_RESULT AllocationResult
      AllocateRawTwoByteString(int length, PretenureFlag pretenure);

  // Allocates an internalized string in old space based on the character
  // stream.
  MUST_USE_RESULT inline AllocationResult AllocateInternalizedStringFromUtf8(
      Vector<const char> str, int chars, uint32_t hash_field);

  MUST_USE_RESULT inline AllocationResult AllocateOneByteInternalizedString(
      Vector<const uint8_t> str, uint32_t hash_field);

  MUST_USE_RESULT inline AllocationResult AllocateTwoByteInternalizedString(
      Vector<const uc16> str, uint32_t hash_field);

  template <bool is_one_byte, typename T>
  MUST_USE_RESULT AllocationResult
      AllocateInternalizedStringImpl(T t, int chars, uint32_t hash_field);

  template <typename T>
  MUST_USE_RESULT inline AllocationResult AllocateInternalizedStringImpl(
      T t, int chars, uint32_t hash_field);

  // Allocates an uninitialized fixed array. It must be filled by the caller.
  MUST_USE_RESULT AllocationResult AllocateUninitializedFixedArray(int length);

  // Make a copy of src and return it.
  MUST_USE_RESULT inline AllocationResult CopyFixedArray(FixedArray* src);

  // Make a copy of src, also grow the copy, and return the copy.
  MUST_USE_RESULT AllocationResult
  CopyFixedArrayAndGrow(FixedArray* src, int grow_by, PretenureFlag pretenure);

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyFixedArrayWithMap(FixedArray* src, Map* map);

  // Make a copy of src and return it.
  MUST_USE_RESULT inline AllocationResult CopyFixedDoubleArray(
      FixedDoubleArray* src);

  // Computes a single character string where the character has code.
  // A cache is used for one-byte (Latin1) codes.
  MUST_USE_RESULT AllocationResult
      LookupSingleCharacterStringFromCode(uint16_t code);

  // Allocate a symbol in old space.
  MUST_USE_RESULT AllocationResult AllocateSymbol();

  // Allocates an external array of the specified length and type.
  MUST_USE_RESULT AllocationResult AllocateFixedTypedArrayWithExternalPointer(
      int length, ExternalArrayType array_type, void* external_pointer,
      PretenureFlag pretenure);

  // Allocates a fixed typed array of the specified length and type.
  MUST_USE_RESULT AllocationResult
  AllocateFixedTypedArray(int length, ExternalArrayType array_type,
                          bool initialize, PretenureFlag pretenure);

  // Make a copy of src and return it.
  MUST_USE_RESULT AllocationResult CopyAndTenureFixedCOWArray(FixedArray* src);

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyFixedDoubleArrayWithMap(FixedDoubleArray* src, Map* map);

  // Allocates a fixed double array with uninitialized values. Returns
  MUST_USE_RESULT AllocationResult AllocateUninitializedFixedDoubleArray(
      int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocate empty fixed array.
  MUST_USE_RESULT AllocationResult AllocateEmptyFixedArray();

  // Allocate empty fixed typed array of given type.
  MUST_USE_RESULT AllocationResult
      AllocateEmptyFixedTypedArray(ExternalArrayType array_type);

  // Allocate a tenured simple cell.
  MUST_USE_RESULT AllocationResult AllocateCell(Object* value);

  // Allocate a tenured JS global property cell initialized with the hole.
  MUST_USE_RESULT AllocationResult AllocatePropertyCell();

  MUST_USE_RESULT AllocationResult AllocateWeakCell(HeapObject* value);

  MUST_USE_RESULT AllocationResult AllocateTransitionArray(int capacity);

  // Allocates a new utility object in the old generation.
  MUST_USE_RESULT AllocationResult AllocateStruct(InstanceType type);

  // Allocates a new foreign object.
  MUST_USE_RESULT AllocationResult
      AllocateForeign(Address address, PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT AllocationResult
      AllocateCode(int object_size, bool immovable);

  MUST_USE_RESULT AllocationResult InternalizeStringWithKey(HashTableKey* key);

  MUST_USE_RESULT AllocationResult InternalizeString(String* str);

  // ===========================================================================

  void set_force_oom(bool value) { force_oom_ = value; }

  // The amount of external memory registered through the API kept alive
  // by global handles
  int64_t amount_of_external_allocated_memory_;

  // Caches the amount of external memory registered at the last global gc.
  int64_t amount_of_external_allocated_memory_at_last_global_gc_;

  // This can be calculated directly from a pointer to the heap; however, it is
  // more expedient to get at the isolate directly from within Heap methods.
  Isolate* isolate_;

  Object* roots_[kRootListLength];

  size_t code_range_size_;
  int reserved_semispace_size_;
  int max_semi_space_size_;
  int initial_semispace_size_;
  int target_semispace_size_;
  intptr_t max_old_generation_size_;
  intptr_t initial_old_generation_size_;
  bool old_generation_size_configured_;
  intptr_t max_executable_size_;
  intptr_t maximum_committed_;

  // For keeping track of how much data has survived
  // scavenge since last new space expansion.
  int survived_since_last_expansion_;

  // ... and since the last scavenge.
  int survived_last_scavenge_;

  // This is not the depth of nested AlwaysAllocateScope's but rather a single
  // count, as scopes can be acquired from multiple tasks (read: threads).
  AtomicNumber<size_t> always_allocate_scope_count_;

  // For keeping track of context disposals.
  int contexts_disposed_;

  // The length of the retained_maps array at the time of context disposal.
  // This separates maps in the retained_maps array that were created before
  // and after context disposal.
  int number_of_disposed_maps_;

  int global_ic_age_;

  int scan_on_scavenge_pages_;

  NewSpace new_space_;
  OldSpace* old_space_;
  OldSpace* code_space_;
  MapSpace* map_space_;
  LargeObjectSpace* lo_space_;
  HeapState gc_state_;
  int gc_post_processing_depth_;
  Address new_space_top_after_last_gc_;

  // Returns the amount of external memory registered since last global gc.
  int64_t PromotedExternalMemorySize();

  // How many "runtime allocations" happened.
  uint32_t allocations_count_;

  // Running hash over allocations performed.
  uint32_t raw_allocations_hash_;

  // How many mark-sweep collections happened.
  unsigned int ms_count_;

  // How many gc happened.
  unsigned int gc_count_;

  // For post mortem debugging.
  int remembered_unmapped_pages_index_;
  Address remembered_unmapped_pages_[kRememberedUnmappedPages];

#ifdef DEBUG
  // If the --gc-interval flag is set to a positive value, this
  // variable holds the value indicating the number of allocations
  // remain until the next failure and garbage collection.
  int allocation_timeout_;
#endif  // DEBUG

  // Limit that triggers a global GC on the next (normally caused) GC.  This
  // is checked when we have already decided to do a GC to help determine
  // which collector to invoke, before expanding a paged space in the old
  // generation and on every allocation in large object space.
  intptr_t old_generation_allocation_limit_;

  // Indicates that an allocation has failed in the old generation since the
  // last GC.
  bool old_gen_exhausted_;

  // Indicates that memory usage is more important than latency.
  // TODO(ulan): Merge it with memory reducer once chromium:490559 is fixed.
  bool optimize_for_memory_usage_;

  // Indicates that inline bump-pointer allocation has been globally disabled
  // for all spaces. This is used to disable allocations in generated code.
  bool inline_allocation_disabled_;

  // Weak list heads, threaded through the objects.
  // List heads are initialized lazily and contain the undefined_value at start.
  Object* native_contexts_list_;
  Object* allocation_sites_list_;

  // List of encountered weak collections (JSWeakMap and JSWeakSet) during
  // marking. It is initialized during marking, destroyed after marking and
  // contains Smi(0) while marking is not active.
  Object* encountered_weak_collections_;

  Object* encountered_weak_cells_;

  Object* encountered_transition_arrays_;

  StoreBufferRebuilder store_buffer_rebuilder_;

  List<GCCallbackPair> gc_epilogue_callbacks_;
  List<GCCallbackPair> gc_prologue_callbacks_;

  // Total RegExp code ever generated
  double total_regexp_code_generated_;

  int deferred_counters_[v8::Isolate::kUseCounterFeatureCount];

  GCTracer* tracer_;

  int high_survival_rate_period_length_;
  intptr_t promoted_objects_size_;
  double promotion_ratio_;
  double promotion_rate_;
  intptr_t semi_space_copied_object_size_;
  intptr_t previous_semi_space_copied_object_size_;
  double semi_space_copied_rate_;
  int nodes_died_in_new_space_;
  int nodes_copied_in_new_space_;
  int nodes_promoted_;

  // This is the pretenuring trigger for allocation sites that are in maybe
  // tenure state. When we switched to the maximum new space size we deoptimize
  // the code that belongs to the allocation site and derive the lifetime
  // of the allocation site.
  unsigned int maximum_size_scavenges_;

  // Maximum GC pause.
  double max_gc_pause_;

  // Total time spent in GC.
  double total_gc_time_ms_;

  // Maximum size of objects alive after GC.
  intptr_t max_alive_after_gc_;

  // Minimal interval between two subsequent collections.
  double min_in_mutator_;

  // Cumulative GC time spent in marking.
  double marking_time_;

  // Cumulative GC time spent in sweeping.
  double sweeping_time_;

  // Last time an idle notification happened.
  double last_idle_notification_time_;

  // Last time a garbage collection happened.
  double last_gc_time_;

  Scavenger* scavenge_collector_;

  MarkCompactCollector* mark_compact_collector_;

  StoreBuffer store_buffer_;

  IncrementalMarking* incremental_marking_;

  GCIdleTimeHandler* gc_idle_time_handler_;

  MemoryReducer* memory_reducer_;

  ObjectStats* object_stats_;

  ScavengeJob* scavenge_job_;

  InlineAllocationObserver* idle_scavenge_observer_;

  // These two counters are monotomically increasing and never reset.
  size_t full_codegen_bytes_generated_;
  size_t crankshaft_codegen_bytes_generated_;

  // This counter is increased before each GC and never reset.
  // To account for the bytes allocated since the last GC, use the
  // NewSpaceAllocationCounter() function.
  size_t new_space_allocation_counter_;

  // This counter is increased before each GC and never reset. To
  // account for the bytes allocated since the last GC, use the
  // OldGenerationAllocationCounter() function.
  size_t old_generation_allocation_counter_;

  // The size of objects in old generation after the last MarkCompact GC.
  size_t old_generation_size_at_last_gc_;

  // If the --deopt_every_n_garbage_collections flag is set to a positive value,
  // this variable holds the number of garbage collections since the last
  // deoptimization triggered by garbage collection.
  int gcs_since_last_deopt_;

  // The feedback storage is used to store allocation sites (keys) and how often
  // they have been visited (values) by finding a memento behind an object. The
  // storage is only alive temporary during a GC. The invariant is that all
  // pointers in this map are already fixed, i.e., they do not point to
  // forwarding pointers.
  HashMap* global_pretenuring_feedback_;

  char trace_ring_buffer_[kTraceRingBufferSize];
  // If it's not full then the data is from 0 to ring_buffer_end_.  If it's
  // full then the data is from ring_buffer_end_ to the end of the buffer and
  // from 0 to ring_buffer_end_.
  bool ring_buffer_full_;
  size_t ring_buffer_end_;

  // Shared state read by the scavenge collector and set by ScavengeObject.
  PromotionQueue promotion_queue_;

  // Flag is set when the heap has been configured.  The heap can be repeatedly
  // configured through the API until it is set up.
  bool configured_;

  // Currently set GC flags that are respected by all GC components.
  int current_gc_flags_;

  // Currently set GC callback flags that are used to pass information between
  // the embedder and V8's GC.
  GCCallbackFlags current_gc_callback_flags_;

  ExternalStringTable external_string_table_;

  MemoryChunk* chunks_queued_for_free_;

  size_t concurrent_unmapping_tasks_active_;

  base::Semaphore pending_unmapping_tasks_semaphore_;

  base::Mutex relocation_mutex_;

  int gc_callbacks_depth_;

  bool deserialization_complete_;

  StrongRootsList* strong_roots_list_;

  ArrayBufferTracker* array_buffer_tracker_;

  // The depth of HeapIterator nestings.
  int heap_iterator_depth_;

  // Used for testing purposes.
  bool force_oom_;

  // Classes in "heap" can be friends.
  friend class AlwaysAllocateScope;
  friend class GCCallbacksScope;
  friend class GCTracer;
  friend class HeapIterator;
  friend class IdleScavengeObserver;
  friend class IncrementalMarking;
  friend class IteratePointersToFromSpaceVisitor;
  friend class MarkCompactCollector;
  friend class MarkCompactMarkingVisitor;
  friend class NewSpace;
  friend class ObjectStatsVisitor;
  friend class Page;
  friend class Scavenger;
  friend class StoreBuffer;

  // The allocator interface.
  friend class Factory;

  // The Isolate constructs us.
  friend class Isolate;

  // Used in cctest.
  friend class HeapTester;

  DISALLOW_COPY_AND_ASSIGN(Heap);
};


class HeapStats {
 public:
  static const int kStartMarker = 0xDECADE00;
  static const int kEndMarker = 0xDECADE01;

  int* start_marker;                       //  0
  int* new_space_size;                     //  1
  int* new_space_capacity;                 //  2
  intptr_t* old_space_size;                //  3
  intptr_t* old_space_capacity;            //  4
  intptr_t* code_space_size;               //  5
  intptr_t* code_space_capacity;           //  6
  intptr_t* map_space_size;                //  7
  intptr_t* map_space_capacity;            //  8
  intptr_t* lo_space_size;                 //  9
  int* global_handle_count;                // 10
  int* weak_global_handle_count;           // 11
  int* pending_global_handle_count;        // 12
  int* near_death_global_handle_count;     // 13
  int* free_global_handle_count;           // 14
  intptr_t* memory_allocator_size;         // 15
  intptr_t* memory_allocator_capacity;     // 16
  int* objects_per_type;                   // 17
  int* size_per_type;                      // 18
  int* os_error;                           // 19
  char* last_few_messages;                 // 20
  char* js_stacktrace;                     // 21
  int* end_marker;                         // 22
};


class AlwaysAllocateScope {
 public:
  explicit inline AlwaysAllocateScope(Isolate* isolate);
  inline ~AlwaysAllocateScope();

 private:
  Heap* heap_;
};


// Visitor class to verify interior pointers in spaces that do not contain
// or care about intergenerational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitor {
 public:
  inline void VisitPointers(Object** start, Object** end) override;
};


// Verify that all objects are Smis.
class VerifySmisVisitor : public ObjectVisitor {
 public:
  inline void VisitPointers(Object** start, Object** end) override;
};


// Space iterator for iterating over all spaces of the heap.  Returns each space
// in turn, and null when it is done.
class AllSpaces BASE_EMBEDDED {
 public:
  explicit AllSpaces(Heap* heap) : heap_(heap), counter_(FIRST_SPACE) {}
  Space* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all old spaces of the heap: Old space
// and code space.  Returns each space in turn, and null when it is done.
class OldSpaces BASE_EMBEDDED {
 public:
  explicit OldSpaces(Heap* heap) : heap_(heap), counter_(OLD_SPACE) {}
  OldSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all the paged spaces of the heap: Map
// space, old space, code space and cell space.  Returns
// each space in turn, and null when it is done.
class PagedSpaces BASE_EMBEDDED {
 public:
  explicit PagedSpaces(Heap* heap) : heap_(heap), counter_(OLD_SPACE) {}
  PagedSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all spaces of the heap.
// For each space an object iterator is provided. The deallocation of the
// returned object iterators is handled by the space iterator.
class SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  virtual ~SpaceIterator();

  bool has_next();
  ObjectIterator* next();

 private:
  ObjectIterator* CreateIterator();

  Heap* heap_;
  int current_space_;         // from enum AllocationSpace.
  ObjectIterator* iterator_;  // object iterator for the current space.
};


// A HeapIterator provides iteration over the whole heap. It
// aggregates the specific iterators for the different spaces as
// these can only iterate over one space only.
//
// HeapIterator ensures there is no allocation during its lifetime
// (using an embedded DisallowHeapAllocation instance).
//
// HeapIterator can skip free list nodes (that is, de-allocated heap
// objects that still remain in the heap). As implementation of free
// nodes filtering uses GC marks, it can't be used during MS/MC GC
// phases. Also, it is forbidden to interrupt iteration in this mode,
// as this will leave heap objects marked (and thus, unusable).
class HeapIterator BASE_EMBEDDED {
 public:
  enum HeapObjectsFiltering { kNoFiltering, kFilterUnreachable };

  explicit HeapIterator(Heap* heap,
                        HeapObjectsFiltering filtering = kNoFiltering);
  ~HeapIterator();

  HeapObject* next();

 private:
  struct MakeHeapIterableHelper {
    explicit MakeHeapIterableHelper(Heap* heap) { heap->MakeHeapIterable(); }
  };

  HeapObject* NextObject();

  // The following two fields need to be declared in this order. Initialization
  // order guarantees that we first make the heap iterable (which may involve
  // allocations) and only then lock it down by not allowing further
  // allocations.
  MakeHeapIterableHelper make_heap_iterable_helper_;
  DisallowHeapAllocation no_heap_allocation_;

  Heap* heap_;
  HeapObjectsFiltering filtering_;
  HeapObjectsFilter* filter_;
  // Space iterator for iterating all the spaces.
  SpaceIterator* space_iterator_;
  // Object iterator for the space currently being iterated.
  ObjectIterator* object_iterator_;
};


// Cache for mapping (map, property name) into field offset.
// Cleared at startup and prior to mark sweep collection.
class KeyedLookupCache {
 public:
  // Lookup field offset for (map, name). If absent, -1 is returned.
  int Lookup(Handle<Map> map, Handle<Name> name);

  // Update an element in the cache.
  void Update(Handle<Map> map, Handle<Name> name, int field_offset);

  // Clear the cache.
  void Clear();

  static const int kLength = 256;
  static const int kCapacityMask = kLength - 1;
  static const int kMapHashShift = 5;
  static const int kHashMask = -4;  // Zero the last two bits.
  static const int kEntriesPerBucket = 4;
  static const int kEntryLength = 2;
  static const int kMapIndex = 0;
  static const int kKeyIndex = 1;
  static const int kNotFound = -1;

  // kEntriesPerBucket should be a power of 2.
  STATIC_ASSERT((kEntriesPerBucket & (kEntriesPerBucket - 1)) == 0);
  STATIC_ASSERT(kEntriesPerBucket == -kHashMask);

 private:
  KeyedLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].map = NULL;
      keys_[i].name = NULL;
      field_offsets_[i] = kNotFound;
    }
  }

  static inline int Hash(Handle<Map> map, Handle<Name> name);

  // Get the address of the keys and field_offsets arrays.  Used in
  // generated code to perform cache lookups.
  Address keys_address() { return reinterpret_cast<Address>(&keys_); }

  Address field_offsets_address() {
    return reinterpret_cast<Address>(&field_offsets_);
  }

  struct Key {
    Map* map;
    Name* name;
  };

  Key keys_[kLength];
  int field_offsets_[kLength];

  friend class ExternalReference;
  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(KeyedLookupCache);
};


// Cache for mapping (map, property name) into descriptor index.
// The cache contains both positive and negative results.
// Descriptor index equals kNotFound means the property is absent.
// Cleared at startup and prior to any gc.
class DescriptorLookupCache {
 public:
  // Lookup descriptor index for (map, name).
  // If absent, kAbsent is returned.
  inline int Lookup(Map* source, Name* name);

  // Update an element in the cache.
  inline void Update(Map* source, Name* name, int result);

  // Clear the cache.
  void Clear();

  static const int kAbsent = -2;

 private:
  DescriptorLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].source = NULL;
      keys_[i].name = NULL;
      results_[i] = kAbsent;
    }
  }

  static int Hash(Object* source, Name* name) {
    // Uses only lower 32 bits if pointers are larger.
    uint32_t source_hash =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(source)) >>
        kPointerSizeLog2;
    uint32_t name_hash =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name)) >>
        kPointerSizeLog2;
    return (source_hash ^ name_hash) % kLength;
  }

  static const int kLength = 64;
  struct Key {
    Map* source;
    Name* name;
  };

  Key keys_[kLength];
  int results_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(DescriptorLookupCache);
};


// Abstract base class for checking whether a weak object should be retained.
class WeakObjectRetainer {
 public:
  virtual ~WeakObjectRetainer() {}

  // Return whether this object should be retained. If NULL is returned the
  // object has no references. Otherwise the address of the retained object
  // should be returned as in some GC situations the object has been moved.
  virtual Object* RetainAs(Object* object) = 0;
};


#ifdef DEBUG
// Helper class for tracing paths to a search target Object from all roots.
// The TracePathFrom() method can be used to trace paths from a specific
// object to the search target object.
class PathTracer : public ObjectVisitor {
 public:
  enum WhatToFind {
    FIND_ALL,   // Will find all matches.
    FIND_FIRST  // Will stop the search after first match.
  };

  // Tags 0, 1, and 3 are used. Use 2 for marking visited HeapObject.
  static const int kMarkTag = 2;

  // For the WhatToFind arg, if FIND_FIRST is specified, tracing will stop
  // after the first match.  If FIND_ALL is specified, then tracing will be
  // done for all matches.
  PathTracer(Object* search_target, WhatToFind what_to_find,
             VisitMode visit_mode)
      : search_target_(search_target),
        found_target_(false),
        found_target_in_trace_(false),
        what_to_find_(what_to_find),
        visit_mode_(visit_mode),
        object_stack_(20),
        no_allocation() {}

  void VisitPointers(Object** start, Object** end) override;

  void Reset();
  void TracePathFrom(Object** root);

  bool found() const { return found_target_; }

  static Object* const kAnyGlobalObject;

 protected:
  class MarkVisitor;
  class UnmarkVisitor;

  void MarkRecursively(Object** p, MarkVisitor* mark_visitor);
  void UnmarkRecursively(Object** p, UnmarkVisitor* unmark_visitor);
  virtual void ProcessResults();

  Object* search_target_;
  bool found_target_;
  bool found_target_in_trace_;
  WhatToFind what_to_find_;
  VisitMode visit_mode_;
  List<Object*> object_stack_;

  DisallowHeapAllocation no_allocation;  // i.e. no gc allowed.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PathTracer);
};
#endif  // DEBUG
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_H_
