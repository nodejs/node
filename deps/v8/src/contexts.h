// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONTEXTS_H_
#define V8_CONTEXTS_H_

#include "src/heap/heap.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


enum ContextLookupFlags {
  FOLLOW_CONTEXT_CHAIN = 1 << 0,
  FOLLOW_PROTOTYPE_CHAIN = 1 << 1,
  STOP_AT_DECLARATION_SCOPE = 1 << 2,
  SKIP_WITH_CONTEXT = 1 << 3,

  DONT_FOLLOW_CHAINS = 0,
  FOLLOW_CHAINS = FOLLOW_CONTEXT_CHAIN | FOLLOW_PROTOTYPE_CHAIN,
  LEXICAL_TEST =
      FOLLOW_CONTEXT_CHAIN | STOP_AT_DECLARATION_SCOPE | SKIP_WITH_CONTEXT,
};


// Heap-allocated activation contexts.
//
// Contexts are implemented as FixedArray objects; the Context
// class is a convenience interface casted on a FixedArray object.
//
// Note: Context must have no virtual functions and Context objects
// must always be allocated via Heap::AllocateContext() or
// Factory::NewContext.

#define NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                           \
  V(IS_ARRAYLIKE, JSFunction, is_arraylike)                             \
  V(GENERATOR_NEXT_INTERNAL, JSFunction, generator_next_internal)       \
  V(GET_TEMPLATE_CALL_SITE_INDEX, JSFunction, get_template_call_site)   \
  V(MAKE_ERROR_INDEX, JSFunction, make_error)                           \
  V(MAKE_RANGE_ERROR_INDEX, JSFunction, make_range_error)               \
  V(MAKE_SYNTAX_ERROR_INDEX, JSFunction, make_syntax_error)             \
  V(MAKE_TYPE_ERROR_INDEX, JSFunction, make_type_error)                 \
  V(MAKE_URI_ERROR_INDEX, JSFunction, make_uri_error)                   \
  V(OBJECT_CREATE, JSFunction, object_create)                           \
  V(OBJECT_DEFINE_PROPERTIES, JSFunction, object_define_properties)     \
  V(OBJECT_DEFINE_PROPERTY, JSFunction, object_define_property)         \
  V(OBJECT_FREEZE, JSFunction, object_freeze)                           \
  V(OBJECT_GET_PROTOTYPE_OF, JSFunction, object_get_prototype_of)       \
  V(OBJECT_IS_EXTENSIBLE, JSFunction, object_is_extensible)             \
  V(OBJECT_IS_FROZEN, JSFunction, object_is_frozen)                     \
  V(OBJECT_IS_SEALED, JSFunction, object_is_sealed)                     \
  V(OBJECT_KEYS, JSFunction, object_keys)                               \
  V(REGEXP_INTERNAL_MATCH, JSFunction, regexp_internal_match)           \
  V(REFLECT_APPLY_INDEX, JSFunction, reflect_apply)                     \
  V(REFLECT_CONSTRUCT_INDEX, JSFunction, reflect_construct)             \
  V(REFLECT_DEFINE_PROPERTY_INDEX, JSFunction, reflect_define_property) \
  V(REFLECT_DELETE_PROPERTY_INDEX, JSFunction, reflect_delete_property) \
  V(SPREAD_ARGUMENTS_INDEX, JSFunction, spread_arguments)               \
  V(SPREAD_ITERABLE_INDEX, JSFunction, spread_iterable)                 \
  V(MATH_FLOOR_INDEX, JSFunction, math_floor)                           \
  V(MATH_POW_INDEX, JSFunction, math_pow)                               \
  V(NEW_PROMISE_CAPABILITY_INDEX, JSFunction, new_promise_capability)   \
  V(PROMISE_INTERNAL_CONSTRUCTOR_INDEX, JSFunction,                     \
    promise_internal_constructor)                                       \
  V(PROMISE_INTERNAL_REJECT_INDEX, JSFunction, promise_internal_reject) \
  V(IS_PROMISE_INDEX, JSFunction, is_promise)                           \
  V(PERFORM_PROMISE_THEN_INDEX, JSFunction, perform_promise_then)       \
  V(PROMISE_RESOLVE_INDEX, JSFunction, promise_resolve)                 \
  V(PROMISE_THEN_INDEX, JSFunction, promise_then)                       \
  V(PROMISE_HANDLE_INDEX, JSFunction, promise_handle)                   \
  V(PROMISE_HANDLE_REJECT_INDEX, JSFunction, promise_handle_reject)

