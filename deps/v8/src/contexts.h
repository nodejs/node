// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONTEXTS_H_
#define V8_CONTEXTS_H_

#include "src/objects/fixed-array.h"

namespace v8 {
namespace internal {

class RegExpMatchInfo;

enum ContextLookupFlags {
  FOLLOW_CONTEXT_CHAIN = 1 << 0,
  FOLLOW_PROTOTYPE_CHAIN = 1 << 1,
  STOP_AT_DECLARATION_SCOPE = 1 << 2,
  SKIP_WITH_CONTEXT = 1 << 3,

  DONT_FOLLOW_CHAINS = 0,
  FOLLOW_CHAINS = FOLLOW_CONTEXT_CHAIN | FOLLOW_PROTOTYPE_CHAIN,
};

// Heap-allocated activation contexts.
//
// Contexts are implemented as FixedArray objects; the Context
// class is a convenience interface casted on a FixedArray object.
//
// Note: Context must have no virtual functions and Context objects
// must always be allocated via Heap::AllocateContext() or
// Factory::NewContext.

#define NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                               \
  V(ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX, JSFunction,                          \
    async_function_await_caught)                                            \
  V(ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX, JSFunction,                        \
    async_function_await_uncaught)                                          \
  V(ASYNC_FUNCTION_PROMISE_CREATE_INDEX, JSFunction,                        \
    async_function_promise_create)                                          \
  V(ASYNC_FUNCTION_PROMISE_RELEASE_INDEX, JSFunction,                       \
    async_function_promise_release)                                         \
  V(IS_ARRAYLIKE, JSFunction, is_arraylike)                                 \
  V(GENERATOR_NEXT_INTERNAL, JSFunction, generator_next_internal)           \
  V(MAKE_ERROR_INDEX, JSFunction, make_error)                               \
  V(MAKE_RANGE_ERROR_INDEX, JSFunction, make_range_error)                   \
  V(MAKE_SYNTAX_ERROR_INDEX, JSFunction, make_syntax_error)                 \
  V(MAKE_TYPE_ERROR_INDEX, JSFunction, make_type_error)                     \
  V(MAKE_URI_ERROR_INDEX, JSFunction, make_uri_error)                       \
  V(OBJECT_CREATE, JSFunction, object_create)                               \
  V(OBJECT_DEFINE_PROPERTIES, JSFunction, object_define_properties)         \
  V(OBJECT_DEFINE_PROPERTY, JSFunction, object_define_property)             \
  V(OBJECT_GET_PROTOTYPE_OF, JSFunction, object_get_prototype_of)           \
  V(OBJECT_IS_EXTENSIBLE, JSFunction, object_is_extensible)                 \
  V(OBJECT_IS_FROZEN, JSFunction, object_is_frozen)                         \
  V(OBJECT_IS_SEALED, JSFunction, object_is_sealed)                         \
  V(OBJECT_KEYS, JSFunction, object_keys)                                   \
  V(REGEXP_INTERNAL_MATCH, JSFunction, regexp_internal_match)               \
  V(REFLECT_APPLY_INDEX, JSFunction, reflect_apply)                         \
  V(REFLECT_CONSTRUCT_INDEX, JSFunction, reflect_construct)                 \
  V(REFLECT_DEFINE_PROPERTY_INDEX, JSFunction, reflect_define_property)     \
  V(REFLECT_DELETE_PROPERTY_INDEX, JSFunction, reflect_delete_property)     \
  V(MATH_FLOOR_INDEX, JSFunction, math_floor)                               \
  V(MATH_POW_INDEX, JSFunction, math_pow)                                   \
  V(NEW_PROMISE_CAPABILITY_INDEX, JSFunction, new_promise_capability)       \
  V(PROMISE_INTERNAL_CONSTRUCTOR_INDEX, JSFunction,                         \
    promise_internal_constructor)                                           \
  V(IS_PROMISE_INDEX, JSFunction, is_promise)                               \
  V(PROMISE_THEN_INDEX, JSFunction, promise_then)                           \
  V(ASYNC_GENERATOR_AWAIT_CAUGHT, JSFunction, async_generator_await_caught) \
  V(ASYNC_GENERATOR_AWAIT_UNCAUGHT, JSFunction, async_generator_await_uncaught)

#define NATIVE_CONTEXT_IMPORTED_FIELDS(V)                                 \
  V(ARRAY_SHIFT_INDEX, JSFunction, array_shift)                           \
  V(ARRAY_SPLICE_INDEX, JSFunction, array_splice)                         \
  V(ARRAY_UNSHIFT_INDEX, JSFunction, array_unshift)                       \
  V(ARRAY_ENTRIES_ITERATOR_INDEX, JSFunction, array_entries_iterator)     \
  V(ARRAY_FOR_EACH_ITERATOR_INDEX, JSFunction, array_for_each_iterator)   \
  V(ARRAY_KEYS_ITERATOR_INDEX, JSFunction, array_keys_iterator)           \
  V(ARRAY_VALUES_ITERATOR_INDEX, JSFunction, array_values_iterator)       \
  V(ERROR_FUNCTION_INDEX, JSFunction, error_function)                     \
  V(ERROR_TO_STRING, JSFunction, error_to_string)                         \
  V(EVAL_ERROR_FUNCTION_INDEX, JSFunction, eval_error_function)           \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun)                   \
  V(GLOBAL_PROXY_FUNCTION_INDEX, JSFunction, global_proxy_function)       \
  V(MAP_DELETE_INDEX, JSFunction, map_delete)                             \
  V(MAP_GET_INDEX, JSFunction, map_get)                                   \
  V(MAP_HAS_INDEX, JSFunction, map_has)                                   \
  V(MAP_SET_INDEX, JSFunction, map_set)                                   \
  V(FUNCTION_HAS_INSTANCE_INDEX, JSFunction, function_has_instance)       \
  V(OBJECT_VALUE_OF, JSFunction, object_value_of)                         \
  V(OBJECT_TO_STRING, JSFunction, object_to_string)                       \
  V(PROMISE_CATCH_INDEX, JSFunction, promise_catch)                       \
  V(PROMISE_FUNCTION_INDEX, JSFunction, promise_function)                 \
  V(RANGE_ERROR_FUNCTION_INDEX, JSFunction, range_error_function)         \
  V(REFERENCE_ERROR_FUNCTION_INDEX, JSFunction, reference_error_function) \
  V(RESOLVE_LOCALE_FUNCTION_INDEX, JSFunction, resolve_locale)            \
  V(SET_ADD_INDEX, JSFunction, set_add)                                   \
  V(SET_DELETE_INDEX, JSFunction, set_delete)                             \
  V(SET_HAS_INDEX, JSFunction, set_has)                                   \
  V(SYNTAX_ERROR_FUNCTION_INDEX, JSFunction, syntax_error_function)       \
  V(TYPE_ERROR_FUNCTION_INDEX, JSFunction, type_error_function)           \
  V(URI_ERROR_FUNCTION_INDEX, JSFunction, uri_error_function)             \
  V(WASM_COMPILE_ERROR_FUNCTION_INDEX, JSFunction,                        \
    wasm_compile_error_function)                                          \
  V(WASM_LINK_ERROR_FUNCTION_INDEX, JSFunction, wasm_link_error_function) \
  V(WASM_RUNTIME_ERROR_FUNCTION_INDEX, JSFunction,                        \
    wasm_runtime_error_function)                                          \
  V(WEAKMAP_SET_INDEX, JSFunction, weakmap_set)                           \
  V(WEAKSET_ADD_INDEX, JSFunction, weakset_add)

#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object)                         \
  V(EMBEDDER_DATA_INDEX, FixedArray, embedder_data)                            \
  /* Below is alpha-sorted */                                                  \
  V(ACCESSOR_PROPERTY_DESCRIPTOR_MAP_INDEX, Map,                               \
    accessor_property_descriptor_map)                                          \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings)    \
  V(ARRAY_BUFFER_FUN_INDEX, JSFunction, array_buffer_fun)                      \
  V(ARRAY_BUFFER_MAP_INDEX, Map, array_buffer_map)                             \
  V(ARRAY_BUFFER_NOINIT_FUN_INDEX, JSFunction, array_buffer_noinit_fun)        \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function)                          \
  V(ASYNC_FROM_SYNC_ITERATOR_MAP_INDEX, Map, async_from_sync_iterator_map)     \
  V(ASYNC_FUNCTION_AWAIT_REJECT_SHARED_FUN, SharedFunctionInfo,                \
    async_function_await_reject_shared_fun)                                    \
  V(ASYNC_FUNCTION_AWAIT_RESOLVE_SHARED_FUN, SharedFunctionInfo,               \
    async_function_await_resolve_shared_fun)                                   \
  V(ASYNC_FUNCTION_FUNCTION_INDEX, JSFunction, async_function_constructor)     \
  V(ASYNC_GENERATOR_FUNCTION_FUNCTION_INDEX, JSFunction,                       \
    async_generator_function_function)                                         \
  V(ASYNC_ITERATOR_VALUE_UNWRAP_SHARED_FUN, SharedFunctionInfo,                \
    async_iterator_value_unwrap_shared_fun)                                    \
  V(ASYNC_GENERATOR_AWAIT_REJECT_SHARED_FUN, SharedFunctionInfo,               \
    async_generator_await_reject_shared_fun)                                   \
  V(ASYNC_GENERATOR_AWAIT_RESOLVE_SHARED_FUN, SharedFunctionInfo,              \
    async_generator_await_resolve_shared_fun)                                  \
  V(ASYNC_GENERATOR_YIELD_RESOLVE_SHARED_FUN, SharedFunctionInfo,              \
    async_generator_yield_resolve_shared_fun)                                  \
  V(ASYNC_GENERATOR_RETURN_RESOLVE_SHARED_FUN, SharedFunctionInfo,             \
    async_generator_return_resolve_shared_fun)                                 \
  V(ASYNC_GENERATOR_RETURN_CLOSED_RESOLVE_SHARED_FUN, SharedFunctionInfo,      \
    async_generator_return_closed_resolve_shared_fun)                          \
  V(ASYNC_GENERATOR_RETURN_CLOSED_REJECT_SHARED_FUN, SharedFunctionInfo,       \
    async_generator_return_closed_reject_shared_fun)                           \
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
  V(EXTRAS_EXPORTS_OBJECT_INDEX, JSObject, extras_binding_object)              \
  V(EXTRAS_UTILS_OBJECT_INDEX, Object, extras_utils_object)                    \
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
  V(INITIAL_ASYNC_GENERATOR_PROTOTYPE_INDEX, JSObject,                         \
    initial_async_generator_prototype)                                         \
  V(INITIAL_ITERATOR_PROTOTYPE_INDEX, JSObject, initial_iterator_prototype)    \
  V(INITIAL_MAP_PROTOTYPE_MAP_INDEX, Map, initial_map_prototype_map)           \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype)        \
  V(INITIAL_SET_PROTOTYPE_MAP_INDEX, Map, initial_set_prototype_map)           \
  V(INITIAL_STRING_PROTOTYPE_INDEX, JSObject, initial_string_prototype)        \
  V(INITIAL_WEAKMAP_PROTOTYPE_MAP_INDEX, Map, initial_weakmap_prototype_map)   \
  V(INITIAL_WEAKSET_PROTOTYPE_MAP_INDEX, Map, initial_weakset_prototype_map)   \
  V(INT16_ARRAY_FUN_INDEX, JSFunction, int16_array_fun)                        \
  V(INT32_ARRAY_FUN_INDEX, JSFunction, int32_array_fun)                        \
  V(INT8_ARRAY_FUN_INDEX, JSFunction, int8_array_fun)                          \
  V(INTERNAL_ARRAY_FUNCTION_INDEX, JSFunction, internal_array_function)        \
  V(ITERATOR_RESULT_MAP_INDEX, Map, iterator_result_map)                       \
  V(INTL_DATE_TIME_FORMAT_FUNCTION_INDEX, JSFunction,                          \
    intl_date_time_format_function)                                            \
  V(INTL_NUMBER_FORMAT_FUNCTION_INDEX, JSFunction,                             \
    intl_number_format_function)                                               \
  V(INTL_NUMBER_FORMAT_INTERNAL_FORMAT_NUMBER_SHARED_FUN, SharedFunctionInfo,  \
    number_format_internal_format_number_shared_fun)                           \
  V(INTL_LOCALE_FUNCTION_INDEX, JSFunction, intl_locale_function)              \
  V(INTL_COLLATOR_FUNCTION_INDEX, JSFunction, intl_collator_function)          \
  V(INTL_PLURAL_RULES_FUNCTION_INDEX, JSFunction, intl_plural_rules_function)  \
  V(INTL_V8_BREAK_ITERATOR_FUNCTION_INDEX, JSFunction,                         \
    intl_v8_break_iterator_function)                                           \
  V(JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX, Map,                               \
    js_array_fast_smi_elements_map_index)                                      \
  V(JS_ARRAY_HOLEY_SMI_ELEMENTS_MAP_INDEX, Map,                                \
    js_array_fast_holey_smi_elements_map_index)                                \
  V(JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX, Map, js_array_fast_elements_map_index) \
  V(JS_ARRAY_HOLEY_ELEMENTS_MAP_INDEX, Map,                                    \
    js_array_fast_holey_elements_map_index)                                    \
  V(JS_ARRAY_PACKED_DOUBLE_ELEMENTS_MAP_INDEX, Map,                            \
    js_array_fast_double_elements_map_index)                                   \
  V(JS_ARRAY_HOLEY_DOUBLE_ELEMENTS_MAP_INDEX, Map,                             \
    js_array_fast_holey_double_elements_map_index)                             \
  V(JS_MAP_FUN_INDEX, JSFunction, js_map_fun)                                  \
  V(JS_MAP_MAP_INDEX, Map, js_map_map)                                         \
  V(JS_MODULE_NAMESPACE_MAP, Map, js_module_namespace_map)                     \
  V(JS_SET_FUN_INDEX, JSFunction, js_set_fun)                                  \
  V(JS_SET_MAP_INDEX, Map, js_set_map)                                         \
  V(JS_WEAK_MAP_FUN_INDEX, JSFunction, js_weak_map_fun)                        \
  V(JS_WEAK_SET_FUN_INDEX, JSFunction, js_weak_set_fun)                        \
  V(MAP_CACHE_INDEX, Object, map_cache)                                        \
  V(MAP_KEY_ITERATOR_MAP_INDEX, Map, map_key_iterator_map)                     \
  V(MAP_KEY_VALUE_ITERATOR_MAP_INDEX, Map, map_key_value_iterator_map)         \
  V(MAP_VALUE_ITERATOR_MAP_INDEX, Map, map_value_iterator_map)                 \
  V(MATH_RANDOM_INDEX_INDEX, Smi, math_random_index)                           \
  V(MATH_RANDOM_CACHE_INDEX, Object, math_random_cache)                        \
  V(MESSAGE_LISTENERS_INDEX, TemplateList, message_listeners)                  \
  V(NATIVES_UTILS_OBJECT_INDEX, Object, natives_utils_object)                  \
  V(NORMALIZED_MAP_CACHE_INDEX, Object, normalized_map_cache)                  \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function)                        \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
  V(OBJECT_FUNCTION_PROTOTYPE_MAP_INDEX, Map, object_function_prototype_map)   \
  V(OPAQUE_REFERENCE_FUNCTION_INDEX, JSFunction, opaque_reference_function)    \
  V(PROXY_CALLABLE_MAP_INDEX, Map, proxy_callable_map)                         \
  V(PROXY_CONSTRUCTOR_MAP_INDEX, Map, proxy_constructor_map)                   \
  V(PROXY_FUNCTION_INDEX, JSFunction, proxy_function)                          \
  V(PROXY_MAP_INDEX, Map, proxy_map)                                           \
  V(PROXY_REVOCABLE_RESULT_MAP_INDEX, Map, proxy_revocable_result_map)         \
  V(PROXY_REVOKE_SHARED_FUN, SharedFunctionInfo, proxy_revoke_shared_fun)      \
  V(PROMISE_GET_CAPABILITIES_EXECUTOR_SHARED_FUN, SharedFunctionInfo,          \
    promise_get_capabilities_executor_shared_fun)                              \
  V(PROMISE_CAPABILITY_DEFAULT_REJECT_SHARED_FUN_INDEX, SharedFunctionInfo,    \
    promise_capability_default_reject_shared_fun)                              \
  V(PROMISE_CAPABILITY_DEFAULT_RESOLVE_SHARED_FUN_INDEX, SharedFunctionInfo,   \
    promise_capability_default_resolve_shared_fun)                             \
  V(PROMISE_THEN_FINALLY_SHARED_FUN, SharedFunctionInfo,                       \
    promise_then_finally_shared_fun)                                           \
  V(PROMISE_CATCH_FINALLY_SHARED_FUN, SharedFunctionInfo,                      \
    promise_catch_finally_shared_fun)                                          \
  V(PROMISE_VALUE_THUNK_FINALLY_SHARED_FUN, SharedFunctionInfo,                \
    promise_value_thunk_finally_shared_fun)                                    \
  V(PROMISE_THROWER_FINALLY_SHARED_FUN, SharedFunctionInfo,                    \
    promise_thrower_finally_shared_fun)                                        \
  V(PROMISE_ALL_RESOLVE_ELEMENT_SHARED_FUN, SharedFunctionInfo,                \
    promise_all_resolve_element_shared_fun)                                    \
  V(PROMISE_PROTOTYPE_INDEX, JSObject, promise_prototype)                      \
  V(REGEXP_EXEC_FUNCTION_INDEX, JSFunction, regexp_exec_function)              \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function)                        \
  V(REGEXP_LAST_MATCH_INFO_INDEX, RegExpMatchInfo, regexp_last_match_info)     \
  V(REGEXP_INTERNAL_MATCH_INFO_INDEX, RegExpMatchInfo,                         \
    regexp_internal_match_info)                                                \
  V(REGEXP_PROTOTYPE_MAP_INDEX, Map, regexp_prototype_map)                     \
  V(INITIAL_REGEXP_STRING_ITERATOR_PROTOTYPE_MAP_INDEX, Map,                   \
    initial_regexp_string_iterator_prototype_map_index)                        \
  V(REGEXP_RESULT_MAP_INDEX, Map, regexp_result_map)                           \
  V(SCRIPT_CONTEXT_TABLE_INDEX, ScriptContextTable, script_context_table)      \
  V(SECURITY_TOKEN_INDEX, Object, security_token)                              \
  V(SELF_WEAK_CELL_INDEX, WeakCell, self_weak_cell)                            \
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
  V(METHOD_WITH_HOME_OBJECT_MAP_INDEX, Map, method_with_home_object_map)       \
  V(METHOD_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX, Map,                           \
    method_with_name_and_home_object_map)                                      \
  V(ASYNC_FUNCTION_MAP_INDEX, Map, async_function_map)                         \
  V(ASYNC_FUNCTION_WITH_NAME_MAP_INDEX, Map, async_function_with_name_map)     \
  V(ASYNC_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX, Map,                            \
    async_function_with_home_object_map)                                       \
  V(ASYNC_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX, Map,                   \
    async_function_with_name_and_home_object_map)                              \
  V(GENERATOR_FUNCTION_MAP_INDEX, Map, generator_function_map)                 \
  V(GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX, Map,                               \
    generator_function_with_name_map)                                          \
  V(GENERATOR_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX, Map,                        \
    generator_function_with_home_object_map)                                   \
  V(GENERATOR_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX, Map,               \
    generator_function_with_name_and_home_object_map)                          \
  V(ASYNC_GENERATOR_FUNCTION_MAP_INDEX, Map, async_generator_function_map)     \
  V(ASYNC_GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX, Map,                         \
    async_generator_function_with_name_map)                                    \
  V(ASYNC_GENERATOR_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX, Map,                  \
    async_generator_function_with_home_object_map)                             \
  V(ASYNC_GENERATOR_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX, Map,         \
    async_generator_function_with_name_and_home_object_map)                    \
  V(CLASS_FUNCTION_MAP_INDEX, Map, class_function_map)                         \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function)                        \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map)   \
  V(STRING_ITERATOR_MAP_INDEX, Map, string_iterator_map)                       \
  V(SYMBOL_FUNCTION_INDEX, JSFunction, symbol_function)                        \
  V(NATIVE_FUNCTION_MAP_INDEX, Map, native_function_map)                       \
  V(WASM_GLOBAL_CONSTRUCTOR_INDEX, JSFunction, wasm_global_constructor)        \
  V(WASM_INSTANCE_CONSTRUCTOR_INDEX, JSFunction, wasm_instance_constructor)    \
  V(WASM_MEMORY_CONSTRUCTOR_INDEX, JSFunction, wasm_memory_constructor)        \
  V(WASM_MODULE_CONSTRUCTOR_INDEX, JSFunction, wasm_module_constructor)        \
  V(WASM_TABLE_CONSTRUCTOR_INDEX, JSFunction, wasm_table_constructor)          \
  V(TYPED_ARRAY_FUN_INDEX, JSFunction, typed_array_function)                   \
  V(TYPED_ARRAY_PROTOTYPE_INDEX, JSObject, typed_array_prototype)              \
  V(UINT16_ARRAY_FUN_INDEX, JSFunction, uint16_array_fun)                      \
  V(UINT32_ARRAY_FUN_INDEX, JSFunction, uint32_array_fun)                      \
  V(UINT8_ARRAY_FUN_INDEX, JSFunction, uint8_array_fun)                        \
  V(UINT8_CLAMPED_ARRAY_FUN_INDEX, JSFunction, uint8_clamped_array_fun)        \
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                                        \
  NATIVE_CONTEXT_IMPORTED_FIELDS(V)

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

  static inline Handle<Context> GetContext(Isolate* isolate,
                                           Handle<ScriptContextTable> table,
                                           int i);

  // Lookup a variable `name` in a ScriptContextTable.
  // If it returns true, the variable is found and `result` contains
  // valid information about its location.
  // If it returns false, `result` is untouched.
  V8_WARN_UNUSED_RESULT
  static bool Lookup(Isolate* isolate, Handle<ScriptContextTable> table,
                     Handle<String> name, LookupResult* result);

  V8_WARN_UNUSED_RESULT
  static Handle<ScriptContextTable> Extend(Handle<ScriptContextTable> table,
                                           Handle<Context> script_context);

  static const int kUsedSlotIndex = 0;
  static const int kFirstContextSlotIndex = 1;
  static const int kMinLength = kFirstContextSlotIndex;

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
// [ scope_info     ]  This is the scope info describing the current context. It
//                     contains the names of statically allocated context slots,
//                     and stack-allocated locals.  The names are needed for
//                     dynamic lookups in the presence of 'with' or 'eval', and
//                     for the debugger.
//
// [ previous       ]  A pointer to the previous context.
//
// [ extension      ]  Additional data.
//
//                     For module contexts, it contains the module object.
//
//                     For block contexts, it may contain an "extension object"
//                     (see below).
//
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

