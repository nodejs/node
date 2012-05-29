// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_CONTEXTS_H_
#define V8_CONTEXTS_H_

#include "heap.h"
#include "objects.h"

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

#define GLOBAL_CONTEXT_FIELDS(V) \
  V(GLOBAL_PROXY_INDEX, JSObject, global_proxy_object) \
  V(SECURITY_TOKEN_INDEX, Object, security_token) \
  V(BOOLEAN_FUNCTION_INDEX, JSFunction, boolean_function) \
  V(NUMBER_FUNCTION_INDEX, JSFunction, number_function) \
  V(STRING_FUNCTION_INDEX, JSFunction, string_function) \
  V(STRING_FUNCTION_PROTOTYPE_MAP_INDEX, Map, string_function_prototype_map) \
  V(OBJECT_FUNCTION_INDEX, JSFunction, object_function) \
  V(INTERNAL_ARRAY_FUNCTION_INDEX, JSFunction, internal_array_function) \
  V(ARRAY_FUNCTION_INDEX, JSFunction, array_function) \
  V(JS_ARRAY_MAPS_INDEX, Object, js_array_maps) \
  V(DATE_FUNCTION_INDEX, JSFunction, date_function) \
  V(JSON_OBJECT_INDEX, JSObject, json_object) \
  V(REGEXP_FUNCTION_INDEX, JSFunction, regexp_function) \
  V(INITIAL_OBJECT_PROTOTYPE_INDEX, JSObject, initial_object_prototype) \
  V(CREATE_DATE_FUN_INDEX, JSFunction,  create_date_fun) \
  V(TO_NUMBER_FUN_INDEX, JSFunction, to_number_fun) \
  V(TO_STRING_FUN_INDEX, JSFunction, to_string_fun) \
  V(TO_DETAIL_STRING_FUN_INDEX, JSFunction, to_detail_string_fun) \
  V(TO_OBJECT_FUN_INDEX, JSFunction, to_object_fun) \
  V(TO_INTEGER_FUN_INDEX, JSFunction, to_integer_fun) \
  V(TO_UINT32_FUN_INDEX, JSFunction, to_uint32_fun) \
  V(TO_INT32_FUN_INDEX, JSFunction, to_int32_fun) \
  V(GLOBAL_EVAL_FUN_INDEX, JSFunction, global_eval_fun) \
  V(INSTANTIATE_FUN_INDEX, JSFunction, instantiate_fun) \
  V(CONFIGURE_INSTANCE_FUN_INDEX, JSFunction, configure_instance_fun) \
  V(FUNCTION_MAP_INDEX, Map, function_map) \
  V(STRICT_MODE_FUNCTION_MAP_INDEX, Map, strict_mode_function_map) \
  V(FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map, function_without_prototype_map) \
  V(STRICT_MODE_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX, Map, \
    strict_mode_function_without_prototype_map) \
  V(FUNCTION_INSTANCE_MAP_INDEX, Map, function_instance_map) \
  V(STRICT_MODE_FUNCTION_INSTANCE_MAP_INDEX, Map, \
    strict_mode_function_instance_map) \
  V(REGEXP_RESULT_MAP_INDEX, Map, regexp_result_map)\
  V(ARGUMENTS_BOILERPLATE_INDEX, JSObject, arguments_boilerplate) \
  V(ALIASED_ARGUMENTS_BOILERPLATE_INDEX, JSObject, \
    aliased_arguments_boilerplate) \
  V(STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX, JSObject, \
    strict_mode_arguments_boilerplate) \
  V(MESSAGE_LISTENERS_INDEX, JSObject, message_listeners) \
  V(MAKE_MESSAGE_FUN_INDEX, JSFunction, make_message_fun) \
  V(GET_STACK_TRACE_LINE_INDEX, JSFunction, get_stack_trace_line_fun) \
  V(CONFIGURE_GLOBAL_INDEX, JSFunction, configure_global_fun) \
  V(FUNCTION_CACHE_INDEX, JSObject, function_cache) \
  V(JSFUNCTION_RESULT_CACHES_INDEX, FixedArray, jsfunction_result_caches) \
  V(NORMALIZED_MAP_CACHE_INDEX, NormalizedMapCache, normalized_map_cache) \
  V(RUNTIME_CONTEXT_INDEX, Context, runtime_context) \
  V(CALL_AS_FUNCTION_DELEGATE_INDEX, JSFunction, call_as_function_delegate) \
  V(CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, JSFunction, \
    call_as_constructor_delegate) \
  V(SCRIPT_FUNCTION_INDEX, JSFunction, script_function) \
  V(OPAQUE_REFERENCE_FUNCTION_INDEX, JSFunction, opaque_reference_function) \
  V(CONTEXT_EXTENSION_FUNCTION_INDEX, JSFunction, context_extension_function) \
  V(OUT_OF_MEMORY_INDEX, Object, out_of_memory) \
  V(MAP_CACHE_INDEX, Object, map_cache) \
  V(CONTEXT_DATA_INDEX, Object, data) \
  V(ALLOW_CODE_GEN_FROM_STRINGS_INDEX, Object, allow_code_gen_from_strings) \
  V(TO_COMPLETE_PROPERTY_DESCRIPTOR_INDEX, JSFunction, \
    to_complete_property_descriptor) \
  V(DERIVED_HAS_TRAP_INDEX, JSFunction, derived_has_trap) \
  V(DERIVED_GET_TRAP_INDEX, JSFunction, derived_get_trap) \
  V(DERIVED_SET_TRAP_INDEX, JSFunction, derived_set_trap) \
  V(PROXY_ENUMERATE, JSFunction, proxy_enumerate) \
  V(RANDOM_SEED_INDEX, ByteArray, random_seed)

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
//
// [ global    ]  A pointer to the global object. Provided for quick
//                access to the global object from inside the code (since
//                we always have a context pointer).
//
// In addition, function contexts may have statically allocated context slots
// to store local variables/functions that are accessed from inner functions
// (via static context addresses) or through 'eval' (dynamic context lookups).
// Finally, the global context contains additional slots for fast access to
// global properties.

