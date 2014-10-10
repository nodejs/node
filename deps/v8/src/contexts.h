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
  FOLLOW_CONTEXT_CHAIN = 1,
  FOLLOW_PROTOTYPE_CHAIN = 2,

  DONT_FOLLOW_CHAINS = 0,
  FOLLOW_CHAINS = FOLLOW_CONTEXT_CHAIN | FOLLOW_PROTOTYPE_CHAIN
};


// ES5 10.2 defines lexical environments with mutable and immutable bindings.
// Immutable bindings have two states, initialized and uninitialized, and
// their state is changed by the InitializeImmutableBinding method. The
// BindingFlags enum represents information if a binding has definitely been
// initialized. A mutable binding does not need to be checked and thus has
// the BindingFlag MUTABLE_IS_INITIALIZED.
//
// There are two possibilities for immutable bindings
//  * 'const' declared variables. They are initialized when evaluating the
//    corresponding declaration statement. They need to be checked for being
//    initialized and thus get the flag IMMUTABLE_CHECK_INITIALIZED.
//  * The function name of a named function literal. The binding is immediately
//    initialized when entering the function and thus does not need to be
//    checked. it gets the BindingFlag IMMUTABLE_IS_INITIALIZED.
// Accessing an uninitialized binding produces the undefined value.
//
// The harmony proposal for block scoped bindings also introduces the
// uninitialized state for mutable bindings.
//  * A 'let' declared variable. They are initialized when evaluating the
//    corresponding declaration statement. They need to be checked for being
//    initialized and thus get the flag MUTABLE_CHECK_INITIALIZED.
//  * A 'var' declared variable. It is initialized immediately upon creation
//    and thus doesn't need to be checked. It gets the flag
//    MUTABLE_IS_INITIALIZED.
//  * Catch bound variables, function parameters and variables introduced by
//    function declarations are initialized immediately and do not need to be
//    checked. Thus they get the flag MUTABLE_IS_INITIALIZED.
// Immutable bindings in harmony mode get the _HARMONY flag variants. Accessing
// an uninitialized binding produces a reference error.
//
// In V8 uninitialized bindings are set to the hole value upon creation and set
// to a different value upon initialization.
enum BindingFlags {
  MUTABLE_IS_INITIALIZED,
  MUTABLE_CHECK_INITIALIZED,
  IMMUTABLE_IS_INITIALIZED,
  IMMUTABLE_CHECK_INITIALIZED,
  IMMUTABLE_IS_INITIALIZED_HARMONY,
  IMMUTABLE_CHECK_INITIALIZED_HARMONY,
  MISSING_BINDING
};


// Heap-allocated activation contexts.
//
// Contexts are implemented as FixedArray objects; the Context
// class is a convenience interface casted on a FixedArray object.
//
// Note: Context must have no virtual functions and Context objects
// must always be allocated via Heap::AllocateContext() or
// Factory::NewContext.