#define NATIVE_CONTEXT_IMPORTED_FIELDS(V)                                     \
  V(ARRAY_CONCAT_INDEX, JSFunction, array_concat)                             \
  V(ARRAY_POP_INDEX, JSFunction, array_pop)                                   \
  V(ARRAY_PUSH_INDEX, JSFunction, array_push)                                 \
  V(ARRAY_SHIFT_INDEX, JSFunction, array_shift)                               \
  V(ARRAY_SPLICE_INDEX, JSFunction, array_splice)                             \
  V(ARRAY_SLICE_INDEX, JSFunction, array_slice)                               \
  V(ARRAY_UNSHIFT_INDEX, JSFunction, array_unshift)                           \
  V(ARRAY_VALUES_ITERATOR_INDEX, JSFunction, array_values_iterator)           \
  V(ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX, JSFunction,                            \
    async_function_await_caught)                                              \
  V(ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX, JSFunction,                          \
    async_function_await_uncaught)                                            \
  V(ASYNC_FUNCTION_PROMISE_CREATE_INDEX, JSFunction,                          \
    async_function_promise_create)                                            \
  V(ASYNC_FUNCTION_PROMISE_RELEASE_INDEX, JSFunction,                         \
    async_function_promise_release)                                           \
  V(DERIVED_GET_TRAP_INDEX, JSFunction, derived_get_trap)                     \
  V(ERROR_FUNCTION_INDEX, JSFunction, error_function)                         \
  V(ERROR_TO_STRING, JSFunction, error_to_string)                             \
  V(EVAL_ERROR_FUNCTION_INDEX, JSFunction, eval_error_function)               \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun)                       \
  V(GLOBAL_PROXY_FUNCTION_INDEX, JSFunction, global_proxy_function)           \
  V(MAP_DELETE_METHOD_INDEX, JSFunction, map_delete)                          \
  V(MAP_GET_METHOD_INDEX, JSFunction, map_get)                                \
  V(MAP_HAS_METHOD_INDEX, JSFunction, map_has)                                \
  V(MAP_SET_METHOD_INDEX, JSFunction, map_set)                                \
  V(FUNCTION_HAS_INSTANCE_INDEX, JSFunction, function_has_instance)           \
  V(OBJECT_VALUE_OF, JSFunction, object_value_of)                             \
  V(OBJECT_TO_STRING, JSFunction, object_to_string)                           \
  V(PROMISE_CATCH_INDEX, JSFunction, promise_catch)                           \
  V(PROMISE_FUNCTION_INDEX, JSFunction, promise_function)                     \
  V(PROMISE_ID_RESOLVE_HANDLER_INDEX, JSFunction, promise_id_resolve_handler) \
  V(PROMISE_ID_REJECT_HANDLER_INDEX, JSFunction, promise_id_reject_handler)   \
  V(RANGE_ERROR_FUNCTION_INDEX, JSFunction, range_error_function)             \
  V(REJECT_PROMISE_NO_DEBUG_EVENT_INDEX, JSFunction,                          \
    reject_promise_no_debug_event)                                            \
  V(REFERENCE_ERROR_FUNCTION_INDEX, JSFunction, reference_error_function)     \
  V(SET_ADD_METHOD_INDEX, JSFunction, set_add)                                \
  V(SET_DELETE_METHOD_INDEX, JSFunction, set_delete)                          \
  V(SET_HAS_METHOD_INDEX, JSFunction, set_has)                                \
  V(SYNTAX_ERROR_FUNCTION_INDEX, JSFunction, syntax_error_function)           \
  V(TYPE_ERROR_FUNCTION_INDEX, JSFunction, type_error_function)               \
  V(URI_ERROR_FUNCTION_INDEX, JSFunction, uri_error_function)                 \
  V(WASM_COMPILE_ERROR_FUNCTION_INDEX, JSFunction,                            \
    wasm_compile_error_function)                                              \
  V(WASM_LINK_ERROR_FUNCTION_INDEX, JSFunction, wasm_link_error_function)     \
  V(WASM_RUNTIME_ERROR_FUNCTION_INDEX, JSFunction, wasm_runtime_error_function)