class Context: public FixedArray {
 public:
  // Conversions.
  static Context* cast(Object* context) {
    ASSERT(context->IsContext());
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
    // scope info (block contexts).
    EXTENSION_INDEX,
    GLOBAL_INDEX,
    MIN_CONTEXT_SLOTS,

    // This slot holds the thrown value in catch contexts.
    THROWN_OBJECT_INDEX = MIN_CONTEXT_SLOTS,

    // These slots are only in global contexts.
    GLOBAL_PROXY_INDEX = MIN_CONTEXT_SLOTS,
    SECURITY_TOKEN_INDEX,
    ARGUMENTS_BOILERPLATE_INDEX,
    ALIASED_ARGUMENTS_BOILERPLATE_INDEX,
    STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX,
    REGEXP_RESULT_MAP_INDEX,
    FUNCTION_MAP_INDEX,
    STRICT_MODE_FUNCTION_MAP_INDEX,
    FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
    STRICT_MODE_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
    FUNCTION_INSTANCE_MAP_INDEX,
    STRICT_MODE_FUNCTION_INSTANCE_MAP_INDEX,
    INITIAL_OBJECT_PROTOTYPE_INDEX,
    BOOLEAN_FUNCTION_INDEX,
    NUMBER_FUNCTION_INDEX,
    STRING_FUNCTION_INDEX,
    STRING_FUNCTION_PROTOTYPE_MAP_INDEX,
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
    CONTEXT_DATA_INDEX,
    ALLOW_CODE_GEN_FROM_STRINGS_INDEX,
    TO_COMPLETE_PROPERTY_DESCRIPTOR_INDEX,
    DERIVED_HAS_TRAP_INDEX,
    DERIVED_GET_TRAP_INDEX,
    DERIVED_SET_TRAP_INDEX,
    PROXY_ENUMERATE,
    RANDOM_SEED_INDEX,

    // Properties from here are treated as weak references by the full GC.
    // Scavenge treats them as strong references.
    OPTIMIZED_FUNCTIONS_LIST,  // Weak.
    MAP_CACHE_INDEX,  // Weak.
    NEXT_CONTEXT_LINK,  // Weak.

    // Total number of slots.
    GLOBAL_CONTEXT_SLOTS,

    FIRST_WEAK_SLOT = OPTIMIZED_FUNCTIONS_LIST
  };

  // Direct slot access.
  JSFunction* closure() { return JSFunction::cast(get(CLOSURE_INDEX)); }
  void set_closure(JSFunction* closure) { set(CLOSURE_INDEX, closure); }

  Context* previous() {
    Object* result = unchecked_previous();
    ASSERT(IsBootstrappingOrContext(result));
    return reinterpret_cast<Context*>(result);
  }
  void set_previous(Context* context) { set(PREVIOUS_INDEX, context); }

  bool has_extension() { return extension() != NULL; }
  Object* extension() { return get(EXTENSION_INDEX); }
  void set_extension(Object* object) { set(EXTENSION_INDEX, object); }

  // Get the context where var declarations will be hoisted to, which
  // may be the context itself.
  Context* declaration_context();

  GlobalObject* global() {
    Object* result = get(GLOBAL_INDEX);
    ASSERT(IsBootstrappingOrGlobalObject(result));
    return reinterpret_cast<GlobalObject*>(result);
  }
  void set_global(GlobalObject* global) { set(GLOBAL_INDEX, global); }

  // Returns a JSGlobalProxy object or null.
  JSObject* global_proxy();
  void set_global_proxy(JSObject* global);

  // The builtins object.
  JSBuiltinsObject* builtins();

  // Compute the global context by traversing the context chain.
  Context* global_context();

  // Predicates for context types.  IsGlobalContext is defined on Object
  // because we frequently have to know if arbitrary objects are global
  // contexts.
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

  // Tells whether the global context is marked with out of memory.
  inline bool has_out_of_memory();

  // Mark the global context with out of memory.
  inline void mark_out_of_memory();

  // A global context hold a list of all functions which have been optimized.
  void AddOptimizedFunction(JSFunction* function);
  void RemoveOptimizedFunction(JSFunction* function);
  Object* OptimizedFunctionsListHead();
  void ClearOptimizedFunctions();

#define GLOBAL_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void  set_##name(type* value) {                         \
    ASSERT(IsGlobalContext());                            \
    set(index, value);                                    \
  }                                                       \
  type* name() {                                          \
    ASSERT(IsGlobalContext());                            \
    return type::cast(get(index));                        \
  }
  GLOBAL_CONTEXT_FIELDS(GLOBAL_CONTEXT_FIELD_ACCESSORS)
#undef GLOBAL_CONTEXT_FIELD_ACCESSORS

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

  static const int kSize = kHeaderSize + GLOBAL_CONTEXT_SLOTS * kPointerSize;

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
  static bool IsBootstrappingOrContext(Object* object);
  static bool IsBootstrappingOrGlobalObject(Object* object);
#endif
};

} }  // namespace v8::internal

#endif  // V8_CONTEXTS_H_