#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object)                         \
  V(SECURITY_TOKEN_INDEX, Object, security_token)                              \
  V(BOOLEAN_FUNCTION_INDEX, JSFunction, boolean_function)                      \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function)                        \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function)                        \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map)   \
  V(SYMBOL_FUNCTION_INDEX, JSFunction, symbol_function)                        \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
  V(INTERNAL_ARRAY_FUNCTION_INDEX, JSFunction, internal_array_function)        \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function)                          \
  V(JS_ARRAY_MAPS_INDEX, Object, js_array_maps)                                \
  V(DATE_FUNCTION_INDEX, JSFunction, date_function)                            \
  V(JSON_OBJECT_INDEX, JSObject, json_object)                                  \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function)                        \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype)        \
  V(INITIAL_ARRAY_PROTOTYPE_INDEX, JSObject, initial_array_prototype)          \
  V(CREATE_DATE_FUN_INDEX, JSFunction, create_date_fun)                        \
  V(TO_NUMBER_FUN_INDEX, JSFunction, to_number_fun)                            \
  V(TO_STRING_FUN_INDEX, JSFunction, to_string_fun)                            \
  V(TO_DETAIL_STRING_FUN_INDEX, JSFunction, to_detail_string_fun)              \
  V(TO_OBJECT_FUN_INDEX, JSFunction, to_object_fun)                            \
  V(TO_INTEGER_FUN_INDEX, JSFunction, to_integer_fun)                          \
  V(TO_UINT32_FUN_INDEX, JSFunction, to_uint32_fun)                            \
  V(TO_INT32_FUN_INDEX, JSFunction, to_int32_fun)                              \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun)                        \
  V(INSTANTIATE_FUN_INDEX, JSFunction, instantiate_fun)                        \
  V(CONFIGURE_INSTANCE_FUN_INDEX, JSFunction, configure_instance_fun)          \
  V(MATH_ABS_FUN_INDEX, JSFunction, math_abs_fun)                              \
  V(MATH_ACOS_FUN_INDEX, JSFunction, math_acos_fun)                            \
  V(MATH_ASIN_FUN_INDEX, JSFunction, math_asin_fun)                            \
  V(MATH_ATAN_FUN_INDEX, JSFunction, math_atan_fun)                            \
  V(MATH_ATAN2_FUN_INDEX, JSFunction, math_atan2_fun)                          \
  V(MATH_CEIL_FUN_INDEX, JSFunction, math_ceil_fun)                            \
  V(MATH_COS_FUN_INDEX, JSFunction, math_cos_fun)                              \
  V(MATH_EXP_FUN_INDEX, JSFunction, math_exp_fun)                              \
  V(MATH_FLOOR_FUN_INDEX, JSFunction, math_floor_fun)                          \
  V(MATH_IMUL_FUN_INDEX, JSFunction, math_imul_fun)                            \
  V(MATH_LOG_FUN_INDEX, JSFunction, math_log_fun)                              \
  V(MATH_MAX_FUN_INDEX, JSFunction, math_max_fun)                              \
  V(MATH_MIN_FUN_INDEX, JSFunction, math_min_fun)                              \
  V(MATH_POW_FUN_INDEX, JSFunction, math_pow_fun)                              \
  V(MATH_RANDOM_FUN_INDEX, JSFunction, math_random_fun)                        \
  V(MATH_ROUND_FUN_INDEX, JSFunction, math_round_fun)                          \
  V(MATH_SIN_FUN_INDEX, JSFunction, math_sin_fun)                              \
  V(MATH_SQRT_FUN_INDEX, JSFunction, math_sqrt_fun)                            \
  V(MATH_TAN_FUN_INDEX, JSFunction, math_tan_fun)                              \
  V(ARRAY_BUFFER_FUN_INDEX, JSFunction, array_buffer_fun)                      \
  V(UINT8_ARRAY_FUN_INDEX, JSFunction, uint8_array_fun)                        \
  V(INT8_ARRAY_FUN_INDEX, JSFunction, int8_array_fun)                          \
  V(UINT16_ARRAY_FUN_INDEX, JSFunction, uint16_array_fun)                      \
  V(INT16_ARRAY_FUN_INDEX, JSFunction, int16_array_fun)                        \
  V(UINT32_ARRAY_FUN_INDEX, JSFunction, uint32_array_fun)                      \
  V(INT32_ARRAY_FUN_INDEX, JSFunction, int32_array_fun)                        \
  V(FLOAT32_ARRAY_FUN_INDEX, JSFunction, float32_array_fun)                    \
  V(FLOAT64_ARRAY_FUN_INDEX, JSFunction, float64_array_fun)                    \
  V(UINT8_CLAMPED_ARRAY_FUN_INDEX, JSFunction, uint8_clamped_array_fun)        \
  V(INT8_ARRAY_EXTERNAL_MAP_INDEX, Map, int8_array_external_map)               \
  V(UINT8_ARRAY_EXTERNAL_MAP_INDEX, Map, uint8_array_external_map)             \
  V(INT16_ARRAY_EXTERNAL_MAP_INDEX, Map, int16_array_external_map)             \
  V(UINT16_ARRAY_EXTERNAL_MAP_INDEX, Map, uint16_array_external_map)           \
  V(INT32_ARRAY_EXTERNAL_MAP_INDEX, Map, int32_array_external_map)             \
  V(UINT32_ARRAY_EXTERNAL_MAP_INDEX, Map, uint32_array_external_map)           \
  V(FLOAT32_ARRAY_EXTERNAL_MAP_INDEX, Map, float32_array_external_map)         \
  V(FLOAT64_ARRAY_EXTERNAL_MAP_INDEX, Map, float64_array_external_map)         \
  V(UINT8_CLAMPED_ARRAY_EXTERNAL_MAP_INDEX, Map,                               \
    uint8_clamped_array_external_map)                                          \
  V(DATA_VIEW_FUN_INDEX, JSFunction, data_view_fun)                            \
  V(SLOPPY_FUNCTION_MAP_INDEX, Map, sloppy_function_map)                       \
  V(SLOPPY_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX, Map,                    \
    sloppy_function_with_readonly_prototype_map)                               \
  V(STRICT_FUNCTION_MAP_INDEX, Map, strict_function_map)                       \
  V(SLOPPY_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    sloppy_function_without_prototype_map)                                     \
  V(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    strict_function_without_prototype_map)                                     \
  V(BOUND_FUNCTION_MAP_INDEX, Map, bound_function_map)                         \
  V(REGEXP_RESULT_MAP_INDEX, Map, regexp_result_map)                           \
  V(SLOPPY_ARGUMENTS_MAP_INDEX, Map, sloppy_arguments_map)                     \
  V(ALIASED_ARGUMENTS_MAP_INDEX, Map, aliased_arguments_map)                   \
  V(STRICT_ARGUMENTS_MAP_INDEX, Map, strict_arguments_map)                     \
  V(MESSAGE_LISTENERS_INDEX, JSObject, message_listeners)                      \
  V(MAKE_MESSAGE_FUN_INDEX, JSFunction, make_message_fun)                      \
  V(GET_STACK_TRACE_LINE_INDEX, JSFunction, get_stack_trace_line_fun)          \
  V(CONFIGURE_GLOBAL_INDEX, JSFunction, configure_global_fun)                  \
  V(FUNCTION_CACHE_INDEX, JSObject, function_cache)                            \
  V(JSFUNCTION_RESULT_CACHES_INDEX, FixedArray, jsfunction_result_caches)      \
  V(NORMALIZED_MAP_CACHE_INDEX, Object, normalized_map_cache)                  \
  V(RUNTIME_CONTEXT_INDEX, Context, runtime_context)                           \
  V(CALL_AS_FUNCTION_DELEGATE_INDEX, JSFunction, call_as_function_delegate)    \
  V(CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, JSFunction,                            \
    call_as_constructor_delegate)                                              \
  V(SCRIPT_FUNCTION_INDEX, JSFunction, script_function)                        \
  V(OPAQUE_REFERENCE_FUNCTION_INDEX, JSFunction, opaque_reference_function)    \
  V(CONTEXT_EXTENSION_FUNCTION_INDEX, JSFunction, context_extension_function)  \
  V(MAP_CACHE_INDEX, Object, map_cache)                                        \
  V(EMBEDDER_DATA_INDEX, FixedArray, embedder_data)                            \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings)    \
  V(ERROR_MESSAGE_FOR_CODE_GEN_FROM_STRINGS_INDEX, Object,                     \
    error_message_for_code_gen_from_strings)                                   \
  V(IS_PROMISE_INDEX, JSFunction, is_promise)                                  \
  V(PROMISE_CREATE_INDEX, JSFunction, promise_create)                          \
  V(PROMISE_RESOLVE_INDEX, JSFunction, promise_resolve)                        \
  V(PROMISE_REJECT_INDEX, JSFunction, promise_reject)                          \
  V(PROMISE_CHAIN_INDEX, JSFunction, promise_chain)                            \
  V(PROMISE_CATCH_INDEX, JSFunction, promise_catch)                            \
  V(PROMISE_THEN_INDEX, JSFunction, promise_then)                              \
  V(TO_COMPLETE_PROPERTY_DESCRIPTOR_INDEX, JSFunction,                         \
    to_complete_property_descriptor)                                           \
  V(DERIVED_HAS_TRAP_INDEX, JSFunction, derived_has_trap)                      \
  V(DERIVED_GET_TRAP_INDEX, JSFunction, derived_get_trap)                      \
  V(DERIVED_SET_TRAP_INDEX, JSFunction, derived_set_trap)                      \
  V(PROXY_ENUMERATE_INDEX, JSFunction, proxy_enumerate)                        \
  V(OBSERVERS_NOTIFY_CHANGE_INDEX, JSFunction, observers_notify_change)        \
  V(OBSERVERS_ENQUEUE_SPLICE_INDEX, JSFunction, observers_enqueue_splice)      \
  V(OBSERVERS_BEGIN_SPLICE_INDEX, JSFunction, observers_begin_perform_splice)  \
  V(OBSERVERS_END_SPLICE_INDEX, JSFunction, observers_end_perform_splice)      \
  V(NATIVE_OBJECT_OBSERVE_INDEX, JSFunction, native_object_observe)            \
  V(NATIVE_OBJECT_GET_NOTIFIER_INDEX, JSFunction, native_object_get_notifier)  \
  V(NATIVE_OBJECT_NOTIFIER_PERFORM_CHANGE, JSFunction,                         \
    native_object_notifier_perform_change)                                     \
  V(SLOPPY_GENERATOR_FUNCTION_MAP_INDEX, Map, sloppy_generator_function_map)   \
  V(STRICT_GENERATOR_FUNCTION_MAP_INDEX, Map, strict_generator_function_map)   \
  V(GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX, Map, generator_object_prototype_map) \
  V(ITERATOR_RESULT_MAP_INDEX, Map, iterator_result_map)                       \
  V(MAP_ITERATOR_MAP_INDEX, Map, map_iterator_map)                             \
  V(SET_ITERATOR_MAP_INDEX, Map, set_iterator_map)                             \
  V(ITERATOR_SYMBOL_INDEX, Symbol, iterator_symbol)                            \
  V(UNSCOPABLES_SYMBOL_INDEX, Symbol, unscopables_symbol)                      \
  V(ARRAY_VALUES_ITERATOR_INDEX, JSFunction, array_values_iterator)

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
// [ previous  ]  A pointer to the previous context. It is NULL for
//                function contexts, and non-NULL for 'with' contexts.
//                Used to implement the 'with' statement.
//
// [ extension ]  A pointer to an extension JSObject, or NULL. Used to
//                implement 'with' statements and dynamic declarations
//                (through 'eval'). The object in a 'with' statement is
//                stored in the extension slot of a 'with' context.
//                Dynamically declared variables/functions are also added
//                to lazily allocated extension object. Context::Lookup
//                searches the extension object for properties.
//                For global and block contexts, contains the respective
//                ScopeInfo.
//                For module contexts, points back to the respective JSModule.
//
// [ global_object ]  A pointer to the global object. Provided for quick
//                access to the global object from inside the code (since
//                we always have a context pointer).
//
// In addition, function contexts may have statically allocated context slots
// to store local variables/functions that are accessed from inner functions
// (via static context addresses) or through 'eval' (dynamic context lookups).
// The native context contains additional slots for fast access to native
// properties.
//
// Finally, with Harmony scoping, the JSFunction representing a top level
// script will have the GlobalContext rather than a FunctionContext.

