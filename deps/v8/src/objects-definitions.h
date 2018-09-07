// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEFINITIONS_H_
#define V8_OBJECTS_DEFINITIONS_H_

#include "src/heap-symbols.h"

namespace v8 {

namespace internal {

// All Maps have a field instance_type containing a InstanceType.
// It describes the type of the instances.
//
// As an example, a JavaScript object is a heap object and its map
// instance_type is JS_OBJECT_TYPE.
//
// The names of the string instance types are intended to systematically
// mirror their encoding in the instance_type field of the map.  The default
// encoding is considered TWO_BYTE.  It is not mentioned in the name.  ONE_BYTE
// encoding is mentioned explicitly in the name.  Likewise, the default
// representation is considered sequential.  It is not mentioned in the
// name.  The other representations (e.g. CONS, EXTERNAL) are explicitly
// mentioned.  Finally, the string is either a STRING_TYPE (if it is a normal
// string) or a INTERNALIZED_STRING_TYPE (if it is a internalized string).
//
// NOTE: The following things are some that depend on the string types having
// instance_types that are less than those of all other types:
// HeapObject::Size, HeapObject::IterateBody, the typeof operator, and
// Object::IsString.
//
// NOTE: Everything following JS_VALUE_TYPE is considered a
// JSObject for GC purposes. The first four entries here have typeof
// 'object', whereas JS_FUNCTION_TYPE has typeof 'function'.
//
// NOTE: List had to be split into two, because of conditional item(s) from
// INTL namespace. They can't just be appended to the end, because of the
// checks we do in tests (expecting JS_FUNCTION_TYPE to be last).
#define INSTANCE_TYPE_LIST_BEFORE_INTL(V)                       \
  V(INTERNALIZED_STRING_TYPE)                                   \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE)                          \
  V(ONE_BYTE_INTERNALIZED_STRING_TYPE)                          \
  V(EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)                 \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE)       \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE)                    \
  V(SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE)           \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE) \
  V(STRING_TYPE)                                                \
  V(CONS_STRING_TYPE)                                           \
  V(EXTERNAL_STRING_TYPE)                                       \
  V(SLICED_STRING_TYPE)                                         \
  V(THIN_STRING_TYPE)                                           \
  V(ONE_BYTE_STRING_TYPE)                                       \
  V(CONS_ONE_BYTE_STRING_TYPE)                                  \
  V(EXTERNAL_ONE_BYTE_STRING_TYPE)                              \
  V(SLICED_ONE_BYTE_STRING_TYPE)                                \
  V(THIN_ONE_BYTE_STRING_TYPE)                                  \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)                    \
  V(SHORT_EXTERNAL_STRING_TYPE)                                 \
  V(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE)                        \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE)              \
                                                                \
  V(SYMBOL_TYPE)                                                \
  V(HEAP_NUMBER_TYPE)                                           \
  V(BIGINT_TYPE)                                                \
  V(ODDBALL_TYPE)                                               \
                                                                \
  V(MAP_TYPE)                                                   \
  V(CODE_TYPE)                                                  \
  V(MUTABLE_HEAP_NUMBER_TYPE)                                   \
  V(FOREIGN_TYPE)                                               \
  V(BYTE_ARRAY_TYPE)                                            \
  V(BYTECODE_ARRAY_TYPE)                                        \
  V(FREE_SPACE_TYPE)                                            \
                                                                \
  V(FIXED_INT8_ARRAY_TYPE)                                      \
  V(FIXED_UINT8_ARRAY_TYPE)                                     \
  V(FIXED_INT16_ARRAY_TYPE)                                     \
  V(FIXED_UINT16_ARRAY_TYPE)                                    \
  V(FIXED_INT32_ARRAY_TYPE)                                     \
  V(FIXED_UINT32_ARRAY_TYPE)                                    \
  V(FIXED_FLOAT32_ARRAY_TYPE)                                   \
  V(FIXED_FLOAT64_ARRAY_TYPE)                                   \
  V(FIXED_UINT8_CLAMPED_ARRAY_TYPE)                             \
  V(FIXED_BIGINT64_ARRAY_TYPE)                                  \
  V(FIXED_BIGUINT64_ARRAY_TYPE)                                 \
                                                                \
  V(FIXED_DOUBLE_ARRAY_TYPE)                                    \
  V(FEEDBACK_METADATA_TYPE)                                     \
  V(FILLER_TYPE)                                                \
                                                                \
  V(ACCESS_CHECK_INFO_TYPE)                                     \
  V(ACCESSOR_INFO_TYPE)                                         \
  V(ACCESSOR_PAIR_TYPE)                                         \
  V(ALIASED_ARGUMENTS_ENTRY_TYPE)                               \
  V(ALLOCATION_MEMENTO_TYPE)                                    \
  V(ASYNC_GENERATOR_REQUEST_TYPE)                               \
  V(DEBUG_INFO_TYPE)                                            \
  V(FUNCTION_TEMPLATE_INFO_TYPE)                                \
  V(INTERCEPTOR_INFO_TYPE)                                      \
  V(INTERPRETER_DATA_TYPE)                                      \
  V(MODULE_INFO_ENTRY_TYPE)                                     \
  V(MODULE_TYPE)                                                \
  V(OBJECT_TEMPLATE_INFO_TYPE)                                  \
  V(PROMISE_CAPABILITY_TYPE)                                    \
  V(PROMISE_REACTION_TYPE)                                      \
  V(PROTOTYPE_INFO_TYPE)                                        \
  V(SCRIPT_TYPE)                                                \
  V(STACK_FRAME_INFO_TYPE)                                      \
  V(TUPLE2_TYPE)                                                \
  V(TUPLE3_TYPE)                                                \
  V(ARRAY_BOILERPLATE_DESCRIPTION_TYPE)                         \
  V(WASM_DEBUG_INFO_TYPE)                                       \
  V(WASM_EXPORTED_FUNCTION_DATA_TYPE)                           \
                                                                \
  V(CALLABLE_TASK_TYPE)                                         \
  V(CALLBACK_TASK_TYPE)                                         \
  V(PROMISE_FULFILL_REACTION_JOB_TASK_TYPE)                     \
  V(PROMISE_REJECT_REACTION_JOB_TASK_TYPE)                      \
  V(PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE)                     \
                                                                \
  V(ALLOCATION_SITE_TYPE)                                       \
                                                                \
  V(FIXED_ARRAY_TYPE)                                           \
  V(OBJECT_BOILERPLATE_DESCRIPTION_TYPE)                        \
  V(HASH_TABLE_TYPE)                                            \
  V(ORDERED_HASH_MAP_TYPE)                                      \
  V(ORDERED_HASH_SET_TYPE)                                      \
  V(NAME_DICTIONARY_TYPE)                                       \
  V(GLOBAL_DICTIONARY_TYPE)                                     \
  V(NUMBER_DICTIONARY_TYPE)                                     \
  V(SIMPLE_NUMBER_DICTIONARY_TYPE)                              \
  V(STRING_TABLE_TYPE)                                          \
  V(EPHEMERON_HASH_TABLE_TYPE)                                  \
  V(SCOPE_INFO_TYPE)                                            \
  V(SCRIPT_CONTEXT_TABLE_TYPE)                                  \
                                                                \
  V(BLOCK_CONTEXT_TYPE)                                         \
  V(CATCH_CONTEXT_TYPE)                                         \
  V(DEBUG_EVALUATE_CONTEXT_TYPE)                                \
  V(EVAL_CONTEXT_TYPE)                                          \
  V(FUNCTION_CONTEXT_TYPE)                                      \
  V(MODULE_CONTEXT_TYPE)                                        \
  V(NATIVE_CONTEXT_TYPE)                                        \
  V(SCRIPT_CONTEXT_TYPE)                                        \
  V(WITH_CONTEXT_TYPE)                                          \
                                                                \
  V(WEAK_FIXED_ARRAY_TYPE)                                      \
  V(DESCRIPTOR_ARRAY_TYPE)                                      \
  V(TRANSITION_ARRAY_TYPE)                                      \
                                                                \
  V(CALL_HANDLER_INFO_TYPE)                                     \
  V(CELL_TYPE)                                                  \
  V(CODE_DATA_CONTAINER_TYPE)                                   \
  V(FEEDBACK_CELL_TYPE)                                         \
  V(FEEDBACK_VECTOR_TYPE)                                       \
  V(LOAD_HANDLER_TYPE)                                          \
  V(PRE_PARSED_SCOPE_DATA_TYPE)                                 \
  V(PROPERTY_ARRAY_TYPE)                                        \
  V(PROPERTY_CELL_TYPE)                                         \
  V(SHARED_FUNCTION_INFO_TYPE)                                  \
  V(SMALL_ORDERED_HASH_MAP_TYPE)                                \
  V(SMALL_ORDERED_HASH_SET_TYPE)                                \
  V(STORE_HANDLER_TYPE)                                         \
  V(UNCOMPILED_DATA_WITHOUT_PRE_PARSED_SCOPE_TYPE)              \
  V(UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_TYPE)                 \
  V(WEAK_CELL_TYPE)                                             \
  V(WEAK_ARRAY_LIST_TYPE)                                       \
                                                                \
  V(JS_PROXY_TYPE)                                              \
  V(JS_GLOBAL_OBJECT_TYPE)                                      \
  V(JS_GLOBAL_PROXY_TYPE)                                       \
  V(JS_MODULE_NAMESPACE_TYPE)                                   \
  V(JS_SPECIAL_API_OBJECT_TYPE)                                 \
  V(JS_VALUE_TYPE)                                              \
  V(JS_API_OBJECT_TYPE)                                         \
  V(JS_OBJECT_TYPE)                                             \
                                                                \
  V(JS_ARGUMENTS_TYPE)                                          \
  V(JS_ARRAY_BUFFER_TYPE)                                       \
  V(JS_ARRAY_ITERATOR_TYPE)                                     \
  V(JS_ARRAY_TYPE)                                              \
  V(JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)                           \
  V(JS_ASYNC_GENERATOR_OBJECT_TYPE)                             \
  V(JS_CONTEXT_EXTENSION_OBJECT_TYPE)                           \
  V(JS_DATE_TYPE)                                               \
  V(JS_ERROR_TYPE)                                              \
  V(JS_GENERATOR_OBJECT_TYPE)                                   \
  V(JS_MAP_TYPE)                                                \
  V(JS_MAP_KEY_ITERATOR_TYPE)                                   \
  V(JS_MAP_KEY_VALUE_ITERATOR_TYPE)                             \
  V(JS_MAP_VALUE_ITERATOR_TYPE)                                 \
  V(JS_MESSAGE_OBJECT_TYPE)                                     \
  V(JS_PROMISE_TYPE)                                            \
  V(JS_REGEXP_TYPE)                                             \
  V(JS_REGEXP_STRING_ITERATOR_TYPE)                             \
  V(JS_SET_TYPE)                                                \
  V(JS_SET_KEY_VALUE_ITERATOR_TYPE)                             \
  V(JS_SET_VALUE_ITERATOR_TYPE)                                 \
  V(JS_STRING_ITERATOR_TYPE)                                    \
  V(JS_WEAK_MAP_TYPE)                                           \
  V(JS_WEAK_SET_TYPE)                                           \
  V(JS_TYPED_ARRAY_TYPE)                                        \
  V(JS_DATA_VIEW_TYPE)