#define NATIVE_CONTEXT_JS_ARRAY_ITERATOR_MAPS(V)                               \
  V(TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX, Map, typed_array_key_iterator_map)     \
  V(FAST_ARRAY_KEY_ITERATOR_MAP_INDEX, Map, fast_array_key_iterator_map)       \
  V(GENERIC_ARRAY_KEY_ITERATOR_MAP_INDEX, Map, array_key_iterator_map)         \
                                                                               \
  V(UINT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                             \
    uint8_array_key_value_iterator_map)                                        \
  V(INT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                              \
    int8_array_key_value_iterator_map)                                         \
  V(UINT16_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                            \
    uint16_array_key_value_iterator_map)                                       \
  V(INT16_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                             \
    int16_array_key_value_iterator_map)                                        \
  V(UINT32_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                            \
    uint32_array_key_value_iterator_map)                                       \
  V(INT32_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                             \
    int32_array_key_value_iterator_map)                                        \
  V(FLOAT32_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                           \
    float32_array_key_value_iterator_map)                                      \
  V(FLOAT64_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                           \
    float64_array_key_value_iterator_map)                                      \
  V(UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                     \
    uint8_clamped_array_key_value_iterator_map)                                \
                                                                               \
  V(FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                          \
    fast_smi_array_key_value_iterator_map)                                     \
  V(FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                    \
    fast_holey_smi_array_key_value_iterator_map)                               \
  V(FAST_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                              \
    fast_array_key_value_iterator_map)                                         \
  V(FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                        \
    fast_holey_array_key_value_iterator_map)                                   \
  V(FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                       \
    fast_double_array_key_value_iterator_map)                                  \
  V(FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                 \
    fast_holey_double_array_key_value_iterator_map)                            \
  V(GENERIC_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX, Map,                           \
    array_key_value_iterator_map)                                              \
                                                                               \
  V(UINT8_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, uint8_array_value_iterator_map) \
  V(INT8_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, int8_array_value_iterator_map)   \
  V(UINT16_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                                \
    uint16_array_value_iterator_map)                                           \
  V(INT16_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, int16_array_value_iterator_map) \
  V(UINT32_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                                \
    uint32_array_value_iterator_map)                                           \
  V(INT32_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, int32_array_value_iterator_map) \
  V(FLOAT32_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                               \
    float32_array_value_iterator_map)                                          \
  V(FLOAT64_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                               \
    float64_array_value_iterator_map)                                          \
  V(UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                         \
    uint8_clamped_array_value_iterator_map)                                    \
                                                                               \
  V(FAST_SMI_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                              \
    fast_smi_array_value_iterator_map)                                         \
  V(FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                        \
    fast_holey_smi_array_value_iterator_map)                                   \
  V(FAST_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, fast_array_value_iterator_map)   \
  V(FAST_HOLEY_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                            \
    fast_holey_array_value_iterator_map)                                       \
  V(FAST_DOUBLE_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                           \
    fast_double_array_value_iterator_map)                                      \
  V(FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map,                     \
    fast_holey_double_array_value_iterator_map)                                \
  V(GENERIC_ARRAY_VALUE_ITERATOR_MAP_INDEX, Map, array_value_iterator_map)

