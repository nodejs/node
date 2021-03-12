// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CONTEXTS_H_
#define V8_OBJECTS_CONTEXTS_H_

#include "src/objects/fixed-array.h"
#include "src/objects/function-kind.h"
#include "src/objects/ordered-hash-table.h"
#include "src/objects/osr-optimized-code-cache.h"
#include "torque-generated/field-offsets.h"
// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSGlobalObject;
class JSGlobalProxy;
class MicrotaskQueue;
class NativeContext;
class RegExpMatchInfo;

enum ContextLookupFlags {
  FOLLOW_CONTEXT_CHAIN = 1 << 0,
  FOLLOW_PROTOTYPE_CHAIN = 1 << 1,

  DONT_FOLLOW_CHAINS = 0,
  FOLLOW_CHAINS = FOLLOW_CONTEXT_CHAIN | FOLLOW_PROTOTYPE_CHAIN,
};

// Heap-allocated activation contexts.
//
// Contexts are implemented as FixedArray-like objects having a fixed
// header with a set of common fields.
//
// Note: Context must have no virtual functions and Context objects
// must always be allocated via Heap::AllocateContext() or
// Factory::NewContext.

#define NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                     \
  V(GENERATOR_NEXT_INTERNAL, JSFunction, generator_next_internal) \
  V(ASYNC_MODULE_EVALUATE_INTERNAL, JSFunction,                   \
    async_module_evaluate_internal)                               \
  V(OBJECT_CREATE, JSFunction, object_create)                     \
  V(REFLECT_APPLY_INDEX, JSFunction, reflect_apply)               \
  V(REFLECT_CONSTRUCT_INDEX, JSFunction, reflect_construct)       \
  V(MATH_FLOOR_INDEX, JSFunction, math_floor)                     \
  V(MATH_POW_INDEX, JSFunction, math_pow)                         \
  V(PROMISE_INTERNAL_CONSTRUCTOR_INDEX, JSFunction,               \
    promise_internal_constructor)                                 \
  V(PROMISE_THEN_INDEX, JSFunction, promise_then)