class Context : public FixedArray, public NeverReadOnlySpaceObject {
 public:
  // Use the mixin methods over the HeapObject methods.
  // TODO(v8:7786) Remove once the HeapObject methods are gone.
  using NeverReadOnlySpaceObject::GetHeap;
  using NeverReadOnlySpaceObject::GetIsolate;

  // Conversions.
  static inline Context* cast(Object* context);

  // The default context slot layout; indices are FixedArray slot indices.
  enum Field {
    // These slots are in all contexts.
    SCOPE_INFO_INDEX,
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
    OPTIMIZED_CODE_LIST,    // Weak.
    DEOPTIMIZED_CODE_LIST,  // Weak.
    NEXT_CONTEXT_LINK,      // Weak.

    // Total number of slots.
    NATIVE_CONTEXT_SLOTS,
    FIRST_WEAK_SLOT = OPTIMIZED_CODE_LIST,
    FIRST_JS_ARRAY_MAP_SLOT = JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX,

    MIN_CONTEXT_SLOTS = GLOBAL_PROXY_INDEX,
    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,

    // These slots hold values in debug evaluate contexts.
    WRAPPED_CONTEXT_INDEX = MIN_CONTEXT_SLOTS,
    WHITE_LIST_INDEX = MIN_CONTEXT_SLOTS + 1
  };