#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object)                         \
  V(EMBEDDER_DATA_INDEX, FixedArray, embedder_data)                            \
  /* Below is alpha-sorted */                                                  \
  V(ACCESSOR_PROPERTY_DESCRIPTOR_MAP_INDEX, Map,                               \
    accessor_property_descriptor_map)                                          \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings)    \
  V(ARRAY_BUFFER_FUN_INDEX, JSFunction, array_buffer_fun)                      \
  V(ARRAY_BUFFER_MAP_INDEX, Map, array_buffer_map)                             \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function)                          \
  V(ASYNC_FUNCTION_FUNCTION_INDEX, JSFunction, async_function_constructor)     \
  V(BOOL16X8_FUNCTION_INDEX, JSFunction, bool16x8_function)                    \
  V(BOOL32X4_FUNCTION_INDEX, JSFunction, bool32x4_function)                    \
  V(BOOL8X16_FUNCTION_INDEX, JSFunction, bool8x16_function)                    \
  V(BOOLEAN_FUNCTION_INDEX, JSFunction, boolean_function)                      \
  V(BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX, Map,                            \
    bound_function_with_constructor_map)                                       \
  V(BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX, Map,                         \
    bound_function_without_constructor_map)                                    \
  V(CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, JSFunction,                            \
    call_as_constructor_delegate)                                              \
  V(CALL_AS_FUNCTION_DELEGATE_INDEX, JSFunction, call_as_function_delegate)    \
  V(CALLSITE_FUNCTION_INDEX, JSFunction, callsite_function)                    \
  V(CONTEXT_EXTENSION_FUNCTION_INDEX, JSFunction, context_extension_function)  \
  V(CURRENT_MODULE_INDEX, Module, current_module)                              \
  V(DATA_PROPERTY_DESCRIPTOR_MAP_INDEX, Map, data_property_descriptor_map)     \
  V(DATA_VIEW_FUN_INDEX, JSFunction, data_view_fun)                            \
  V(DATE_FUNCTION_INDEX, JSFunction, date_function)                            \
  V(ERROR_MESSAGE_FOR_CODE_GEN_FROM_STRINGS_INDEX, Object,                     \
    error_message_for_code_gen_from_strings)                                   \
  V(ERRORS_THROWN_INDEX, Smi, errors_thrown)                                   \
  V(EXTRAS_EXPORTS_OBJECT_INDEX, JSObject, extras_binding_object)              \
  V(EXTRAS_UTILS_OBJECT_INDEX, JSObject, extras_utils_object)                  \
  V(FAST_ALIASED_ARGUMENTS_MAP_INDEX, Map, fast_aliased_arguments_map)         \
  V(FAST_TEMPLATE_INSTANTIATIONS_CACHE_INDEX, FixedArray,                      \
    fast_template_instantiations_cache)                                        \
  V(FLOAT32_ARRAY_FUN_INDEX, JSFunction, float32_array_fun)                    \
  V(FLOAT32X4_FUNCTION_INDEX, JSFunction, float32x4_function)                  \
  V(FLOAT64_ARRAY_FUN_INDEX, JSFunction, float64_array_fun)                    \
  V(FUNCTION_FUNCTION_INDEX, JSFunction, function_function)                    \
  V(GENERATOR_FUNCTION_FUNCTION_INDEX, JSFunction,                             \
    generator_function_function)                                               \
  V(GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX, Map, generator_object_prototype_map) \
  V(INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX, JSObject,                          \
    initial_array_iterator_prototype)                                          \
  V(INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX, Map,                           \
    initial_array_iterator_prototype_map)                                      \
  V(INITIAL_ARRAY_PROTOTYPE_INDEX, JSObject, initial_array_prototype)          \
  V(INITIAL_GENERATOR_PROTOTYPE_INDEX, JSObject, initial_generator_prototype)  \
  V(INITIAL_ITERATOR_PROTOTYPE_INDEX, JSObject, initial_iterator_prototype)    \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype)        \
  V(INT16_ARRAY_FUN_INDEX, JSFunction, int16_array_fun)                        \
  V(INT16X8_FUNCTION_INDEX, JSFunction, int16x8_function)                      \
  V(INT32_ARRAY_FUN_INDEX, JSFunction, int32_array_fun)                        \
  V(INT32X4_FUNCTION_INDEX, JSFunction, int32x4_function)                      \
  V(INT8_ARRAY_FUN_INDEX, JSFunction, int8_array_fun)                          \
  V(INT8X16_FUNCTION_INDEX, JSFunction, int8x16_function)                      \
  V(INTERNAL_ARRAY_FUNCTION_INDEX, JSFunction, internal_array_function)        \
  V(ITERATOR_RESULT_MAP_INDEX, Map, iterator_result_map)                       \
  V(INTL_DATE_TIME_FORMAT_FUNCTION_INDEX, JSFunction,                          \
    intl_date_time_format_function)                                            \
  V(INTL_NUMBER_FORMAT_FUNCTION_INDEX, JSFunction,                             \
    intl_number_format_function)                                               \
  V(INTL_COLLATOR_FUNCTION_INDEX, JSFunction, intl_collator_function)          \
  V(INTL_V8_BREAK_ITERATOR_FUNCTION_INDEX, JSFunction,                         \
    intl_v8_break_iterator_function)                                           \
  V(JS_ARRAY_FAST_SMI_ELEMENTS_MAP_INDEX, Map,                                 \
    js_array_fast_smi_elements_map_index)                                      \
  V(JS_ARRAY_FAST_HOLEY_SMI_ELEMENTS_MAP_INDEX, Map,                           \
    js_array_fast_holey_smi_elements_map_index)                                \
  V(JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, Map, js_array_fast_elements_map_index)   \
  V(JS_ARRAY_FAST_HOLEY_ELEMENTS_MAP_INDEX, Map,                               \
    js_array_fast_holey_elements_map_index)                                    \
  V(JS_ARRAY_FAST_DOUBLE_ELEMENTS_MAP_INDEX, Map,                              \
    js_array_fast_double_elements_map_index)                                   \
  V(JS_ARRAY_FAST_HOLEY_DOUBLE_ELEMENTS_MAP_INDEX, Map,                        \
    js_array_fast_holey_double_elements_map_index)                             \
  V(JS_MAP_FUN_INDEX, JSFunction, js_map_fun)                                  \
  V(JS_MAP_MAP_INDEX, Map, js_map_map)                                         \
  V(JS_MODULE_NAMESPACE_MAP, Map, js_module_namespace_map)                     \
  V(JS_SET_FUN_INDEX, JSFunction, js_set_fun)                                  \
  V(JS_SET_MAP_INDEX, Map, js_set_map)                                         \
  V(JS_WEAK_MAP_FUN_INDEX, JSFunction, js_weak_map_fun)                        \
  V(JS_WEAK_SET_FUN_INDEX, JSFunction, js_weak_set_fun)                        \
  V(MAP_CACHE_INDEX, Object, map_cache)                                        \
  V(MAP_ITERATOR_MAP_INDEX, Map, map_iterator_map)                             \
  V(MATH_RANDOM_INDEX_INDEX, Smi, math_random_index)                           \
  V(MATH_RANDOM_CACHE_INDEX, Object, math_random_cache)                        \
  V(MESSAGE_LISTENERS_INDEX, TemplateList, message_listeners)                  \
  V(NATIVES_UTILS_OBJECT_INDEX, Object, natives_utils_object)                  \
  V(NORMALIZED_MAP_CACHE_INDEX, Object, normalized_map_cache)                  \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function)                        \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
  V(OBJECT_FUNCTION_PROTOTYPE_MAP_INDEX, Map, object_function_prototype_map)   \
  V(OPAQUE_REFERENCE_FUNCTION_INDEX, JSFunction, opaque_reference_function)    \
  V(OSR_CODE_TABLE_INDEX, FixedArray, osr_code_table)                          \
  V(PROXY_CALLABLE_MAP_INDEX, Map, proxy_callable_map)                         \
  V(PROXY_CONSTRUCTOR_MAP_INDEX, Map, proxy_constructor_map)                   \
  V(PROXY_FUNCTION_INDEX, JSFunction, proxy_function)                          \
  V(PROXY_FUNCTION_MAP_INDEX, Map, proxy_function_map)                         \
  V(PROXY_MAP_INDEX, Map, proxy_map)                                           \
  V(PROMISE_GET_CAPABILITIES_EXECUTOR_SHARED_FUN, SharedFunctionInfo,          \
    promise_get_capabilities_executor_shared_fun)                              \
  V(PROMISE_RESOLVE_SHARED_FUN, SharedFunctionInfo,                            \
    promise_resolve_shared_fun)                                                \
  V(PROMISE_REJECT_SHARED_FUN, SharedFunctionInfo, promise_reject_shared_fun)  \
  V(PROMISE_PROTOTYPE_MAP_INDEX, Map, promise_prototype_map)                   \
  V(REGEXP_EXEC_FUNCTION_INDEX, JSFunction, regexp_exec_function)              \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function)                        \
  V(REGEXP_LAST_MATCH_INFO_INDEX, RegExpMatchInfo, regexp_last_match_info)     \
  V(REGEXP_INTERNAL_MATCH_INFO_INDEX, RegExpMatchInfo,                         \
    regexp_internal_match_info)                                                \
  V(REGEXP_PROTOTYPE_MAP_INDEX, Map, regexp_prototype_map)                     \
  V(REGEXP_RESULT_MAP_INDEX, Map, regexp_result_map)                           \
  V(SCRIPT_CONTEXT_TABLE_INDEX, ScriptContextTable, script_context_table)      \
  V(SCRIPT_FUNCTION_INDEX, JSFunction, script_function)                        \
  V(SECURITY_TOKEN_INDEX, Object, security_token)                              \
  V(SELF_WEAK_CELL_INDEX, WeakCell, self_weak_cell)                            \
  V(SET_ITERATOR_MAP_INDEX, Map, set_iterator_map)                             \
  V(SHARED_ARRAY_BUFFER_FUN_INDEX, JSFunction, shared_array_buffer_fun)        \
  V(SLOPPY_ARGUMENTS_MAP_INDEX, Map, sloppy_arguments_map)                     \
  V(SLOPPY_FUNCTION_MAP_INDEX, Map, sloppy_function_map)                       \
  V(SLOPPY_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    sloppy_function_without_prototype_map)                                     \
  V(SLOPPY_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX, Map,                    \
    sloppy_function_with_readonly_prototype_map)                               \
  V(SLOW_ALIASED_ARGUMENTS_MAP_INDEX, Map, slow_aliased_arguments_map)         \
  V(SLOW_OBJECT_WITH_NULL_PROTOTYPE_MAP, Map,                                  \
    slow_object_with_null_prototype_map)                                       \
  V(SLOW_TEMPLATE_INSTANTIATIONS_CACHE_INDEX, UnseededNumberDictionary,        \
    slow_template_instantiations_cache)                                        \
  V(STRICT_ARGUMENTS_MAP_INDEX, Map, strict_arguments_map)                     \
  V(ASYNC_FUNCTION_MAP_INDEX, Map, async_function_map)                         \
  V(STRICT_FUNCTION_MAP_INDEX, Map, strict_function_map)                       \
  V(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    strict_function_without_prototype_map)                                     \
  V(GENERATOR_FUNCTION_MAP_INDEX, Map, generator_function_map)                 \
  V(CLASS_FUNCTION_MAP_INDEX, Map, class_function_map)                         \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function)                        \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map)   \
  V(STRING_ITERATOR_MAP_INDEX, Map, string_iterator_map)                       \
  V(SYMBOL_FUNCTION_INDEX, JSFunction, symbol_function)                        \
  V(WASM_FUNCTION_MAP_INDEX, Map, wasm_function_map)                           \
  V(WASM_INSTANCE_CONSTRUCTOR_INDEX, JSFunction, wasm_instance_constructor)    \
  V(WASM_INSTANCE_SYM_INDEX, Symbol, wasm_instance_sym)                        \
  V(WASM_MEMORY_CONSTRUCTOR_INDEX, JSFunction, wasm_memory_constructor)        \
  V(WASM_MEMORY_SYM_INDEX, Symbol, wasm_memory_sym)                            \
  V(WASM_MODULE_CONSTRUCTOR_INDEX, JSFunction, wasm_module_constructor)        \
  V(WASM_MODULE_SYM_INDEX, Symbol, wasm_module_sym)                            \
  V(WASM_TABLE_CONSTRUCTOR_INDEX, JSFunction, wasm_table_constructor)          \
  V(WASM_TABLE_SYM_INDEX, Symbol, wasm_table_sym)                              \
  V(TYPED_ARRAY_FUN_INDEX, JSFunction, typed_array_function)                   \
  V(TYPED_ARRAY_PROTOTYPE_INDEX, JSObject, typed_array_prototype)              \
  V(UINT16_ARRAY_FUN_INDEX, JSFunction, uint16_array_fun)                      \
  V(UINT16X8_FUNCTION_INDEX, JSFunction, uint16x8_function)                    \
  V(UINT32_ARRAY_FUN_INDEX, JSFunction, uint32_array_fun)                      \
  V(UINT32X4_FUNCTION_INDEX, JSFunction, uint32x4_function)                    \
  V(UINT8_ARRAY_FUN_INDEX, JSFunction, uint8_array_fun)                        \
  V(UINT8_CLAMPED_ARRAY_FUN_INDEX, JSFunction, uint8_clamped_array_fun)        \
  V(UINT8X16_FUNCTION_INDEX, JSFunction, uint8x16_function)                    \
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                                        \
  NATIVE_CONTEXT_IMPORTED_FIELDS(V)                                            \
  NATIVE_CONTEXT_JS_ARRAY_ITERATOR_MAPS(V)