#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSGlobalProxy, global_proxy_object)                    \
  /* TODO(ishell): Actually we store exactly EmbedderDataArray here but */     \
  /* it's already UBSan-fiendly and doesn't require a star... So declare */    \
  /* it as a HeapObject for now. */                                            \
  V(EMBEDDER_DATA_INDEX, HeapObject, embedder_data)                            \
  V(CONTINUATION_PRESERVED_EMBEDDER_DATA_INDEX, HeapObject,                    \
    continuation_preserved_embedder_data)                                      \
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                                        \
  /* Below is alpha-sorted */                                                  \
  V(ACCESSOR_PROPERTY_DESCRIPTOR_MAP_INDEX, Map,                               \
    accessor_property_descriptor_map)                                          \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings)    \
  V(ARRAY_BUFFER_FUN_INDEX, JSFunction, array_buffer_fun)                      \
  V(ARRAY_BUFFER_MAP_INDEX, Map, array_buffer_map)                             \
  V(ARRAY_BUFFER_NOINIT_FUN_INDEX, JSFunction, array_buffer_noinit_fun)        \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function)                          \
  V(ARRAY_JOIN_STACK_INDEX, HeapObject, array_join_stack)                      \
  V(ASYNC_FROM_SYNC_ITERATOR_MAP_INDEX, Map, async_from_sync_iterator_map)     \
  V(ASYNC_FUNCTION_FUNCTION_INDEX, JSFunction, async_function_constructor)     \
  V(ASYNC_FUNCTION_OBJECT_MAP_INDEX, Map, async_function_object_map)           \
  V(ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX, JSFunction,                       \
    async_generator_function_function)                                         \
  V(ATOMICS_OBJECT, JSObject, atomics_object)                                  \
  V(BIGINT_FUNCTION_INDEX, JSFunction, bigint_function)                        \
  V(BIGINT64_ARRAY_FUN_INDEX, JSFunction, bigint64_array_fun)                  \
  V(BIGUINT64_ARRAY_FUN_INDEX, JSFunction, biguint64_array_fun)                \
  V(BOOLEAN_FUNCTION_INDEX, JSFunction, boolean_function)                      \
  V(BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX, Map,                            \
    bound_function_with_constructor_map)                                       \
  V(BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX, Map,                         \
    bound_function_without_constructor_map)                                    \
  V(CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, JSFunction,                            \
    call_as_constructor_delegate)                                              \
  V(CALL_AS_FUNCTION_DELEGATE_INDEX, JSFunction, call_as_function_delegate)    \
  V(CALL_ASYNC_MODULE_FULFILLED, JSFunction, call_async_module_fulfilled)      \
  V(CALL_ASYNC_MODULE_REJECTED, JSFunction, call_async_module_rejected)        \
  V(CALLSITE_FUNCTION_INDEX, JSFunction, callsite_function)                    \
  V(CONTEXT_EXTENSION_FUNCTION_INDEX, JSFunction, context_extension_function)  \
  V(DATA_PROPERTY_DESCRIPTOR_MAP_INDEX, Map, data_property_descriptor_map)     \
  V(DATA_VIEW_FUN_INDEX, JSFunction, data_view_fun)                            \
  V(DATE_FUNCTION_INDEX, JSFunction, date_function)                            \
  V(DEBUG_CONTEXT_ID_INDEX, Object, debug_context_id)                          \
  V(EMPTY_FUNCTION_INDEX, JSFunction, empty_function)                          \
  V(ERROR_MESSAGE_FOR_CODE_GEN_FROM_STRINGS_INDEX, Object,                     \
    error_message_for_code_gen_from_strings)                                   \
  V(ERRORS_THROWN_INDEX, Smi, errors_thrown)                                   \
  V(EXTRAS_BINDING_OBJECT_INDEX, JSObject, extras_binding_object)              \
  V(FAST_ALIASED_ARGUMENTS_MAP_INDEX, Map, fast_aliased_arguments_map)         \
  V(FAST_TEMPLATE_INSTANTIATIONS_CACHE_INDEX, FixedArray,                      \
    fast_template_instantiations_cache)                                        \
  V(FLOAT32_ARRAY_FUN_INDEX, JSFunction, float32_array_fun)                    \
  V(FLOAT64_ARRAY_FUN_INDEX, JSFunction, float64_array_fun)                    \
  V(FUNCTION_FUNCTION_INDEX, JSFunction, function_function)                    \
  V(GENERATOR_FUNCTION_FUNCTION_INDEX, JSFunction,                             \
    generator_function_function)                                               \
  V(GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX, Map, generator_object_prototype_map) \
  V(ASYNC_GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX, Map,                           \
    async_generator_object_prototype_map)                                      \
  V(INITIAL_ARRAY_ITERATOR_MAP_INDEX, Map, initial_array_iterator_map)         \
  V(INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX, JSObject,                          \
    initial_array_iterator_prototype)                                          \
  V(INITIAL_ARRAY_PROTOTYPE_INDEX, JSObject, initial_array_prototype)          \
  V(INITIAL_ERROR_PROTOTYPE_INDEX, JSObject, initial_error_prototype)          \
  V(INITIAL_GENERATOR_PROTOTYPE_INDEX, JSObject, initial_generator_prototype)  \
  V(INITIAL_ASYNC_ITERATOR_PROTOTYPE_INDEX, JSObject,                          \
    initial_async_iterator_prototype)                                          \
  V(INITIAL_ASYNC_GENERATOR_PROTOTYPE_INDEX, JSObject,                         \
    initial_async_generator_prototype)                                         \
  V(INITIAL_ITERATOR_PROTOTYPE_INDEX, JSObject, initial_iterator_prototype)    \
  V(INITIAL_MAP_ITERATOR_PROTOTYPE_INDEX, JSObject,                            \
    initial_map_iterator_prototype)                                            \
  V(INITIAL_MAP_PROTOTYPE_MAP_INDEX, Map, initial_map_prototype_map)           \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype)        \
  V(INITIAL_SET_ITERATOR_PROTOTYPE_INDEX, JSObject,                            \
    initial_set_iterator_prototype)                                            \
  V(INITIAL_SET_PROTOTYPE_INDEX, JSObject, initial_set_prototype)              \
  V(INITIAL_SET_PROTOTYPE_MAP_INDEX, Map, initial_set_prototype_map)           \
  V(INITIAL_STRING_ITERATOR_MAP_INDEX, Map, initial_string_iterator_map)       \
  V(INITIAL_STRING_ITERATOR_PROTOTYPE_INDEX, JSObject,                         \
    initial_string_iterator_prototype)                                         \
  V(INITIAL_STRING_PROTOTYPE_INDEX, JSObject, initial_string_prototype)        \
  V(INITIAL_WEAKMAP_PROTOTYPE_MAP_INDEX, Map, initial_weakmap_prototype_map)   \
  V(INITIAL_WEAKSET_PROTOTYPE_MAP_INDEX, Map, initial_weakset_prototype_map)   \
  V(INT16_ARRAY_FUN_INDEX, JSFunction, int16_array_fun)                        \
  V(INT32_ARRAY_FUN_INDEX, JSFunction, int32_array_fun)                        \
  V(INT8_ARRAY_FUN_INDEX, JSFunction, int8_array_fun)                          \
  V(INTL_COLLATOR_FUNCTION_INDEX, JSFunction, intl_collator_function)          \
  V(INTL_DATE_TIME_FORMAT_FUNCTION_INDEX, JSFunction,                          \
    intl_date_time_format_function)                                            \
  V(INTL_DISPLAY_NAMES_FUNCTION_INDEX, JSFunction,                             \
    intl_display_names_function)                                               \
  V(INTL_NUMBER_FORMAT_FUNCTION_INDEX, JSFunction,                             \
    intl_number_format_function)                                               \
  V(INTL_LOCALE_FUNCTION_INDEX, JSFunction, intl_locale_function)              \
  V(INTL_LIST_FORMAT_FUNCTION_INDEX, JSFunction, intl_list_format_function)    \
  V(INTL_PLURAL_RULES_FUNCTION_INDEX, JSFunction, intl_plural_rules_function)  \
  V(INTL_RELATIVE_TIME_FORMAT_FUNCTION_INDEX, JSFunction,                      \
    intl_relative_time_format_function)                                        \
  V(INTL_SEGMENTER_FUNCTION_INDEX, JSFunction, intl_segmenter_function)        \
  V(INTL_SEGMENTS_MAP_INDEX, Map, intl_segments_map)                           \
  V(INTL_SEGMENT_ITERATOR_MAP_INDEX, Map, intl_segment_iterator_map)           \
  V(ITERATOR_RESULT_MAP_INDEX, Map, iterator_result_map)                       \
  V(JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX, Map,                               \
    js_array_packed_smi_elements_map)                                          \
  V(JS_ARRAY_HOLEY_SMI_ELEMENTS_MAP_INDEX, Map,                                \
    js_array_holey_smi_elements_map)                                           \
  V(JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX, Map, js_array_packed_elements_map)     \
  V(JS_ARRAY_HOLEY_ELEMENTS_MAP_INDEX, Map, js_array_holey_elements_map)       \
  V(JS_ARRAY_PACKED_DOUBLE_ELEMENTS_MAP_INDEX, Map,                            \
    js_array_packed_double_elements_map)                                       \
  V(JS_ARRAY_HOLEY_DOUBLE_ELEMENTS_MAP_INDEX, Map,                             \
    js_array_holey_double_elements_map)                                        \
  V(JS_MAP_FUN_INDEX, JSFunction, js_map_fun)                                  \
  V(JS_MAP_MAP_INDEX, Map, js_map_map)                                         \
  V(JS_MODULE_NAMESPACE_MAP, Map, js_module_namespace_map)                     \
  V(JS_SET_FUN_INDEX, JSFunction, js_set_fun)                                  \
  V(JS_SET_MAP_INDEX, Map, js_set_map)                                         \
  V(JS_WEAK_MAP_FUN_INDEX, JSFunction, js_weak_map_fun)                        \
  V(JS_WEAK_SET_FUN_INDEX, JSFunction, js_weak_set_fun)                        \
  V(JS_WEAK_REF_FUNCTION_INDEX, JSFunction, js_weak_ref_fun)                   \
  V(JS_FINALIZATION_REGISTRY_FUNCTION_INDEX, JSFunction,                       \
    js_finalization_registry_fun)                                              \
  /* Context maps */                                                           \
  V(NATIVE_CONTEXT_MAP_INDEX, Map, native_context_map)                         \
  V(FUNCTION_CONTEXT_MAP_INDEX, Map, function_context_map)                     \
  V(MODULE_CONTEXT_MAP_INDEX, Map, module_context_map)                         \
  V(EVAL_CONTEXT_MAP_INDEX, Map, eval_context_map)                             \
  V(SCRIPT_CONTEXT_MAP_INDEX, Map, script_context_map)                         \
  V(AWAIT_CONTEXT_MAP_INDEX, Map, await_context_map)                           \
  V(BLOCK_CONTEXT_MAP_INDEX, Map, block_context_map)                           \
  V(CATCH_CONTEXT_MAP_INDEX, Map, catch_context_map)                           \
  V(WITH_CONTEXT_MAP_INDEX, Map, with_context_map)                             \
  V(DEBUG_EVALUATE_CONTEXT_MAP_INDEX, Map, debug_evaluate_context_map)         \
  V(MAP_CACHE_INDEX, Object, map_cache)                                        \
  V(MAP_KEY_ITERATOR_MAP_INDEX, Map, map_key_iterator_map)                     \
  V(MAP_KEY_VALUE_ITERATOR_MAP_INDEX, Map, map_key_value_iterator_map)         \
  V(MAP_VALUE_ITERATOR_MAP_INDEX, Map, map_value_iterator_map)                 \
  V(MATH_RANDOM_INDEX_INDEX, Smi, math_random_index)                           \
  V(MATH_RANDOM_STATE_INDEX, ByteArray, math_random_state)                     \
  V(MATH_RANDOM_CACHE_INDEX, FixedDoubleArray, math_random_cache)              \
  V(MESSAGE_LISTENERS_INDEX, TemplateList, message_listeners)                  \
  V(NORMALIZED_MAP_CACHE_INDEX, Object, normalized_map_cache)                  \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function)                        \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
  V(OBJECT_FUNCTION_PROTOTYPE_MAP_INDEX, Map, object_function_prototype_map)   \
  V(PROXY_CALLABLE_MAP_INDEX, Map, proxy_callable_map)                         \
  V(PROXY_CONSTRUCTOR_MAP_INDEX, Map, proxy_constructor_map)                   \
  V(PROXY_FUNCTION_INDEX, JSFunction, proxy_function)                          \
  V(PROXY_MAP_INDEX, Map, proxy_map)                                           \
  V(PROXY_REVOCABLE_RESULT_MAP_INDEX, Map, proxy_revocable_result_map)         \
  V(PROMISE_PROTOTYPE_INDEX, JSObject, promise_prototype)                      \
  V(RECORDER_CONTEXT_ID, Object, recorder_context_id)                          \
  V(REGEXP_EXEC_FUNCTION_INDEX, JSFunction, regexp_exec_function)              \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function)                        \
  V(REGEXP_LAST_MATCH_INFO_INDEX, RegExpMatchInfo, regexp_last_match_info)     \
  V(REGEXP_MATCH_ALL_FUNCTION_INDEX, JSFunction, regexp_match_all_function)    \
  V(REGEXP_MATCH_FUNCTION_INDEX, JSFunction, regexp_match_function)            \
  V(REGEXP_PROTOTYPE_INDEX, JSObject, regexp_prototype)                        \
  V(REGEXP_PROTOTYPE_MAP_INDEX, Map, regexp_prototype_map)                     \
  V(REGEXP_REPLACE_FUNCTION_INDEX, JSFunction, regexp_replace_function)        \
  V(REGEXP_RESULT_MAP_INDEX, Map, regexp_result_map)                           \
  V(REGEXP_RESULT_WITH_INDICES_MAP_INDEX, Map, regexp_result_with_indices_map) \
  V(REGEXP_RESULT_INDICES_MAP_INDEX, Map, regexp_result_indices_map)           \
  V(REGEXP_SEARCH_FUNCTION_INDEX, JSFunction, regexp_search_function)          \
  V(REGEXP_SPLIT_FUNCTION_INDEX, JSFunction, regexp_split_function)            \
  V(INITIAL_REGEXP_STRING_ITERATOR_PROTOTYPE_MAP_INDEX, Map,                   \
    initial_regexp_string_iterator_prototype_map)                              \
  V(SCRIPT_CONTEXT_TABLE_INDEX, ScriptContextTable, script_context_table)      \
  V(SCRIPT_EXECUTION_CALLBACK_INDEX, Object, script_execution_callback)        \
  V(SECURITY_TOKEN_INDEX, Object, security_token)                              \
  V(SERIALIZED_OBJECTS, FixedArray, serialized_objects)                        \
  V(SET_VALUE_ITERATOR_MAP_INDEX, Map, set_value_iterator_map)                 \
  V(SET_KEY_VALUE_ITERATOR_MAP_INDEX, Map, set_key_value_iterator_map)         \
  V(SHARED_ARRAY_BUFFER_FUN_INDEX, JSFunction, shared_array_buffer_fun)        \
  V(SLOPPY_ARGUMENTS_MAP_INDEX, Map, sloppy_arguments_map)                     \
  V(SLOW_ALIASED_ARGUMENTS_MAP_INDEX, Map, slow_aliased_arguments_map)         \
  V(STRICT_ARGUMENTS_MAP_INDEX, Map, strict_arguments_map)                     \
  V(SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP, Map,                                  \
    slow_object_with_null_prototype_map)                                       \
  V(SLOW_OBJECT_WITH_OBJECT_PROTOTYPE_MAP, Map,                                \
    slow_object_with_object_prototype_map)                                     \
  V(SLOW_TEMPLATE_INSTANTIATIONS_CACHE_INDEX, SimpleNumberDictionary,          \
    slow_template_instantiations_cache)                                        \
  V(ATOMICS_WAITASYNC_PROMISES, OrderedHashSet, atomics_waitasync_promises)    \
  V(WASM_DEBUG_MAPS, FixedArray, wasm_debug_maps)                              \
  /* Fast Path Protectors */                                                   \
  V(REGEXP_SPECIES_PROTECTOR_INDEX, PropertyCell, regexp_species_protector)    \
  /* All *_FUNCTION_MAP_INDEX definitions used by Context::FunctionMapIndex */ \
  /* must remain together. */                                                  \
  V(SLOPPY_FUNCTION_MAP_INDEX, Map, sloppy_function_map)                       \
  V(SLOPPY_FUNCTION_WITH_NAME_MAP_INDEX, Map, sloppy_function_with_name_map)   \
  V(SLOPPY_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    sloppy_function_without_prototype_map)                                     \
  V(SLOPPY_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX, Map,                    \
    sloppy_function_with_readonly_prototype_map)                               \
  V(STRICT_FUNCTION_MAP_INDEX, Map, strict_function_map)                       \
  V(STRICT_FUNCTION_WITH_NAME_MAP_INDEX, Map, strict_function_with_name_map)   \
  V(STRICT_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX, Map,                    \
    strict_function_with_readonly_prototype_map)                               \
  V(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    strict_function_without_prototype_map)                                     \
  V(METHOD_WITH_NAME_MAP_INDEX, Map, method_with_name_map)                     \
  V(ASYNC_FUNCTION_MAP_INDEX, Map, async_function_map)                         \
  V(ASYNC_FUNCTION_WITH_NAME_MAP_INDEX, Map, async_function_with_name_map)     \
  V(GENERATOR_FUNCTION_MAP_INDEX, Map, generator_function_map)                 \
  V(GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX, Map,                               \
    generator_function_with_name_map)                                          \
  V(ASYNC_GENERATOR_FUNCTION_MAP_INDEX, Map, async_generator_function_map)     \
  V(ASYNC_GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX, Map,                         \
    async_generator_function_with_name_map)                                    \
  V(CLASS_FUNCTION_MAP_INDEX, Map, class_function_map)                         \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function)                        \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map)   \
  V(SYMBOL_FUNCTION_INDEX, JSFunction, symbol_function)                        \
  V(WASM_EXPORTED_FUNCTION_MAP_INDEX, Map, wasm_exported_function_map)         \
  V(WASM_EXCEPTION_CONSTRUCTOR_INDEX, JSFunction, wasm_exception_constructor)  \
  V(WASM_GLOBAL_CONSTRUCTOR_INDEX, JSFunction, wasm_global_constructor)        \
  V(WASM_INSTANCE_CONSTRUCTOR_INDEX, JSFunction, wasm_instance_constructor)    \
  V(WASM_MEMORY_CONSTRUCTOR_INDEX, JSFunction, wasm_memory_constructor)        \
  V(WASM_MODULE_CONSTRUCTOR_INDEX, JSFunction, wasm_module_constructor)        \
  V(WASM_TABLE_CONSTRUCTOR_INDEX, JSFunction, wasm_table_constructor)          \
  V(TEMPLATE_WEAKMAP_INDEX, HeapObject, template_weakmap)                      \
  V(TYPED_ARRAY_FUN_INDEX, JSFunction, typed_array_function)                   \
  V(TYPED_ARRAY_PROTOTYPE_INDEX, JSObject, typed_array_prototype)              \
  V(UINT16_ARRAY_FUN_INDEX, JSFunction, uint16_array_fun)                      \
  V(UINT32_ARRAY_FUN_INDEX, JSFunction, uint32_array_fun)                      \
  V(UINT8_ARRAY_FUN_INDEX, JSFunction, uint8_array_fun)                        \
  V(UINT8_CLAMPED_ARRAY_FUN_INDEX, JSFunction, uint8_clamped_array_fun)        \
  V(ARRAY_ENTRIES_ITERATOR_INDEX, JSFunction, array_entries_iterator)          \
  V(ARRAY_FOR_EACH_ITERATOR_INDEX, JSFunction, array_for_each_iterator)        \
  V(ARRAY_KEYS_ITERATOR_INDEX, JSFunction, array_keys_iterator)                \
  V(ARRAY_VALUES_ITERATOR_INDEX, JSFunction, array_values_iterator)            \
  V(ERROR_FUNCTION_INDEX, JSFunction, error_function)                          \
  V(ERROR_TO_STRING, JSFunction, error_to_string)                              \
  V(EVAL_ERROR_FUNCTION_INDEX, JSFunction, eval_error_function)                \
  V(AGGREGATE_ERROR_FUNCTION_INDEX, JSFunction, aggregate_error_function)      \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun)                        \
  V(GLOBAL_PROXY_FUNCTION_INDEX, JSFunction, global_proxy_function)            \
  V(MAP_DELETE_INDEX, JSFunction, map_delete)                                  \
  V(MAP_GET_INDEX, JSFunction, map_get)                                        \
  V(MAP_HAS_INDEX, JSFunction, map_has)                                        \
  V(MAP_SET_INDEX, JSFunction, map_set)                                        \
  V(FINALIZATION_REGISTRY_CLEANUP_SOME, JSFunction,                            \
    finalization_registry_cleanup_some)                                        \
  V(FUNCTION_HAS_INSTANCE_INDEX, JSFunction, function_has_instance)            \
  V(FUNCTION_TO_STRING_INDEX, JSFunction, function_to_string)                  \
  V(OBJECT_TO_STRING, JSFunction, object_to_string)                            \
  V(OBJECT_VALUE_OF_FUNCTION_INDEX, JSFunction, object_value_of_function)      \
  V(PROMISE_ALL_INDEX, JSFunction, promise_all)                                \
  V(PROMISE_ANY_INDEX, JSFunction, promise_any)                                \
  V(PROMISE_FUNCTION_INDEX, JSFunction, promise_function)                      \
  V(RANGE_ERROR_FUNCTION_INDEX, JSFunction, range_error_function)              \
  V(REFERENCE_ERROR_FUNCTION_INDEX, JSFunction, reference_error_function)      \
  V(SET_ADD_INDEX, JSFunction, set_add)                                        \
  V(SET_DELETE_INDEX, JSFunction, set_delete)                                  \
  V(SET_HAS_INDEX, JSFunction, set_has)                                        \
  V(SYNTAX_ERROR_FUNCTION_INDEX, JSFunction, syntax_error_function)            \
  V(TYPE_ERROR_FUNCTION_INDEX, JSFunction, type_error_function)                \
  V(URI_ERROR_FUNCTION_INDEX, JSFunction, uri_error_function)                  \
  V(WASM_COMPILE_ERROR_FUNCTION_INDEX, JSFunction,                             \
    wasm_compile_error_function)                                               \
  V(WASM_LINK_ERROR_FUNCTION_INDEX, JSFunction, wasm_link_error_function)      \
  V(WASM_RUNTIME_ERROR_FUNCTION_INDEX, JSFunction,                             \
    wasm_runtime_error_function)                                               \
  V(WEAKMAP_SET_INDEX, JSFunction, weakmap_set)                                \
  V(WEAKMAP_GET_INDEX, JSFunction, weakmap_get)                                \
  V(WEAKSET_ADD_INDEX, JSFunction, weakset_add)                                \
  V(RETAINED_MAPS, WeakArrayList, retained_maps)                               \
  V(OSR_CODE_CACHE_INDEX, WeakFixedArray, osr_code_cache)