  // A region of native context entries containing maps for functions created
  // by Builtins::kFastNewClosure.
  static const int FIRST_FUNCTION_MAP_INDEX = SLOPPY_FUNCTION_MAP_INDEX;
  static const int LAST_FUNCTION_MAP_INDEX = CLASS_FUNCTION_MAP_INDEX;

  static const int kNoContext = 0;
  static const int kInvalidContext = 1;

  void ResetErrorsThrown();
  void IncrementErrorsThrown();
  int GetErrorsThrown();

  // Direct slot access.
  inline void set_scope_info(ScopeInfo* scope_info);
  inline Context* previous();
  inline void set_previous(Context* context);

  inline Object* next_context_link();

  inline bool has_extension();
  inline HeapObject* extension();
  inline void set_extension(HeapObject* object);
  JSObject* extension_object();
  JSReceiver* extension_receiver();
  ScopeInfo* scope_info();

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
  inline Context* native_context() const;
  inline void set_native_context(Context* context);

  // Predicates for context types.  IsNativeContext is also defined on Object
  // because we frequently have to know if arbitrary objects are natives
  // contexts.
  inline bool IsNativeContext() const;
  inline bool IsFunctionContext() const;
  inline bool IsCatchContext() const;
  inline bool IsWithContext() const;
  inline bool IsDebugEvaluateContext() const;
  inline bool IsBlockContext() const;
  inline bool IsModuleContext() const;
  inline bool IsEvalContext() const;
  inline bool IsScriptContext() const;

