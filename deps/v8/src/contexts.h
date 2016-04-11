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

#define NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(V)                             \
  V(IS_ARRAYLIKE, JSFunction, is_arraylike)                               \
  V(CONCAT_ITERABLE_TO_ARRAY_INDEX, JSFunction, concat_iterable_to_array) \
  V(GET_TEMPLATE_CALL_SITE_INDEX, JSFunction, get_template_call_site)     \
  V(MAKE_RANGE_ERROR_INDEX, JSFunction, make_range_error)                 \
  V(MAKE_TYPE_ERROR_INDEX, JSFunction, make_type_error)                   \
  V(OBJECT_FREEZE, JSFunction, object_freeze)                             \
  V(OBJECT_IS_EXTENSIBLE, JSFunction, object_is_extensible)               \
  V(OBJECT_IS_FROZEN, JSFunction, object_is_frozen)                       \
  V(OBJECT_IS_SEALED, JSFunction, object_is_sealed)                       \
  V(OBJECT_KEYS, JSFunction, object_keys)                                 \
  V(REFLECT_APPLY_INDEX, JSFunction, reflect_apply)                       \
  V(REFLECT_CONSTRUCT_INDEX, JSFunction, reflect_construct)               \
  V(REFLECT_DEFINE_PROPERTY_INDEX, JSFunction, reflect_define_property)   \
  V(REFLECT_DELETE_PROPERTY_INDEX, JSFunction, reflect_delete_property)   \
  V(SPREAD_ARGUMENTS_INDEX, JSFunction, spread_arguments)                 \
  V(SPREAD_ITERABLE_INDEX, JSFunction, spread_iterable)


#define NATIVE_CONTEXT_JS_BUILTINS(V)                   \
  V(CONCAT_ITERABLE_TO_ARRAY_BUILTIN_INDEX, JSFunction, \
    concat_iterable_to_array_builtin)