#include "torque-generated/src/objects/contexts-tq.inc"

// A table of all script contexts. Every loaded top-level script with top-level
// lexical declarations contributes its ScriptContext into this table.
//
// The table is a fixed array, its first slot is the current used count and
// the subsequent slots 1..used contain ScriptContexts.
class ScriptContextTable : public FixedArray {
 public:
  DECL_CAST(ScriptContextTable)

  struct LookupResult {
    int context_index;
    int slot_index;
    VariableMode mode;
    InitializationFlag init_flag;
    MaybeAssignedFlag maybe_assigned_flag;
  };

  inline int synchronized_used() const;
  inline void synchronized_set_used(int used);

  static inline Handle<Context> GetContext(Isolate* isolate,
                                           Handle<ScriptContextTable> table,
                                           int i);
  inline Context get_context(int i) const;

  // Lookup a variable `name` in a ScriptContextTable.
  // If it returns true, the variable is found and `result` contains
  // valid information about its location.
  // If it returns false, `result` is untouched.
  V8_WARN_UNUSED_RESULT
  V8_EXPORT_PRIVATE static bool Lookup(Isolate* isolate,
                                       ScriptContextTable table, String name,
                                       LookupResult* result);

  V8_WARN_UNUSED_RESULT
  V8_EXPORT_PRIVATE static Handle<ScriptContextTable> Extend(
      Handle<ScriptContextTable> table, Handle<Context> script_context);