// A table of all script contexts. Every loaded top-level script with top-level
// lexical declarations contributes its ScriptContext into this table.
//
// The table is a fixed array, its first slot is the current used count and
// the subsequent slots 1..used contain ScriptContexts.
class ScriptContextTable : public FixedArray {
 public:
  // Conversions.
  static inline ScriptContextTable* cast(Object* context);

  struct LookupResult {
    int context_index;
    int slot_index;
    VariableMode mode;
    InitializationFlag init_flag;
    MaybeAssignedFlag maybe_assigned_flag;
  };

  inline int used() const;
  inline void set_used(int used);

  static inline Handle<Context> GetContext(Handle<ScriptContextTable> table,
                                           int i);

  // Lookup a variable `name` in a ScriptContextTable.
  // If it returns true, the variable is found and `result` contains
  // valid information about its location.
  // If it returns false, `result` is untouched.
  MUST_USE_RESULT
  static bool Lookup(Handle<ScriptContextTable> table, Handle<String> name,
                     LookupResult* result);

  MUST_USE_RESULT
  static Handle<ScriptContextTable> Extend(Handle<ScriptContextTable> table,
                                           Handle<Context> script_context);

  static int GetContextOffset(int context_index) {
    return kFirstContextOffset + context_index * kPointerSize;
  }