#define NATIVE_CONTEXT_IMPORTED_FIELDS(V)                                     \
  V(ARRAY_CONCAT_INDEX, JSFunction, array_concat)                             \
  V(ARRAY_POP_INDEX, JSFunction, array_pop)                                   \
  V(ARRAY_PUSH_INDEX, JSFunction, array_push)                                 \
  V(ARRAY_SHIFT_INDEX, JSFunction, array_shift)                               \
  V(ARRAY_SPLICE_INDEX, JSFunction, array_splice)                             \
  V(ARRAY_SLICE_INDEX, JSFunction, array_slice)                               \
  V(ARRAY_UNSHIFT_INDEX, JSFunction, array_unshift)                           \
  V(ARRAY_VALUES_ITERATOR_INDEX, JSFunction, array_values_iterator)           \
  V(DERIVED_GET_TRAP_INDEX, JSFunction, derived_get_trap)                     \
  V(ERROR_FUNCTION_INDEX, JSFunction, error_function)                         \
  V(EVAL_ERROR_FUNCTION_INDEX, JSFunction, eval_error_function)               \
  V(GET_STACK_TRACE_LINE_INDEX, JSFunction, get_stack_trace_line_fun)         \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun)                       \
  V(JSON_SERIALIZE_ADAPTER_INDEX, JSFunction, json_serialize_adapter)         \
  V(MAKE_ERROR_FUNCTION_INDEX, JSFunction, make_error_function)               \
  V(MAP_DELETE_METHOD_INDEX, JSFunction, map_delete)                          \
  V(MAP_GET_METHOD_INDEX, JSFunction, map_get)                                \
  V(MAP_HAS_METHOD_INDEX, JSFunction, map_has)                                \
  V(MAP_SET_METHOD_INDEX, JSFunction, map_set)                                \
  V(MESSAGE_GET_COLUMN_NUMBER_INDEX, JSFunction, message_get_column_number)   \
  V(MESSAGE_GET_LINE_NUMBER_INDEX, JSFunction, message_get_line_number)       \
  V(MESSAGE_GET_SOURCE_LINE_INDEX, JSFunction, message_get_source_line)       \
  V(NATIVE_OBJECT_GET_NOTIFIER_INDEX, JSFunction, native_object_get_notifier) \
  V(NATIVE_OBJECT_NOTIFIER_PERFORM_CHANGE, JSFunction,                        \
    native_object_notifier_perform_change)                                    \
  V(NATIVE_OBJECT_OBSERVE_INDEX, JSFunction, native_object_observe)           \
  V(NO_SIDE_EFFECTS_TO_STRING_FUN_INDEX, JSFunction,                          \
    no_side_effects_to_string_fun)                                            \
  V(OBJECT_VALUE_OF, JSFunction, object_value_of)                             \
  V(OBJECT_TO_STRING, JSFunction, object_to_string)                           \
  V(OBSERVERS_BEGIN_SPLICE_INDEX, JSFunction, observers_begin_perform_splice) \
  V(OBSERVERS_END_SPLICE_INDEX, JSFunction, observers_end_perform_splice)     \
  V(OBSERVERS_ENQUEUE_SPLICE_INDEX, JSFunction, observers_enqueue_splice)     \
  V(OBSERVERS_NOTIFY_CHANGE_INDEX, JSFunction, observers_notify_change)       \
  V(PROMISE_CATCH_INDEX, JSFunction, promise_catch)                           \
  V(PROMISE_CHAIN_INDEX, JSFunction, promise_chain)                           \
  V(PROMISE_CREATE_INDEX, JSFunction, promise_create)                         \
  V(PROMISE_FUNCTION_INDEX, JSFunction, promise_function)                     \
  V(PROMISE_HAS_USER_DEFINED_REJECT_HANDLER_INDEX, JSFunction,                \
    promise_has_user_defined_reject_handler)                                  \
  V(PROMISE_REJECT_INDEX, JSFunction, promise_reject)                         \
  V(PROMISE_RESOLVE_INDEX, JSFunction, promise_resolve)                       \
  V(PROMISE_THEN_INDEX, JSFunction, promise_then)                             \
  V(PROXY_ENUMERATE_INDEX, JSFunction, proxy_enumerate)                       \
  V(RANGE_ERROR_FUNCTION_INDEX, JSFunction, range_error_function)             \
  V(REFERENCE_ERROR_FUNCTION_INDEX, JSFunction, reference_error_function)     \
  V(SET_ADD_METHOD_INDEX, JSFunction, set_add)                                \
  V(SET_DELETE_METHOD_INDEX, JSFunction, set_delete)                          \
  V(SET_HAS_METHOD_INDEX, JSFunction, set_has)                                \
  V(STACK_OVERFLOW_BOILERPLATE_INDEX, JSObject, stack_overflow_boilerplate)   \
  V(SYNTAX_ERROR_FUNCTION_INDEX, JSFunction, syntax_error_function)           \
  V(TYPE_ERROR_FUNCTION_INDEX, JSFunction, type_error_function)               \
  V(URI_ERROR_FUNCTION_INDEX, JSFunction, uri_error_function)                 \
  NATIVE_CONTEXT_JS_BUILTINS(V)