  static const int kUsedSlotIndex = 0;
  static const int kFirstContextSlotIndex = 1;
  static const int kMinLength = kFirstContextSlotIndex;

  OBJECT_CONSTRUCTORS(ScriptContextTable, FixedArray);
};

// JSFunctions are pairs (context, function code), sometimes also called
// closures. A Context object is used to represent function contexts and
// dynamically pushed 'with' contexts (or 'scopes' in ECMA-262 speak).
//
// At runtime, the contexts build a stack in parallel to the execution
// stack, with the top-most context being the current context. All contexts
// have the following slots:
//
// [ scope_info     ]  This is the scope info describing the current context. It
//                     contains the names of statically allocated context slots,
//                     and stack-allocated locals.  The names are needed for
//                     dynamic lookups in the presence of 'with' or 'eval', and
//                     for the debugger.
//
// [ previous       ]  A pointer to the previous context.
//
// [ extension      ]  Additional data. This slot is only available when
//                     ScopeInfo::HasContextExtensionSlot returns true.
//
//                     For native contexts, it contains the global object.
//                     For module contexts, it contains the module object.
//                     For await contexts, it contains the generator object.
//                     For var block contexts, it may contain an "extension
//                     object".
//                     For with contexts, it contains an "extension object".
//
//                     An "extension object" is used to dynamically extend a
//                     context with additional variables, namely in the
//                     implementation of the 'with' construct and the 'eval'
//                     construct.  For instance, Context::Lookup also searches
//                     the extension object for properties.  (Storing the
//                     extension object is the original purpose of this context
//                     slot, hence the name.)
//
// In addition, function contexts with sloppy eval may have statically
// allocated context slots to store local variables/functions that are accessed
// from inner functions (via static context addresses) or through 'eval'
// (dynamic context lookups).
// The native context contains additional slots for fast access to native
// properties.
//
// Finally, with Harmony scoping, the JSFunction representing a top level
// script will have the ScriptContext rather than a FunctionContext.
// Script contexts from all top-level scripts are gathered in
// ScriptContextTable.