class Context: public FixedArray {
 public:
  // Conversions.
  static Context* cast(Object* context) {
    DCHECK(context->IsContext());
    return reinterpret_cast<Context*>(context);
  }

  // The default context slot layout; indices are FixedArray slot indices.
  enum {
    // These slots are in all contexts.
    CLOSURE_INDEX,
    PREVIOUS_INDEX,
    // The extension slot is used for either the global object (in global
    // contexts), eval extension object (function contexts), subject of with
    // (with contexts), or the variable name (catch contexts), the serialized
    // scope info (block contexts), or the module instance (module contexts).
    EXTENSION_INDEX,
    GLOBAL_OBJECT_INDEX,
    MIN_CONTEXT_SLOTS,

    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,

    // These slots are only in native contexts.
    GLOBAL_PROXY_INDEX = MIN_CONTEXT_SLOTS,
    SECURITY_TOKEN_INDEX,
    SLOPPY_ARGUMENTS_MAP_INDEX,
    ALIASED_ARGUMENTS_MAP_INDEX,
    STRICT_ARGUMENTS_MAP_INDEX,
    REGEXP_RESULT_MAP_INDEX,
    SLOPPY_FUNCTION_MAP_INDEX,
    SLOPPY_FUNCTION_WITH_READONLY_PROTOTYPE_MAP_INDEX,
    STRICT_FUNCTION_MAP_INDEX,
    SLOPPY_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
    STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
    BOUND_FUNCTION_MAP_INDEX,
    INITIAL_OBJECT_PROTOTYPE_INDEX,
    INITIAL_ARRAY_PROTOTYPE_INDEX,
    BOOLEAN_FUNCTION_INDEX,
    NUMBER_FUNCTION_INDEX,
    STRING_FUNCTION_INDEX,
    STRING_FUNCTION_PROTOTYPE_MAP_INDEX,
    SYMBOL_FUNCTION_INDEX,
    OBJECT_FUNCTION_INDEX,
    INTERNAL_ARRAY_FUNCTION_INDEX,
    ARRAY_FUNCTION_INDEX,
    JS_ARRAY_MAPS_INDEX,
    DATE_FUNCTION_INDEX,
    JSON_OBJECT_INDEX,
    REGEXP_FUNCTION_INDEX,
    CREATE_DATE_FUN_INDEX,
    TO_NUMBER_FUN_INDEX,
    TO_STRING_FUN_INDEX,
    TO_DETAIL_STRING_FUN_INDEX,
    TO_OBJECT_FUN_INDEX,
    TO_INTEGER_FUN_INDEX,
    TO_UINT32_FUN_INDEX,
    TO_INT32_FUN_INDEX,
    TO_BOOLEAN_FUN_INDEX,
    GLOBAL_EVAL_FUN_INDEX,
    INSTANTIATE_FUN_INDEX,
    CONFIGURE_INSTANCE_FUN_INDEX,
    MATH_ABS_FUN_INDEX,
    MATH_ACOS_FUN_INDEX,
    MATH_ASIN_FUN_INDEX,
    MATH_ATAN_FUN_INDEX,
    MATH_ATAN2_FUN_INDEX,
    MATH_CEIL_FUN_INDEX,
    MATH_COS_FUN_INDEX,
    MATH_EXP_FUN_INDEX,
    MATH_FLOOR_FUN_INDEX,
    MATH_IMUL_FUN_INDEX,
    MATH_LOG_FUN_INDEX,
    MATH_MAX_FUN_INDEX,
    MATH_MIN_FUN_INDEX,
    MATH_POW_FUN_INDEX,
    MATH_RANDOM_FUN_INDEX,
    MATH_ROUND_FUN_INDEX,
    MATH_SIN_FUN_INDEX,
    MATH_SQRT_FUN_INDEX,
    MATH_TAN_FUN_INDEX,
    ARRAY_BUFFER_FUN_INDEX,
    UINT8_ARRAY_FUN_INDEX,
    INT8_ARRAY_FUN_INDEX,
    UINT16_ARRAY_FUN_INDEX,
    INT16_ARRAY_FUN_INDEX,
    UINT32_ARRAY_FUN_INDEX,
    INT32_ARRAY_FUN_INDEX,
    FLOAT32_ARRAY_FUN_INDEX,
    FLOAT64_ARRAY_FUN_INDEX,
    UINT8_CLAMPED_ARRAY_FUN_INDEX,
    INT8_ARRAY_EXTERNAL_MAP_INDEX,
    UINT8_ARRAY_EXTERNAL_MAP_INDEX,
    INT16_ARRAY_EXTERNAL_MAP_INDEX,
    UINT16_ARRAY_EXTERNAL_MAP_INDEX,
    INT32_ARRAY_EXTERNAL_MAP_INDEX,
    UINT32_ARRAY_EXTERNAL_MAP_INDEX,
    FLOAT32_ARRAY_EXTERNAL_MAP_INDEX,
    FLOAT64_ARRAY_EXTERNAL_MAP_INDEX,
    UINT8_CLAMPED_ARRAY_EXTERNAL_MAP_INDEX,
    DATA_VIEW_FUN_INDEX,
    MESSAGE_LISTENERS_INDEX,
    MAKE_MESSAGE_FUN_INDEX,
    GET_STACK_TRACE_LINE_INDEX,
    CONFIGURE_GLOBAL_INDEX,
    FUNCTION_CACHE_INDEX,
    JSFUNCTION_RESULT_CACHES_INDEX,
    NORMALIZED_MAP_CACHE_INDEX,
    RUNTIME_CONTEXT_INDEX,
    CALL_AS_FUNCTION_DELEGATE_INDEX,
    CALL_AS_CONSTRUCTOR_DELEGATE_INDEX,
    SCRIPT_FUNCTION_INDEX,
    OPAQUE_REFERENCE_FUNCTION_INDEX,
    CONTEXT_EXTENSION_FUNCTION_INDEX,
    OUT_OF_MEMORY_INDEX,
    EMBEDDER_DATA_INDEX,
    ALLOW_CODE_GEN_FROM_STRINGS_INDEX,
    ERROR_MESSAGE_FOR_CODE_GEN_FROM_STRINGS_INDEX,
    RUN_MICROTASKS_INDEX,
    ENQUEUE_MICROTASK_INDEX,
    IS_PROMISE_INDEX,
    PROMISE_CREATE_INDEX,
    PROMISE_RESOLVE_INDEX,
    PROMISE_REJECT_INDEX,
    PROMISE_CHAIN_INDEX,
    PROMISE_CATCH_INDEX,
    PROMISE_THEN_INDEX,
    TO_COMPLETE_PROPERTY_DESCRIPTOR_INDEX,
    DERIVED_HAS_TRAP_INDEX,
    DERIVED_GET_TRAP_INDEX,
    DERIVED_SET_TRAP_INDEX,
    PROXY_ENUMERATE_INDEX,
    OBSERVERS_NOTIFY_CHANGE_INDEX,
    OBSERVERS_ENQUEUE_SPLICE_INDEX,
    OBSERVERS_BEGIN_SPLICE_INDEX,
    OBSERVERS_END_SPLICE_INDEX,
    NATIVE_OBJECT_OBSERVE_INDEX,
    NATIVE_OBJECT_GET_NOTIFIER_INDEX,
    NATIVE_OBJECT_NOTIFIER_PERFORM_CHANGE,
    SLOPPY_GENERATOR_FUNCTION_MAP_INDEX,
    STRICT_GENERATOR_FUNCTION_MAP_INDEX,
    GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX,
    ITERATOR_RESULT_MAP_INDEX,
    MAP_ITERATOR_MAP_INDEX,
    SET_ITERATOR_MAP_INDEX,
    ITERATOR_SYMBOL_INDEX,
    UNSCOPABLES_SYMBOL_INDEX,
    ARRAY_VALUES_ITERATOR_INDEX,