#define NATIVE_CONTEXT_FIELDS(V)                                               \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object)                         \
  V(EMBEDDER_DATA_INDEX, FixedArray, embedder_data)                            \
  /* Below is alpha-sorted */                                                  \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings)    \
  V(ARRAY_BUFFER_FUN_INDEX, JSFunction, array_buffer_fun)                      \
  V(ARRAY_BUFFER_MAP_INDEX, Map, array_buffer_map)                             \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function)                          \
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
  V(CONTEXT_EXTENSION_FUNCTION_INDEX, JSFunction, context_extension_function)  \
  V(DATA_VIEW_FUN_INDEX, JSFunction, data_view_fun)                            \
  V(DATE_FUNCTION_INDEX, JSFunction, date_function)                            \
  V(ERROR_MESSAGE_FOR_CODE_GEN_FROM_STRINGS_INDEX, Object,                     \
    error_message_for_code_gen_from_strings)                                   \
  V(ERRORS_THROWN_INDEX, Smi, errors_thrown)                                   \
  V(EXTRAS_EXPORTS_OBJECT_INDEX, JSObject, extras_binding_object)              \
  V(EXTRAS_UTILS_OBJECT_INDEX, JSObject, extras_utils_object)                  \
  V(FAST_ALIASED_ARGUMENTS_MAP_INDEX, Map, fast_aliased_arguments_map)         \
  V(FLOAT32_ARRAY_FUN_INDEX, JSFunction, float32_array_fun)                    \
  V(FLOAT32X4_FUNCTION_INDEX, JSFunction, float32x4_function)                  \
  V(FLOAT64_ARRAY_FUN_INDEX, JSFunction, float64_array_fun)                    \
  V(FUNCTION_CACHE_INDEX, ObjectHashTable, function_cache)                     \
  V(FUNCTION_FUNCTION_INDEX, JSFunction, function_function)                    \
  V(GENERATOR_FUNCTION_FUNCTION_INDEX, JSFunction,                             \
    generator_function_function)                                               \
  V(GENERATOR_OBJECT_PROTOTYPE_MAP_INDEX, Map, generator_object_prototype_map) \
  V(INITIAL_ARRAY_PROTOTYPE_INDEX, JSObject, initial_array_prototype)          \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype)        \
  V(INT16_ARRAY_FUN_INDEX, JSFunction, int16_array_fun)                        \
  V(INT16X8_FUNCTION_INDEX, JSFunction, int16x8_function)                      \
  V(INT32_ARRAY_FUN_INDEX, JSFunction, int32_array_fun)                        \
  V(INT32X4_FUNCTION_INDEX, JSFunction, int32x4_function)                      \
  V(INT8_ARRAY_FUN_INDEX, JSFunction, int8_array_fun)                          \
  V(INT8X16_FUNCTION_INDEX, JSFunction, int8x16_function)                      \
  V(INTERNAL_ARRAY_FUNCTION_INDEX, JSFunction, internal_array_function)        \
  V(ITERATOR_RESULT_MAP_INDEX, Map, iterator_result_map)                       \
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
  V(JS_ARRAY_FAST_SMI_ELEMENTS_STRONG_MAP_INDEX, Map,                          \
    js_array_fast_smi_elements_strong_map_index)                               \
  V(JS_ARRAY_FAST_HOLEY_SMI_ELEMENTS_STRONG_MAP_INDEX, Map,                    \
    js_array_fast_holey_smi_elements_strong_map_index)                         \
  V(JS_ARRAY_FAST_ELEMENTS_STRONG_MAP_INDEX, Map,                              \
    js_array_fast_elements_strong_map_index)                                   \
  V(JS_ARRAY_FAST_HOLEY_ELEMENTS_STRONG_MAP_INDEX, Map,                        \
    js_array_fast_holey_elements_strong_map_index)                             \
  V(JS_ARRAY_FAST_DOUBLE_ELEMENTS_STRONG_MAP_INDEX, Map,                       \
    js_array_fast_double_elements_strong_map_index)                            \
  V(JS_ARRAY_FAST_HOLEY_DOUBLE_ELEMENTS_STRONG_MAP_INDEX, Map,                 \
    js_array_fast_holey_double_elements_strong_map_index)                      \
  V(JS_MAP_FUN_INDEX, JSFunction, js_map_fun)                                  \
  V(JS_MAP_MAP_INDEX, Map, js_map_map)                                         \
  V(JS_OBJECT_STRONG_MAP_INDEX, Map, js_object_strong_map)                     \
  V(JS_SET_FUN_INDEX, JSFunction, js_set_fun)                                  \
  V(JS_SET_MAP_INDEX, Map, js_set_map)                                         \
  V(JS_WEAK_MAP_FUN_INDEX, JSFunction, js_weak_map_fun)                        \
  V(JS_WEAK_SET_FUN_INDEX, JSFunction, js_weak_set_fun)                        \
  V(MAP_CACHE_INDEX, Object, map_cache)                                        \
  V(MAP_ITERATOR_MAP_INDEX, Map, map_iterator_map)                             \
  V(STRING_ITERATOR_MAP_INDEX, Map, string_iterator_map)                       \
  V(MESSAGE_LISTENERS_INDEX, JSObject, message_listeners)                      \
  V(NATIVES_UTILS_OBJECT_INDEX, Object, natives_utils_object)                  \
  V(NORMALIZED_MAP_CACHE_INDEX, Object, normalized_map_cache)                  \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function)                        \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function)                        \
  V(OBJECT_FUNCTION_PROTOTYPE_MAP_INDEX, Map, object_function_prototype_map)   \
  V(OPAQUE_REFERENCE_FUNCTION_INDEX, JSFunction, opaque_reference_function)    \
  V(PROXY_CALLABLE_MAP_INDEX, Map, proxy_callable_map)                         \
  V(PROXY_CONSTRUCTOR_MAP_INDEX, Map, proxy_constructor_map)                   \
  V(PROXY_FUNCTION_INDEX, JSFunction, proxy_function)                          \
  V(PROXY_FUNCTION_MAP_INDEX, Map, proxy_function_map)                         \
  V(PROXY_MAP_INDEX, Map, proxy_map)                                           \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function)                        \
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
  V(WASM_FUNCTION_MAP_INDEX, Map, wasm_function_map)                           \
  V(SLOPPY_GENERATOR_FUNCTION_MAP_INDEX, Map, sloppy_generator_function_map)   \
  V(SLOW_ALIASED_ARGUMENTS_MAP_INDEX, Map, slow_aliased_arguments_map)         \
  V(STRICT_ARGUMENTS_MAP_INDEX, Map, strict_arguments_map)                     \
  V(STRICT_FUNCTION_MAP_INDEX, Map, strict_function_map)                       \
  V(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map,                          \
    strict_function_without_prototype_map)                                     \
  V(STRICT_GENERATOR_FUNCTION_MAP_INDEX, Map, strict_generator_function_map)   \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function)                        \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map)   \
  V(STRONG_CONSTRUCTOR_MAP_INDEX, Map, strong_constructor_map)                 \
  V(STRONG_FUNCTION_MAP_INDEX, Map, strong_function_map)                       \
  V(STRONG_GENERATOR_FUNCTION_MAP_INDEX, Map, strong_generator_function_map)   \
  V(STRONG_MAP_CACHE_INDEX, Object, strong_map_cache)                          \
  V(SYMBOL_FUNCTION_INDEX, JSFunction, symbol_function)                        \
  V(UINT16_ARRAY_FUN_INDEX, JSFunction, uint16_array_fun)                      \
  V(UINT16X8_FUNCTION_INDEX, JSFunction, uint16x8_function)                    \
  V(UINT32_ARRAY_FUN_INDEX, JSFunction, uint32_array_fun)                      \
  V(UINT32X4_FUNCTION_INDEX, JSFunction, uint32x4_function)                    \
  V(UINT8_ARRAY_FUN_INDEX, JSFunction, uint8_array_fun)                        \
  V(UINT8_CLAMPED_ARRAY_FUN_INDEX, JSFunction, uint8_clamped_array_fun)        \
  V(UINT8X16_FUNCTION_INDEX, JSFunction, uint8x16_function)                    \
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
// [ previous  ]  A pointer to the previous context. It is NULL for
//                function contexts, and non-NULL for 'with' contexts.
//                Used to implement the 'with' statement.
//
// [ extension ]  A pointer to an extension JSObject, or "the hole". Used to
//                implement 'with' statements and dynamic declarations
//                (through 'eval'). The object in a 'with' statement is
//                stored in the extension slot of a 'with' context.
//                Dynamically declared variables/functions are also added
//                to lazily allocated extension object. Context::Lookup
//                searches the extension object for properties.
//                For script and block contexts, contains the respective
//                ScopeInfo. For block contexts representing sloppy declaration
//                block scopes, it may also be a struct being a
//                SloppyBlockWithEvalContextExtension, pairing the ScopeInfo
//                with an extension object.
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
// script will have the ScriptContext rather than a FunctionContext.
// Script contexts from all top-level scripts are gathered in
// ScriptContextTable.