class Context : public TorqueGeneratedContext<Context, HeapObject> {
 public:
  NEVER_READ_ONLY_SPACE

  // Setter and getter for elements.
  V8_INLINE Object get(int index) const;
  V8_INLINE Object get(IsolateRoot isolate, int index) const;
  V8_INLINE void set(int index, Object value);
  // Setter with explicit barrier mode.
  V8_INLINE void set(int index, Object value, WriteBarrierMode mode);
  // Setter and getter with synchronization semantics.
  V8_INLINE Object synchronized_get(int index) const;
  V8_INLINE Object synchronized_get(IsolateRoot isolate, int index) const;
  V8_INLINE void synchronized_set(int index, Object value);

  static const int kScopeInfoOffset = kElementsOffset;
  static const int kPreviousOffset = kScopeInfoOffset + kTaggedSize;

  /* Header size. */                                                  \
  /* TODO(ishell): use this as header size once MIN_CONTEXT_SLOTS */  \
  /* is removed in favour of offset-based access to common fields. */ \
  static const int kTodoHeaderSize = kPreviousOffset + kTaggedSize;

  // If the extension slot exists, it is the first slot after the header.
  static const int kExtensionOffset = kTodoHeaderSize;

  // Garbage collection support.
  V8_INLINE static constexpr int SizeFor(int length) {
    // TODO(v8:9287): This is a workaround for GCMole build failures.
    int result = kElementsOffset + length * kTaggedSize;
    CONSTEXPR_DCHECK(TorqueGeneratedContext::SizeFor(length) == result);
    return result;
  }