#define INSTANCE_TYPE_LIST_AFTER_INTL(V) \
  V(WASM_GLOBAL_TYPE)                    \
  V(WASM_INSTANCE_TYPE)                  \
  V(WASM_MEMORY_TYPE)                    \
  V(WASM_MODULE_TYPE)                    \
  V(WASM_TABLE_TYPE)                     \
  V(JS_BOUND_FUNCTION_TYPE)              \
  V(JS_FUNCTION_TYPE)

#ifdef V8_INTL_SUPPORT
#define INSTANCE_TYPE_LIST(V)          \
  INSTANCE_TYPE_LIST_BEFORE_INTL(V)    \
  V(JS_INTL_LOCALE_TYPE)               \
  V(JS_INTL_RELATIVE_TIME_FORMAT_TYPE) \
  INSTANCE_TYPE_LIST_AFTER_INTL(V)
#else
#define INSTANCE_TYPE_LIST(V)       \
  INSTANCE_TYPE_LIST_BEFORE_INTL(V) \
  INSTANCE_TYPE_LIST_AFTER_INTL(V)
#endif  // V8_INTL_SUPPORT

// Since string types are not consecutive, this macro is used to
// iterate over them.
#define STRING_TYPE_LIST(V)                                                   \
  V(STRING_TYPE, kVariableSizeSentinel, string, String)                       \
  V(ONE_BYTE_STRING_TYPE, kVariableSizeSentinel, one_byte_string,             \
    OneByteString)                                                            \
  V(CONS_STRING_TYPE, ConsString::kSize, cons_string, ConsString)             \
  V(CONS_ONE_BYTE_STRING_TYPE, ConsString::kSize, cons_one_byte_string,       \
    ConsOneByteString)                                                        \
  V(SLICED_STRING_TYPE, SlicedString::kSize, sliced_string, SlicedString)     \
  V(SLICED_ONE_BYTE_STRING_TYPE, SlicedString::kSize, sliced_one_byte_string, \
    SlicedOneByteString)                                                      \
  V(EXTERNAL_STRING_TYPE, ExternalTwoByteString::kSize, external_string,      \
    ExternalString)                                                           \
  V(EXTERNAL_ONE_BYTE_STRING_TYPE, ExternalOneByteString::kSize,              \
    external_one_byte_string, ExternalOneByteString)                          \
  V(EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE, ExternalTwoByteString::kSize,    \
    external_string_with_one_byte_data, ExternalStringWithOneByteData)        \
  V(SHORT_EXTERNAL_STRING_TYPE, ExternalTwoByteString::kShortSize,            \
    short_external_string, ShortExternalString)                               \
  V(SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE, ExternalOneByteString::kShortSize,   \
    short_external_one_byte_string, ShortExternalOneByteString)               \
  V(SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE,                            \
    ExternalTwoByteString::kShortSize,                                        \
    short_external_string_with_one_byte_data,                                 \
    ShortExternalStringWithOneByteData)                                       \
                                                                              \
  V(INTERNALIZED_STRING_TYPE, kVariableSizeSentinel, internalized_string,     \
    InternalizedString)                                                       \
  V(ONE_BYTE_INTERNALIZED_STRING_TYPE, kVariableSizeSentinel,                 \
    one_byte_internalized_string, OneByteInternalizedString)                  \
  V(EXTERNAL_INTERNALIZED_STRING_TYPE, ExternalTwoByteString::kSize,          \
    external_internalized_string, ExternalInternalizedString)                 \
  V(EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE, ExternalOneByteString::kSize, \
    external_one_byte_internalized_string, ExternalOneByteInternalizedString) \
  V(EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,                     \
    ExternalTwoByteString::kSize,                                             \
    external_internalized_string_with_one_byte_data,                          \
    ExternalInternalizedStringWithOneByteData)                                \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE,                                  \
    ExternalTwoByteString::kShortSize, short_external_internalized_string,    \
    ShortExternalInternalizedString)                                          \
  V(SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE,                         \
    ExternalOneByteString::kShortSize,                                        \
    short_external_one_byte_internalized_string,                              \
    ShortExternalOneByteInternalizedString)                                   \
  V(SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE,               \
    ExternalTwoByteString::kShortSize,                                        \
    short_external_internalized_string_with_one_byte_data,                    \
    ShortExternalInternalizedStringWithOneByteData)                           \
  V(THIN_STRING_TYPE, ThinString::kSize, thin_string, ThinString)             \
  V(THIN_ONE_BYTE_STRING_TYPE, ThinString::kSize, thin_one_byte_string,       \
    ThinOneByteString)