class Context: public FixedArray {
 public:
  // Conversions.
  static inline Context* cast(Object* context);

  // The default context slot layout; indices are FixedArray slot indices.
  enum {
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
    FIRST_JS_ARRAY_STRONG_MAP_SLOT =
        JS_ARRAY_FAST_SMI_ELEMENTS_STRONG_MAP_INDEX,

    MIN_CONTEXT_SLOTS = GLOBAL_PROXY_INDEX,
    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,
  };

  void IncrementErrorsThrown();
  int GetErrorsThrown();

  // Direct slot access.
  inline JSFunction* closure();
  inline void set_closure(JSFunction* closure);

  inline Context* previous();
  inline void set_previous(Context* context);

  inline bool has_extension();
  inline HeapObject* extension();
  inline void set_extension(HeapObject* object);
  JSObject* extension_object();
  JSReceiver* extension_receiver();
  ScopeInfo* scope_info();
  String* catch_name();

  inline JSModule* module();
  inline void set_module(JSModule* module);

  // Get the context where var declarations will be hoisted to, which
  // may be the context itself.
  Context* declaration_context();
  bool is_declaration_context();

  // Returns a JSGlobalProxy object or null.
  JSObject* global_proxy();
  void set_global_proxy(JSObject* global);

  // Get the JSGlobalObject object.
  JSGlobalObject* global_object();

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
  inline bool IsBlockContext();
  inline bool IsModuleContext();
  inline bool IsScriptContext();