  // Code Generation support.
  // Offset of the element from the beginning of object.
  V8_INLINE static constexpr int OffsetOfElementAt(int index) {
    return SizeFor(index);
  }
  // Offset of the element from the heap object pointer.
  V8_INLINE static constexpr int SlotOffset(int index) {
    return OffsetOfElementAt(index) - kHeapObjectTag;
  }

  // Initializes the variable slots of the context. Lexical variables that need
  // initialization are filled with the hole.
  void Initialize(Isolate* isolate);

  // TODO(ishell): eventually migrate to the offset based access instead of
  // index-based.
  // The default context slot layout; indices are FixedArray slot indices.
  enum Field {
    // TODO(shell): use offset-based approach for accessing common values.
    // These slots are in all contexts.
    SCOPE_INFO_INDEX,
    PREVIOUS_INDEX,

    // This slot only exists if ScopeInfo::HasContextExtensionSlot returns true.
    EXTENSION_INDEX,

// These slots are only in native contexts.
#define NATIVE_CONTEXT_SLOT(index, type, name) index,
    NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_SLOT)
#undef NATIVE_CONTEXT_SLOT

    // Properties from here are treated as weak references by the full GC.
    // Scavenge treats them as strong references.
    OPTIMIZED_CODE_LIST,    // Weak.
    DEOPTIMIZED_CODE_LIST,  // Weak.
    NEXT_CONTEXT_LINK,      // Weak.