 private:
  static const int kUsedSlot = 0;
  static const int kFirstContextSlot = kUsedSlot + 1;
  static const int kFirstContextOffset =
      FixedArray::kHeaderSize + kFirstContextSlot * kPointerSize;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScriptContextTable);
};

// JSFunctions are pairs (context, function code), sometimes also called
// closures. A Context object is used to represent function contexts and
// dynamically pushed 'with' contexts (or 'scopes' in ECMA-262 speak).
//
// At runtime, the contexts build a stack in parallel to the execution
// stack, with the top-most context being the current context. All contexts
// have the following slots:
//
// [ closure   ]  This is the current function. It is the same for all
//                contexts inside a function. It provides access to the
//                incoming context (i.e., the outer context, which may
//                or may not become the current function's context), and
//                it provides access to the functions code and thus it's
//                scope information, which in turn contains the names of
//                statically allocated context slots. The names are needed
//                for dynamic lookups in the presence of 'with' or 'eval'.
//
// [ previous  ]  A pointer to the previous context.
//
// [ extension ]  Additional data.
//
//                For script contexts, it contains the respective ScopeInfo.
//
//                For catch contexts, it contains a ContextExtension object
//                consisting of the ScopeInfo and the name of the catch
//                variable.
//
//                For module contexts, it contains the module object.
//
//                For block contexts, it contains either the respective
//                ScopeInfo or a ContextExtension object consisting of the
//                ScopeInfo and an "extension object" (see below).
//
//                For with contexts, it contains a ContextExtension object
//                consisting of the ScopeInfo and an "extension object".
//
//                An "extension object" is used to dynamically extend a context
//                with additional variables, namely in the implementation of the
//                'with' construct and the 'eval' construct.  For instance,
//                Context::Lookup also searches the extension object for
//                properties.  (Storing the extension object is the original
//                purpose of this context slot, hence the name.)
//
// [ native_context ]  A pointer to the native context.
//
// In addition, function contexts may have statically allocated context slots
// to store local variables/functions that are accessed from inner functions
// (via static context addresses) or through 'eval' (dynamic context lookups).
// The native context contains additional slots for fast access to native
// properties.
//
// Finally, with Harmony scoping, the JSFunction representing a top level
// script will have the ScriptContext rather than a FunctionContext.
// Script contexts from all top-level scripts are gathered in
// ScriptContextTable.