    // Properties from here are treated as weak references by the full GC.
    // Scavenge treats them as strong references.
    OPTIMIZED_FUNCTIONS_LIST,  // Weak.
    OPTIMIZED_CODE_LIST,       // Weak.
    DEOPTIMIZED_CODE_LIST,     // Weak.
    MAP_CACHE_INDEX,           // Weak.
    NEXT_CONTEXT_LINK,         // Weak.

    // Total number of slots.
    NATIVE_CONTEXT_SLOTS,
    FIRST_WEAK_SLOT = OPTIMIZED_FUNCTIONS_LIST
  };

  // Direct slot access.
  JSFunction* closure() { return JSFunction::cast(get(CLOSURE_INDEX)); }
  void set_closure(JSFunction* closure) { set(CLOSURE_INDEX, closure); }

  Context* previous() {
    Object* result = unchecked_previous();
    DCHECK(IsBootstrappingOrValidParentContext(result, this));
    return reinterpret_cast<Context*>(result);
  }
  void set_previous(Context* context) { set(PREVIOUS_INDEX, context); }

  bool has_extension() { return extension() != NULL; }
  Object* extension() { return get(EXTENSION_INDEX); }
  void set_extension(Object* object) { set(EXTENSION_INDEX, object); }

  JSModule* module() { return JSModule::cast(get(EXTENSION_INDEX)); }
  void set_module(JSModule* module) { set(EXTENSION_INDEX, module); }