    // Total number of slots.
    NATIVE_CONTEXT_SLOTS,
    FIRST_WEAK_SLOT = OPTIMIZED_CODE_LIST,
    FIRST_JS_ARRAY_MAP_SLOT = JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX,

    // TODO(shell): Remove, once it becomes zero
    MIN_CONTEXT_SLOTS = EXTENSION_INDEX,
    MIN_CONTEXT_EXTENDED_SLOTS = EXTENSION_INDEX + 1,

    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,

    // These slots hold values in debug evaluate contexts.
    WRAPPED_CONTEXT_INDEX = MIN_CONTEXT_EXTENDED_SLOTS,
    BLOCK_LIST_INDEX = MIN_CONTEXT_EXTENDED_SLOTS + 1
  };

  static const int kExtensionSize =
      (MIN_CONTEXT_EXTENDED_SLOTS - MIN_CONTEXT_SLOTS) * kTaggedSize;
  static const int kExtendedHeaderSize = kTodoHeaderSize + kExtensionSize;

  // A region of native context entries containing maps for functions created
  // by Builtins::kFastNewClosure.
  static const int FIRST_FUNCTION_MAP_INDEX = SLOPPY_FUNCTION_MAP_INDEX;
  static const int LAST_FUNCTION_MAP_INDEX = CLASS_FUNCTION_MAP_INDEX;

  static const int kNoContext = 0;
  static const int kInvalidContext = 1;

  // Direct slot access.
  inline void set_scope_info(ScopeInfo scope_info);

  inline Object unchecked_previous();
  inline Context previous();
  inline void set_previous(Context context);

  inline Object next_context_link();

  inline bool has_extension();
  inline HeapObject extension();
  inline void set_extension(HeapObject object);
  JSObject extension_object();
  JSReceiver extension_receiver();
  V8_EXPORT_PRIVATE ScopeInfo scope_info();

  // Find the module context (assuming there is one) and return the associated
  // module object.
  SourceTextModule module();

  // Get the context where var declarations will be hoisted to, which
  // may be the context itself.
  Context declaration_context();
  bool is_declaration_context();

  // Get the next closure's context on the context chain.
  Context closure_context();

  // Returns a JSGlobalProxy object or null.
  V8_EXPORT_PRIVATE JSGlobalProxy global_proxy();

  // Get the JSGlobalObject object.
  V8_EXPORT_PRIVATE JSGlobalObject global_object();

  // Get the script context by traversing the context chain.
  Context script_context();

  // Compute the native context.
  inline NativeContext native_context() const;

  // Predicates for context types.  IsNativeContext is already defined on
  // Object.
  inline bool IsFunctionContext() const;
  inline bool IsCatchContext() const;
  inline bool IsWithContext() const;
  inline bool IsDebugEvaluateContext() const;
  inline bool IsAwaitContext() const;
  inline bool IsBlockContext() const;
  inline bool IsModuleContext() const;
  inline bool IsEvalContext() const;
  inline bool IsScriptContext() const;

  inline bool HasSameSecurityTokenAs(Context that) const;

  Handle<Object> ErrorMessageForCodeGenerationFromStrings();

  static int IntrinsicIndexForName(Handle<String> name);
  static int IntrinsicIndexForName(const unsigned char* name, int length);

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  inline void set_##name(type value);                     \
  inline bool is_##name(type value) const;                \
  inline type name() const;
  NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSORS)