  inline bool HasSameSecurityTokenAs(Context* that);

  // Initializes global variable bindings in given script context.
  void InitializeGlobalSlots();

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

  static bool IsJSBuiltin(Handle<Context> native_context,
                          Handle<JSFunction> function);

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
  Handle<Object> Lookup(Handle<String> name,
                        ContextLookupFlags flags,
                        int* index,
                        PropertyAttributes* attributes,
                        BindingFlags* binding_flags);

  // Code generation support.
  static int SlotOffset(int index) {
    return kHeaderSize + index * kPointerSize - kHeapObjectTag;
  }

  static int FunctionMapIndex(LanguageMode language_mode, FunctionKind kind) {
    if (IsGeneratorFunction(kind)) {
      return is_strong(language_mode) ? STRONG_GENERATOR_FUNCTION_MAP_INDEX :
             is_strict(language_mode) ? STRICT_GENERATOR_FUNCTION_MAP_INDEX
                                      : SLOPPY_GENERATOR_FUNCTION_MAP_INDEX;
    }

    if (IsClassConstructor(kind)) {
      // Use strict function map (no own "caller" / "arguments")
      return is_strong(language_mode) ? STRONG_CONSTRUCTOR_MAP_INDEX
                                      : STRICT_FUNCTION_MAP_INDEX;
    }

    if (IsArrowFunction(kind) || IsConciseMethod(kind) ||
        IsAccessorFunction(kind)) {
      return is_strong(language_mode)
                 ? STRONG_FUNCTION_MAP_INDEX
                 : STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX;
    }

    return is_strong(language_mode) ? STRONG_FUNCTION_MAP_INDEX :
           is_strict(language_mode) ? STRICT_FUNCTION_MAP_INDEX
                                    : SLOPPY_FUNCTION_MAP_INDEX;
  }

  static int ArrayMapIndex(ElementsKind elements_kind,
                           Strength strength = Strength::WEAK) {
    DCHECK(IsFastElementsKind(elements_kind));
    return elements_kind + (is_strong(strength) ? FIRST_JS_ARRAY_STRONG_MAP_SLOT
                                                : FIRST_JS_ARRAY_MAP_SLOT);
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
  static bool IsBootstrappingOrNativeContext(Isolate* isolate, Object* object);
  static bool IsBootstrappingOrValidParentContext(Object* object, Context* kid);
#endif

  STATIC_ASSERT(kHeaderSize == Internals::kContextHeaderSize);
  STATIC_ASSERT(EMBEDDER_DATA_INDEX == Internals::kContextEmbedderDataIndex);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CONTEXTS_H_