  inline bool HasSameSecurityTokenAs(Context* that) const;

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
  inline bool is_##name(type* value) const;               \
  inline type* name() const;
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
  Handle<Object> Lookup(Handle<String> name, ContextLookupFlags flags,
                        int* index, PropertyAttributes* attributes,
                        InitializationFlag* init_flag,
                        VariableMode* variable_mode,
                        bool* is_sloppy_function_name = nullptr);

  // Code generation support.
  static int SlotOffset(int index) {
    return kHeaderSize + index * kPointerSize - kHeapObjectTag;
  }

  static inline int FunctionMapIndex(LanguageMode language_mode,
                                     FunctionKind kind, bool has_prototype_slot,
                                     bool has_shared_name,
                                     bool needs_home_object);

  static int ArrayMapIndex(ElementsKind elements_kind) {
    DCHECK(IsFastElementsKind(elements_kind));
    return elements_kind + FIRST_JS_ARRAY_MAP_SLOT;
  }

  inline Map* GetInitialJSArrayMap(ElementsKind kind) const;

  static const int kSize = kHeaderSize + NATIVE_CONTEXT_SLOTS * kPointerSize;
  static const int kNotFound = -1;

  // GC support.
  typedef FixedBodyDescriptor<kHeaderSize, kSize, kSize> BodyDescriptor;

  typedef FixedBodyDescriptor<
      kHeaderSize, kHeaderSize + FIRST_WEAK_SLOT * kPointerSize, kSize>
      BodyDescriptorWeak;

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