#undef NATIVE_CONTEXT_FIELD_ACCESSORS

  // Lookup the slot called name, starting with the current context.
  // There are three possibilities:
  //
  // 1) result->IsContext():
  //    The binding was found in a context.  *index is always the
  //    non-negative slot index.  *attributes is NONE for var and let
  //    declarations, READ_ONLY for const declarations (never ABSENT).
  //
  // 2) result->IsJSObject():
  //    The binding was found as a named property in a context extension
  //    object (i.e., was introduced via eval), as a property on the subject
  //    of with, or as a property of the global object.  *index is -1 and
  //    *attributes is not ABSENT.
  //
  // 3) result->IsModule():
  //    The binding was found in module imports or exports.
  //     *attributes is never ABSENT. imports are READ_ONLY.
  //
  // 4) result.is_null():
  //    There was no binding found, *index is always -1 and *attributes is
  //    always ABSENT.
  static Handle<Object> Lookup(Handle<Context> context, Handle<String> name,
                               ContextLookupFlags flags, int* index,
                               PropertyAttributes* attributes,
                               InitializationFlag* init_flag,
                               VariableMode* variable_mode,
                               bool* is_sloppy_function_name = nullptr);

  static inline int FunctionMapIndex(LanguageMode language_mode,
                                     FunctionKind kind, bool has_shared_name);

  static int ArrayMapIndex(ElementsKind elements_kind) {
    DCHECK(IsFastElementsKind(elements_kind));
    return elements_kind + FIRST_JS_ARRAY_MAP_SLOT;
  }

  inline Map GetInitialJSArrayMap(ElementsKind kind) const;

  static const int kNotFound = -1;

  // Dispatched behavior.
  DECL_PRINTER(Context)
  DECL_VERIFIER(Context)

  class BodyDescriptor;

 private:
#ifdef DEBUG
  // Bootstrapping-aware type checks.
  static bool IsBootstrappingOrValidParentContext(Object object, Context kid);
#endif

  TQ_OBJECT_CONSTRUCTORS(Context)
};

class NativeContext : public Context {
 public:
  DECL_CAST(NativeContext)
  // TODO(neis): Move some stuff from Context here.

  inline void AllocateExternalPointerEntries(Isolate* isolate);

  // [microtask_queue]: pointer to the MicrotaskQueue object.
  DECL_GETTER(microtask_queue, MicrotaskQueue*)
  inline void set_microtask_queue(Isolate* isolate, MicrotaskQueue* queue);

  inline void synchronized_set_script_context_table(
      ScriptContextTable script_context_table);
  inline ScriptContextTable synchronized_script_context_table() const;

  // Dispatched behavior.
  DECL_PRINTER(NativeContext)
  DECL_VERIFIER(NativeContext)

  // Layout description.
#define NATIVE_CONTEXT_FIELDS_DEF(V)                                        \
  /* TODO(ishell): move definition of common context offsets to Context. */ \
  V(kStartOfNativeContextFieldsOffset,                                      \
    (FIRST_WEAK_SLOT - MIN_CONTEXT_EXTENDED_SLOTS) * kTaggedSize)           \
  V(kEndOfStrongFieldsOffset, 0)                                            \
  V(kStartOfWeakFieldsOffset,                                               \
    (NATIVE_CONTEXT_SLOTS - FIRST_WEAK_SLOT) * kTaggedSize)                 \
  V(kEndOfWeakFieldsOffset, 0)                                              \
  V(kEndOfNativeContextFieldsOffset, 0)                                     \
  V(kEndOfTaggedFieldsOffset, 0)                                            \
  /* Raw data. */                                                           \
  V(kMicrotaskQueueOffset, kSystemPointerSize)                              \
  /* Total size. */                                                         \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Context::kExtendedHeaderSize,
                                NATIVE_CONTEXT_FIELDS_DEF)
#undef NATIVE_CONTEXT_FIELDS_DEF

  class BodyDescriptor;

  // The native context stores a list of all optimized code and a list of all
  // deoptimized code, which are needed by the deoptimizer.
  V8_EXPORT_PRIVATE void AddOptimizedCode(Code code);
  void SetOptimizedCodeListHead(Object head);
  Object OptimizedCodeListHead();
  void SetDeoptimizedCodeListHead(Object head);
  Object DeoptimizedCodeListHead();

  inline OSROptimizedCodeCache GetOSROptimizedCodeCache();

  void ResetErrorsThrown();
  void IncrementErrorsThrown();
  int GetErrorsThrown();

 private:
  STATIC_ASSERT(OffsetOfElementAt(EMBEDDER_DATA_INDEX) ==
                Internals::kNativeContextEmbedderDataOffset);

  OBJECT_CONSTRUCTORS(NativeContext, Context);
};

using ContextField = Context::Field;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CONTEXTS_H_