class Context: public FixedArray {
 public:
  // Conversions.
  static inline Context* cast(Object* context);

  // The default context slot layout; indices are FixedArray slot indices.
  enum Field {
    // These slots are in all contexts.
    CLOSURE_INDEX,
    PREVIOUS_INDEX,
    // The extension slot is used for either the global object (in native
    // contexts), eval extension object (function contexts), subject of with
    // (with contexts), or the variable name (catch contexts), the serialized
    // scope info (block contexts), or the module instance (module contexts).
    EXTENSION_INDEX,
    NATIVE_CONTEXT_INDEX,

    // These slots are only in native contexts.
#define NATIVE_CONTEXT_SLOT(index, type, name) index,
    NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_SLOT)
#undef NATIVE_CONTEXT_SLOT

    // Properties from here are treated as weak references by the full GC.
    // Scavenge treats them as strong references.
    OPTIMIZED_FUNCTIONS_LIST,  // Weak.
    OPTIMIZED_CODE_LIST,       // Weak.
    DEOPTIMIZED_CODE_LIST,     // Weak.
    NEXT_CONTEXT_LINK,         // Weak.

    // Total number of slots.
    NATIVE_CONTEXT_SLOTS,
    FIRST_WEAK_SLOT = OPTIMIZED_FUNCTIONS_LIST,
    FIRST_JS_ARRAY_MAP_SLOT = JS_ARRAY_FAST_SMI_ELEMENTS_MAP_INDEX,

    MIN_CONTEXT_SLOTS = GLOBAL_PROXY_INDEX,
    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,