  // Get the context where var declarations will be hoisted to, which
  // may be the context itself.
  Context* declaration_context();

  GlobalObject* global_object() {
    Object* result = get(GLOBAL_OBJECT_INDEX);
    DCHECK(IsBootstrappingOrGlobalObject(this->GetIsolate(), result));
    return reinterpret_cast<GlobalObject*>(result);
  }
  void set_global_object(GlobalObject* object) {
    set(GLOBAL_OBJECT_INDEX, object);
  }

  // Returns a JSGlobalProxy object or null.
  JSObject* global_proxy();
  void set_global_proxy(JSObject* global);

  // The builtins object.
  JSBuiltinsObject* builtins();

  // Get the innermost global context by traversing the context chain.
  Context* global_context();

  // Compute the native context by traversing the context chain.
  Context* native_context();

  // Predicates for context types.  IsNativeContext is also defined on Object
  // because we frequently have to know if arbitrary objects are natives
  // contexts.
  bool IsNativeContext() {
    Map* map = this->map();
    return map == map->GetHeap()->native_context_map();
  }
  bool IsFunctionContext() {
    Map* map = this->map();
    return map == map->GetHeap()->function_context_map();
  }
  bool IsCatchContext() {
    Map* map = this->map();
    return map == map->GetHeap()->catch_context_map();
  }
  bool IsWithContext() {
    Map* map = this->map();
    return map == map->GetHeap()->with_context_map();
  }
  bool IsBlockContext() {
    Map* map = this->map();
    return map == map->GetHeap()->block_context_map();
  }
  bool IsModuleContext() {
    Map* map = this->map();
    return map == map->GetHeap()->module_context_map();
  }
  bool IsGlobalContext() {
    Map* map = this->map();
    return map == map->GetHeap()->global_context_map();
  }