// A struct is a simple object a set of object-valued fields.  Including an
// object type in this causes the compiler to generate most of the boilerplate
// code for the class including allocation and garbage collection routines,
// casts and predicates.  All you need to define is the class, methods and
// object verification routines.  Easy, no?
//
// Note that for subtle reasons related to the ordering or numerical values of
// type tags, elements in this list have to be added to the INSTANCE_TYPE_LIST
// manually.
#define STRUCT_LIST(V)                                                       \
  V(ACCESS_CHECK_INFO, AccessCheckInfo, access_check_info)                   \
  V(ACCESSOR_INFO, AccessorInfo, accessor_info)                              \
  V(ACCESSOR_PAIR, AccessorPair, accessor_pair)                              \
  V(ALIASED_ARGUMENTS_ENTRY, AliasedArgumentsEntry, aliased_arguments_entry) \
  V(ALLOCATION_MEMENTO, AllocationMemento, allocation_memento)               \
  V(ASYNC_GENERATOR_REQUEST, AsyncGeneratorRequest, async_generator_request) \
  V(DEBUG_INFO, DebugInfo, debug_info)                                       \
  V(FUNCTION_TEMPLATE_INFO, FunctionTemplateInfo, function_template_info)    \
  V(INTERCEPTOR_INFO, InterceptorInfo, interceptor_info)                     \
  V(INTERPRETER_DATA, InterpreterData, interpreter_data)                     \
  V(MODULE_INFO_ENTRY, ModuleInfoEntry, module_info_entry)                   \
  V(MODULE, Module, module)                                                  \
  V(OBJECT_TEMPLATE_INFO, ObjectTemplateInfo, object_template_info)          \
  V(PROMISE_CAPABILITY, PromiseCapability, promise_capability)               \
  V(PROMISE_REACTION, PromiseReaction, promise_reaction)                     \
  V(PROTOTYPE_INFO, PrototypeInfo, prototype_info)                           \
  V(SCRIPT, Script, script)                                                  \
  V(STACK_FRAME_INFO, StackFrameInfo, stack_frame_info)                      \
  V(TUPLE2, Tuple2, tuple2)                                                  \
  V(TUPLE3, Tuple3, tuple3)                                                  \
  V(ARRAY_BOILERPLATE_DESCRIPTION, ArrayBoilerplateDescription,              \
    array_boilerplate_description)                                           \
  V(WASM_DEBUG_INFO, WasmDebugInfo, wasm_debug_info)                         \
  V(WASM_EXPORTED_FUNCTION_DATA, WasmExportedFunctionData,                   \
    wasm_exported_function_data)                                             \
  V(CALLABLE_TASK, CallableTask, callable_task)                              \
  V(CALLBACK_TASK, CallbackTask, callback_task)                              \
  V(PROMISE_FULFILL_REACTION_JOB_TASK, PromiseFulfillReactionJobTask,        \
    promise_fulfill_reaction_job_task)                                       \
  V(PROMISE_REJECT_REACTION_JOB_TASK, PromiseRejectReactionJobTask,          \
    promise_reject_reaction_job_task)                                        \
  V(PROMISE_RESOLVE_THENABLE_JOB_TASK, PromiseResolveThenableJobTask,        \
    promise_resolve_thenable_job_task)

#define ALLOCATION_SITE_LIST(V)                                     \
  V(ALLOCATION_SITE, AllocationSite, WithWeakNext, allocation_site) \
  V(ALLOCATION_SITE, AllocationSite, WithoutWeakNext,               \
    allocation_site_without_weaknext)

#define DATA_HANDLER_LIST(V)                        \
  V(LOAD_HANDLER, LoadHandler, 1, load_handler1)    \
  V(LOAD_HANDLER, LoadHandler, 2, load_handler2)    \
  V(LOAD_HANDLER, LoadHandler, 3, load_handler3)    \
  V(STORE_HANDLER, StoreHandler, 0, store_handler0) \
  V(STORE_HANDLER, StoreHandler, 1, store_handler1) \
  V(STORE_HANDLER, StoreHandler, 2, store_handler2) \
  V(STORE_HANDLER, StoreHandler, 3, store_handler3)

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_DEFINITIONS_H_