    // These slots hold values in debug evaluate contexts.
    WRAPPED_CONTEXT_INDEX = MIN_CONTEXT_SLOTS,
    WHITE_LIST_INDEX = MIN_CONTEXT_SLOTS + 1
  };

  void ResetErrorsThrown();
  void IncrementErrorsThrown();
  int GetErrorsThrown();

  // Direct slot access.
  inline JSFunction* closure();
  inline void set_closure(JSFunction* closure);

  inline Context* previous();
  inline void set_previous(Context* context);

  inline Object* next_context_link();

  inline bool has_extension();
  inline HeapObject* extension();
  inline void set_extension(HeapObject* object);
  JSObject* extension_object();
  JSReceiver* extension_receiver();
  ScopeInfo* scope_info();
  String* catch_name();

  // Find the module context (assuming there is one) and return the associated
  // module object.
  Module* module();

  // Get the context where var declarations will be hoisted to, which
  // may be the context itself.
  Context* declaration_context();
  bool is_declaration_context();

  // Get the next closure's context on the context chain.
  Context* closure_context();

  // Returns a JSGlobalProxy object or null.
  JSObject* global_proxy();
  void set_global_proxy(JSObject* global);

  // Get the JSGlobalObject object.
  V8_EXPORT_PRIVATE JSGlobalObject* global_object();

  // Get the script context by traversing the context chain.
  Context* script_context();

  // Compute the native context.
  inline Context* native_context();
  inline void set_native_context(Context* context);

  // Predicates for context types.  IsNativeContext is also defined on Object
  // because we frequently have to know if arbitrary objects are natives
  // contexts.
  inline bool IsNativeContext();
  inline bool IsFunctionContext();
  inline bool IsCatchContext();
  inline bool IsWithContext();
  inline bool IsDebugEvaluateContext();
  inline bool IsBlockContext();
  inline bool IsModuleContext();
  inline bool IsEvalContext();
  inline bool IsScriptContext();

  inline bool HasSameSecurityTokenAs(Context* that);

  // Removes a specific optimized code object from the optimized code map.
  // In case of non-OSR the code reference is cleared from the cache entry but
  // the entry itself is left in the map in order to proceed sharing literals.
  void EvictFromOptimizedCodeMap(Code* optimized_code, const char* reason);

  // Clear optimized code map.
  void ClearOptimizedCodeMap();

  // A native context keeps track of all osrd optimized functions.
  inline bool OptimizedCodeMapIsCleared();
  void SearchOptimizedCodeMap(SharedFunctionInfo* shared, BailoutId osr_ast_id,
                              Code** pcode, LiteralsArray** pliterals);
  int SearchOptimizedCodeMapEntry(SharedFunctionInfo* shared,
                                  BailoutId osr_ast_id);

  static void AddToOptimizedCodeMap(Handle<Context> native_context,
                                    Handle<SharedFunctionInfo> shared,
                                    Handle<Code> code,
                                    Handle<LiteralsArray> literals,
                                    BailoutId osr_ast_id);

  // A native context holds a list of all functions with optimized code.
  void AddOptimizedFunction(JSFunction* function);
  void RemoveOptimizedFunction(JSFunction* function);
  void SetOptimizedFunctionsListHead(Object* head);
  Object* OptimizedFunctionsListHead();

  // The native context also stores a list of all optimized code and a
  // list of all deoptimized code, which are needed by the deoptimizer.
  void AddOptimizedCode(Code* code);
  void SetOptimizedCodeListHead(Object* head);
  Object* OptimizedCodeListHead();
  void SetDeoptimizedCodeListHead(Object* head);
  Object* DeoptimizedCodeListHead();

  Handle<Object> ErrorMessageForCodeGenerationFromStrings();

  static int ImportedFieldIndexForName(Handle<String> name);
  static int IntrinsicIndexForName(Handle<String> name);
  static int IntrinsicIndexForName(const unsigned char* name, int length);

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  inline void set_##name(type* value);                    \
  inline bool is_##name(type* value);                     \
  inline type* name();
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
  // 3) result.is_null():
  //    There was no binding found, *index is always -1 and *attributes is
  //    always ABSENT.
  Handle<Object> Lookup(Handle<String> name, ContextLookupFlags flags,
                        int* index, PropertyAttributes* attributes,
                        InitializationFlag* init_flag,
                        VariableMode* variable_mode);

  // Code generation support.
  static int SlotOffset(int index) {
    return kHeaderSize + index * kPointerSize - kHeapObjectTag;
  }

  static int FunctionMapIndex(LanguageMode language_mode, FunctionKind kind) {
    // Note: Must be kept in sync with the FastNewClosure builtin.
    if (IsGeneratorFunction(kind)) {
      return GENERATOR_FUNCTION_MAP_INDEX;
    }

    if (IsAsyncFunction(kind)) {
      return ASYNC_FUNCTION_MAP_INDEX;
    }

    if (IsClassConstructor(kind)) {
      // Like the strict function map, but with no 'name' accessor. 'name'
      // needs to be the last property and it is added during instantiation,
      // in case a static property with the same name exists"
      return CLASS_FUNCTION_MAP_INDEX;
    }

    if (IsArrowFunction(kind) || IsConciseMethod(kind) ||
        IsAccessorFunction(kind)) {
      return STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX;
    }

    return is_strict(language_mode) ? STRICT_FUNCTION_MAP_INDEX
                                    : SLOPPY_FUNCTION_MAP_INDEX;
  }

  static int ArrayMapIndex(ElementsKind elements_kind) {
    DCHECK(IsFastElementsKind(elements_kind));
    return elements_kind + FIRST_JS_ARRAY_MAP_SLOT;
  }

  static const int kSize = kHeaderSize + NATIVE_CONTEXT_SLOTS * kPointerSize;
  static const int kNotFound = -1;

  // GC support.
  typedef FixedBodyDescriptor<
      kHeaderSize, kSize, kSize> ScavengeBodyDescriptor;

  typedef FixedBodyDescriptor<
      kHeaderSize,
      kHeaderSize + FIRST_WEAK_SLOT * kPointerSize,
      kSize> MarkCompactBodyDescriptor;

 private:
#ifdef DEBUG
  // Bootstrapping-aware type checks.
  V8_EXPORT_PRIVATE static bool IsBootstrappingOrNativeContext(Isolate* isolate,
                                                               Object* object);
  static bool IsBootstrappingOrValidParentContext(Object* object, Context* kid);
#endif

  STATIC_ASSERT(kHeaderSize == Internals::kContextHeaderSize);
  STATIC_ASSERT(EMBEDDER_DATA_INDEX == Internals::kContextEmbedderDataIndex);
};

typedef Context::Field ContextField;

}  // namespace internal
}  // namespace v8

#endif  // V8_CONTEXTS_H_