  bool HasSameSecurityTokenAs(Context* that) {
    return this->global_object()->native_context()->security_token() ==
        that->global_object()->native_context()->security_token();
  }

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

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void  set_##name(type* value) {                         \
    DCHECK(IsNativeContext());                            \
    set(index, value);                                    \
  }                                                       \
  bool is_##name(type* value) {                           \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index)) == value;               \
  }                                                       \
  type* name() {                                          \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index));                        \
  }
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
  Handle<Object> Lookup(Handle<String> name,
                        ContextLookupFlags flags,
                        int* index,
                        PropertyAttributes* attributes,
                        BindingFlags* binding_flags);

  // Code generation support.
  static int SlotOffset(int index) {
    return kHeaderSize + index * kPointerSize - kHeapObjectTag;
  }

  static int FunctionMapIndex(StrictMode strict_mode, FunctionKind kind) {
    if (IsGeneratorFunction(kind)) {
      return strict_mode == SLOPPY ? SLOPPY_GENERATOR_FUNCTION_MAP_INDEX
                                   : STRICT_GENERATOR_FUNCTION_MAP_INDEX;
    }

    if (IsArrowFunction(kind) || IsConciseMethod(kind)) {
      return strict_mode == SLOPPY
                 ? SLOPPY_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX
                 : STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX;
    }

    return strict_mode == SLOPPY ? SLOPPY_FUNCTION_MAP_INDEX
                                 : STRICT_FUNCTION_MAP_INDEX;
  }

  static const int kSize = kHeaderSize + NATIVE_CONTEXT_SLOTS * kPointerSize;

  // GC support.
  typedef FixedBodyDescriptor<
      kHeaderSize, kSize, kSize> ScavengeBodyDescriptor;

  typedef FixedBodyDescriptor<
      kHeaderSize,
      kHeaderSize + FIRST_WEAK_SLOT * kPointerSize,
      kSize> MarkCompactBodyDescriptor;

 private:
  // Unchecked access to the slots.
  Object* unchecked_previous() { return get(PREVIOUS_INDEX); }

#ifdef DEBUG
  // Bootstrapping-aware type checks.
  static bool IsBootstrappingOrValidParentContext(Object* object, Context* kid);
  static bool IsBootstrappingOrGlobalObject(Isolate* isolate, Object* object);
#endif

  STATIC_ASSERT(kHeaderSize == Internals::kContextHeaderSize);
  STATIC_ASSERT(EMBEDDER_DATA_INDEX == Internals::kContextEmbedderDataIndex);
};

} }  // namespace v8::internal

#endif  // V8_CONTEXTS_H_
