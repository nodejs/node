#ifndef SRC_JS_NATIVE_API_H_
#define SRC_JS_NATIVE_API_H_

// This file needs to be compatible with C compilers.
#include <stddef.h>   // NOLINT(modernize-deprecated-headers)
#include <stdbool.h>  // NOLINT(modernize-deprecated-headers)

// Use INT_MAX, this should only be consumed by the pre-processor anyway.
#define NAPI_VERSION_EXPERIMENTAL 2147483647
#define NODE_API_VERSION_EXPERIMENTAL NAPI_VERSION_EXPERIMENTAL
#if !(defined(NAPI_VERSION) || defined(NODE_API_VERSION))
#if defined(NAPI_EXPERIMENTAL) || defined(NODE_API_EXPERIMENTAL)
#define NAPI_VERSION NAPI_VERSION_EXPERIMENTAL
#define NODE_API_VERSION NODE_API_VERSION_EXPERIMENTAL
#else
// The baseline version for N-API.
// The NAPI_VERSION controls which version will be used by default when
// compilling a native addon. If the addon developer specifically wants to use
// functions available in a new version of N-API that is not yet ported in all
// LTS versions, they can set NAPI_VERSION knowing that they have specifically
// depended on that version.
#define NAPI_VERSION 7
#define NODE_API_VERSION NAPI_VERSION
#endif
#endif

#include "js_native_api_types.h"

// If you need __declspec(dllimport), either include <node_api.h> instead, or
// define NAPI_EXTERN as __declspec(dllimport) on the compiler's command line.
#ifndef NAPI_EXTERN
  #ifdef _WIN32
    #define NAPI_EXTERN __declspec(dllexport)
  #elif defined(__wasm32__)
    #define NAPI_EXTERN __attribute__((visibility("default")))                \
                        __attribute__((__import_module__("napi")))
  #else
    #define NAPI_EXTERN __attribute__((visibility("default")))
  #endif
#endif
#define NODE_API_EXTERN NAPI_EXTERN

#define NAPI_AUTO_LENGTH SIZE_MAX
#define NODE_API_AUTO_LENGTH NAPI_AUTO_LENGTH

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

EXTERN_C_START

NODE_API_EXTERN node_api_status
napi_get_last_error_info(node_api_env env,
                         const node_api_extended_error_info** result);
#define node_api_get_last_error_info napi_get_last_error_info

// Getters for defined singletons
NODE_API_EXTERN node_api_status
napi_get_undefined(node_api_env env, node_api_value* result);
#define node_api_get_undefined napi_get_undefined
NODE_API_EXTERN node_api_status
napi_get_null(node_api_env env, node_api_value* result);
#define node_api_get_null napi_get_null
NODE_API_EXTERN node_api_status
napi_get_global(node_api_env env, node_api_value* result);
#define node_api_get_global napi_get_global
NODE_API_EXTERN node_api_status
napi_get_boolean(node_api_env env, bool value, node_api_value* result);
#define node_api_get_boolean napi_get_boolean

// Methods to create Primitive types/Objects
NODE_API_EXTERN node_api_status
napi_create_object(node_api_env env, node_api_value* result);
#define node_api_create_object napi_create_object
NODE_API_EXTERN node_api_status
napi_create_array(node_api_env env, node_api_value* result);
#define node_api_create_array napi_create_array
NODE_API_EXTERN node_api_status
napi_create_array_with_length(node_api_env env,
                              size_t length,
                              node_api_value* result);
#define node_api_create_array_with_length \
  napi_create_array_with_length
NODE_API_EXTERN node_api_status
napi_create_double(node_api_env env,
                   double value,
                   node_api_value* result);
#define node_api_create_double napi_create_double
NODE_API_EXTERN node_api_status
napi_create_int32(node_api_env env,
                  int32_t value,
                  node_api_value* result);
#define node_api_create_int32 napi_create_int32
NODE_API_EXTERN node_api_status
napi_create_uint32(node_api_env env,
                   uint32_t value,
                   node_api_value* result);
#define node_api_create_uint32 napi_create_uint32
NODE_API_EXTERN node_api_status
napi_create_int64(node_api_env env,
                  int64_t value,
                  node_api_value* result);
#define node_api_create_int64 napi_create_int64
NODE_API_EXTERN node_api_status
napi_create_string_latin1(node_api_env env,
                          const char* str,
                          size_t length,
                          node_api_value* result);
#define node_api_create_string_latin1 napi_create_string_latin1
NODE_API_EXTERN node_api_status
napi_create_string_utf8(node_api_env env,
                        const char* str,
                        size_t length,
                        node_api_value* result);
#define node_api_create_string_utf8 napi_create_string_utf8
NODE_API_EXTERN node_api_status
napi_create_string_utf16(node_api_env env,
                         const char16_t* str,
                         size_t length,
                         node_api_value* result);
#define node_api_create_string_utf16 napi_create_string_utf16
NODE_API_EXTERN node_api_status
napi_create_symbol(node_api_env env,
                   node_api_value description,
                   node_api_value* result);
#define node_api_create_symbol napi_create_symbol
NODE_API_EXTERN node_api_status
napi_create_function(node_api_env env,
                     const char* utf8name,
                     size_t length,
                     node_api_callback cb,
                     void* data,
                     node_api_value* result);
#define node_api_create_function napi_create_function
NODE_API_EXTERN node_api_status
napi_create_error(node_api_env env,
                  node_api_value code,
                  node_api_value msg,
                  node_api_value* result);
#define node_api_create_error napi_create_error
NODE_API_EXTERN node_api_status
napi_create_type_error(node_api_env env,
                       node_api_value code,
                       node_api_value msg,
                       node_api_value* result);
#define node_api_create_type_error napi_create_type_error
NODE_API_EXTERN node_api_status
napi_create_range_error(node_api_env env,
                        node_api_value code,
                        node_api_value msg,
                        node_api_value* result);
#define node_api_create_range_error napi_create_range_error

// Methods to get the native node_api_value from Primitive type
NODE_API_EXTERN node_api_status
napi_typeof(node_api_env env,
            node_api_value value,
            node_api_valuetype* result);
#define node_api_typeof napi_typeof
NODE_API_EXTERN node_api_status
napi_get_value_double(node_api_env env,
                      node_api_value value,
                      double* result);
#define node_api_get_value_double napi_get_value_double
NODE_API_EXTERN node_api_status
napi_get_value_int32(node_api_env env,
                     node_api_value value,
                     int32_t* result);
#define node_api_get_value_int32 napi_get_value_int32
NODE_API_EXTERN node_api_status
napi_get_value_uint32(node_api_env env,
                      node_api_value value,
                      uint32_t* result);
#define node_api_get_value_uint32 napi_get_value_uint32
NODE_API_EXTERN node_api_status
napi_get_value_int64(node_api_env env, node_api_value value, int64_t* result);
#define node_api_get_value_int64 napi_get_value_int64
NODE_API_EXTERN node_api_status
napi_get_value_bool(node_api_env env, node_api_value value, bool* result);
#define node_api_get_value_bool napi_get_value_bool

// Copies LATIN-1 encoded bytes from a string into a buffer.
NODE_API_EXTERN node_api_status
napi_get_value_string_latin1(node_api_env env,
                             node_api_value value,
                             char* buf,
                             size_t bufsize,
                             size_t* result);
#define node_api_get_value_string_latin1 napi_get_value_string_latin1

// Copies UTF-8 encoded bytes from a string into a buffer.
NODE_API_EXTERN node_api_status
napi_get_value_string_utf8(node_api_env env,
                           node_api_value value,
                           char* buf,
                           size_t bufsize,
                           size_t* result);
#define node_api_get_value_string_utf8 napi_get_value_string_utf8

// Copies UTF-16 encoded bytes from a string into a buffer.
NODE_API_EXTERN node_api_status
napi_get_value_string_utf16(node_api_env env,
                            node_api_value value,
                            char16_t* buf,
                            size_t bufsize,
                            size_t* result);
#define node_api_get_value_string_utf16 napi_get_value_string_utf16

// Methods to coerce values
// These APIs may execute user scripts
NODE_API_EXTERN node_api_status
napi_coerce_to_bool(node_api_env env,
                    node_api_value value,
                    node_api_value* result);
#define node_api_coerce_to_bool napi_coerce_to_bool
NODE_API_EXTERN node_api_status
napi_coerce_to_number(node_api_env env,
                      node_api_value value,
                      node_api_value* result);
#define node_api_coerce_to_number napi_coerce_to_number
NODE_API_EXTERN
node_api_status napi_coerce_to_object(node_api_env env,
                                      node_api_value value,
                                      node_api_value* result);
#define node_api_coerce_to_object napi_coerce_to_object
NODE_API_EXTERN node_api_status
napi_coerce_to_string(node_api_env env,
                      node_api_value value,
                      node_api_value* result);
#define node_api_coerce_to_string napi_coerce_to_string

// Methods to work with Objects
NODE_API_EXTERN node_api_status
napi_get_prototype(node_api_env env,
                   node_api_value object,
                   node_api_value* result);
#define node_api_get_prototype napi_get_prototype
NODE_API_EXTERN node_api_status
napi_get_property_names(node_api_env env,
                        node_api_value object,
                        node_api_value* result);
#define node_api_get_property_names napi_get_property_names
NODE_API_EXTERN node_api_status
napi_set_property(node_api_env env,
                  node_api_value object,
                  node_api_value key,
                  node_api_value value);
#define node_api_set_property napi_set_property
NODE_API_EXTERN node_api_status
napi_has_property(node_api_env env,
                  node_api_value object,
                  node_api_value key,
                  bool* result);
#define node_api_has_property napi_has_property
NODE_API_EXTERN node_api_status
napi_get_property(node_api_env env,
                  node_api_value object,
                  node_api_value key,
                  node_api_value* result);
#define node_api_get_property napi_get_property
NODE_API_EXTERN node_api_status
napi_delete_property(node_api_env env,
                     node_api_value object,
                     node_api_value key,
                     bool* result);
#define node_api_delete_property napi_delete_property
NODE_API_EXTERN node_api_status
napi_has_own_property(node_api_env env,
                      node_api_value object,
                      node_api_value key,
                      bool* result);
#define node_api_has_own_property napi_has_own_property
NODE_API_EXTERN node_api_status
napi_set_named_property(node_api_env env,
                        node_api_value object,
                        const char* utf8name,
                        node_api_value value);
#define node_api_set_named_property napi_set_named_property
NODE_API_EXTERN node_api_status
napi_has_named_property(node_api_env env,
                        node_api_value object,
                        const char* utf8name,
                        bool* result);
#define node_api_has_named_property napi_has_named_property
NODE_API_EXTERN node_api_status
napi_get_named_property(node_api_env env,
                        node_api_value object,
                        const char* utf8name,
                        node_api_value* result);
#define node_api_get_named_property napi_get_named_property
NODE_API_EXTERN node_api_status
napi_set_element(node_api_env env,
                 node_api_value object,
                 uint32_t index,
                 node_api_value value);
#define node_api_set_element napi_set_element
NODE_API_EXTERN node_api_status
napi_has_element(node_api_env env,
                 node_api_value object,
                 uint32_t index,
                 bool* result);
#define node_api_has_element napi_has_element
NODE_API_EXTERN node_api_status
napi_get_element(node_api_env env,
                 node_api_value object,
                 uint32_t index,
                 node_api_value* result);
#define node_api_get_element napi_get_element
NODE_API_EXTERN node_api_status
napi_delete_element(node_api_env env,
                    node_api_value object,
                    uint32_t index,
                    bool* result);
#define node_api_delete_element napi_delete_element
NODE_API_EXTERN node_api_status
napi_define_properties(node_api_env env,
                       node_api_value object,
                       size_t property_count,
                       const node_api_property_descriptor* properties);
#define node_api_define_properties napi_define_properties

// Methods to work with Arrays
NODE_API_EXTERN node_api_status
napi_is_array(node_api_env env,
              node_api_value value,
              bool* result);
#define node_api_is_array napi_is_array
NODE_API_EXTERN node_api_status
napi_get_array_length(node_api_env env,
                      node_api_value value,
                      uint32_t* result);
#define node_api_get_array_length napi_get_array_length

// Methods to compare values
NODE_API_EXTERN node_api_status
napi_strict_equals(node_api_env env,
                   node_api_value lhs,
                   node_api_value rhs,
                   bool* result);
#define node_api_strict_equals napi_strict_equals

// Methods to work with Functions
NODE_API_EXTERN node_api_status
napi_call_function(node_api_env env,
                   node_api_value recv,
                   node_api_value func,
                   size_t argc,
                   const node_api_value* argv,
                   node_api_value* result);
#define node_api_call_function napi_call_function
NODE_API_EXTERN node_api_status
napi_new_instance(node_api_env env,
                  node_api_value constructor,
                  size_t argc,
                  const node_api_value* argv,
                  node_api_value* result);
#define node_api_new_instance napi_new_instance
NODE_API_EXTERN node_api_status
napi_instanceof(node_api_env env,
                node_api_value object,
                node_api_value constructor,
                bool* result);
#define node_api_instanceof napi_instanceof

// Methods to work with node_api_callbacks

// Gets all callback info in a single call. (Ugly, but faster.)
NODE_API_EXTERN node_api_status napi_get_cb_info(
    node_api_env env,               // [in] NAPI environment handle
    node_api_callback_info cbinfo,  // [in] Opaque callback-info handle
    size_t* argc,     // [in-out] Specifies the size of the provided argv array
                      // and receives the actual count of args.
    node_api_value* argv,  // [out] Array of values
    node_api_value* this_arg,  // [out] Receives the JS 'this' arg for the call
    void** data);          // [out] Receives the data pointer for the callback.
#define node_api_get_cb_info napi_get_cb_info

NODE_API_EXTERN node_api_status
napi_get_new_target(node_api_env env,
                    node_api_callback_info cbinfo,
                    node_api_value* result);
#define node_api_get_new_target napi_get_new_target
NODE_API_EXTERN node_api_status
napi_define_class(node_api_env env,
                  const char* utf8name,
                  size_t length,
                  node_api_callback constructor,
                  void* data,
                  size_t property_count,
                  const node_api_property_descriptor* properties,
                  node_api_value* result);
#define node_api_define_class napi_define_class

// Methods to work with external data objects
NODE_API_EXTERN node_api_status
napi_wrap(node_api_env env,
          node_api_value js_object,
          void* native_object,
          node_api_finalize finalize_cb,
          void* finalize_hint,
          node_api_ref* result);
#define node_api_wrap napi_wrap
NODE_API_EXTERN node_api_status
napi_unwrap(node_api_env env, node_api_value js_object, void** result);
#define node_api_unwrap napi_unwrap
NODE_API_EXTERN node_api_status
napi_remove_wrap(node_api_env env,
                 node_api_value js_object,
                 void** result);
#define node_api_remove_wrap napi_remove_wrap
NODE_API_EXTERN node_api_status
napi_create_external(node_api_env env,
                     void* data,
                     node_api_finalize finalize_cb,
                     void* finalize_hint,
                     node_api_value* result);
#define node_api_create_external napi_create_external
NODE_API_EXTERN node_api_status
napi_get_value_external(node_api_env env,
                        node_api_value value,
                        void** result);
#define node_api_get_value_external napi_get_value_external

// Methods to control object lifespan

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
NODE_API_EXTERN node_api_status
napi_create_reference(node_api_env env,
                      node_api_value value,
                      uint32_t initial_refcount,
                     node_api_ref* result);
#define node_api_create_reference napi_create_reference

// Deletes a reference. The referenced value is released, and may
// be GC'd unless there are other references to it.
NODE_API_EXTERN node_api_status
napi_delete_reference(node_api_env env, node_api_ref ref);
#define node_api_delete_reference napi_delete_reference

// Increments the reference count, optionally returning the resulting count.
// After this call the  reference will be a strong reference because its
// refcount is >0, and the referenced object is effectively "pinned".
// Calling this when the refcount is 0 and the object is unavailable
// results in an error.
NODE_API_EXTERN node_api_status
napi_reference_ref(node_api_env env,
                   node_api_ref ref,
                   uint32_t* result);
#define node_api_reference_ref napi_reference_ref

// Decrements the reference count, optionally returning the resulting count.
// If the result is 0 the reference is now weak and the object may be GC'd
// at any time if there are no other references. Calling this when the
// refcount is already 0 results in an error.
NODE_API_EXTERN node_api_status
napi_reference_unref(node_api_env env,
                     node_api_ref ref,
                     uint32_t* result);
#define node_api_reference_unref napi_reference_unref

// Attempts to get a referenced value. If the reference is weak,
// the value might no longer be available, in that case the call
// is still successful but the result is NULL.
NODE_API_EXTERN node_api_status
napi_get_reference_value(node_api_env env,
                         node_api_ref ref,
                         node_api_value* result);
#define node_api_get_reference_value napi_get_reference_value

NODE_API_EXTERN node_api_status
napi_open_handle_scope(node_api_env env,
                       node_api_handle_scope* result);
#define node_api_open_handle_scope napi_open_handle_scope
NODE_API_EXTERN node_api_status
napi_close_handle_scope(node_api_env env,
                        node_api_handle_scope scope);
#define node_api_close_handle_scope napi_close_handle_scope

NODE_API_EXTERN node_api_status
napi_open_escapable_handle_scope(node_api_env env,
                                 node_api_escapable_handle_scope* result);
#define node_api_open_escapable_handle_scope napi_open_escapable_handle_scope
NODE_API_EXTERN node_api_status
napi_close_escapable_handle_scope(node_api_env env,
                                  node_api_escapable_handle_scope scope);
#define node_api_close_escapable_handle_scope napi_close_escapable_handle_scope

NODE_API_EXTERN node_api_status
napi_escape_handle(node_api_env env,
                   node_api_escapable_handle_scope scope,
                   node_api_value escapee,
                   node_api_value* result);
#define node_api_escape_handle napi_escape_handle

// Methods to support error handling
NODE_API_EXTERN node_api_status
napi_throw(node_api_env env, node_api_value error);
#define node_api_throw napi_throw
NODE_API_EXTERN node_api_status
napi_throw_error(node_api_env env,
                 const char* code,
                 const char* msg);
#define node_api_throw_error napi_throw_error
NODE_API_EXTERN node_api_status
napi_throw_type_error(node_api_env env,
                      const char* code,
                      const char* msg);
#define node_api_throw_type_error napi_throw_type_error
NODE_API_EXTERN node_api_status
napi_throw_range_error(node_api_env env,
                       const char* code,
                       const char* msg);
#define node_api_throw_range_error napi_throw_range_error
NODE_API_EXTERN node_api_status napi_is_error(node_api_env env,
                                              node_api_value value,
                                              bool* result);
#define node_api_is_error napi_is_error

// Methods to support catching exceptions
NODE_API_EXTERN node_api_status
napi_is_exception_pending(node_api_env env, bool* result);
#define node_api_is_exception_pending napi_is_exception_pending
NODE_API_EXTERN node_api_status
napi_get_and_clear_last_exception(node_api_env env,
                                  node_api_value* result);
#define node_api_get_and_clear_last_exception napi_get_and_clear_last_exception

// Methods to work with array buffers and typed arrays
NODE_API_EXTERN node_api_status
napi_is_arraybuffer(node_api_env env,
                    node_api_value value,
                    bool* result);
#define node_api_is_arraybuffer napi_is_arraybuffer
NODE_API_EXTERN node_api_status
napi_create_arraybuffer(node_api_env env,
                        size_t byte_length,
                        void** data,
                        node_api_value* result);
#define node_api_create_arraybuffer napi_create_arraybuffer
NODE_API_EXTERN node_api_status
napi_create_external_arraybuffer(node_api_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 node_api_finalize finalize_cb,
                                 void* finalize_hint,
                                 node_api_value* result);
#define node_api_create_external_arraybuffer napi_create_external_arraybuffer

NODE_API_EXTERN node_api_status
napi_get_arraybuffer_info(node_api_env env,
                          node_api_value arraybuffer,
                          void** data,
                          size_t* byte_length);
#define node_api_get_arraybuffer_info napi_get_arraybuffer_info
NODE_API_EXTERN node_api_status
napi_is_typedarray(node_api_env env, node_api_value value, bool* result);
#define node_api_is_typedarray napi_is_typedarray
NODE_API_EXTERN node_api_status
napi_create_typedarray(node_api_env env,
                       node_api_typedarray_type type,
                       size_t length,
                       node_api_value arraybuffer,
                       size_t byte_offset,
                       node_api_value* result);
#define node_api_create_typedarray napi_create_typedarray
NODE_API_EXTERN node_api_status
napi_get_typedarray_info(node_api_env env,
                         node_api_value typedarray,
                         napi_typedarray_type* type,
                         size_t* length,
                         void** data,
                         node_api_value* arraybuffer,
                         size_t* byte_offset);
#define node_api_get_typedarray_info napi_get_typedarray_info

NODE_API_EXTERN node_api_status
napi_create_dataview(node_api_env env,
                     size_t length,
                     node_api_value arraybuffer,
                     size_t byte_offset,
                     node_api_value* result);
#define node_api_create_dataview napi_create_dataview
NODE_API_EXTERN node_api_status
napi_is_dataview(node_api_env env,
                 node_api_value value,
                 bool* result);
#define node_api_is_dataview napi_is_dataview
NODE_API_EXTERN node_api_status
napi_get_dataview_info(node_api_env env,
                       node_api_value dataview,
                       size_t* bytelength,
                       void** data,
                       node_api_value* arraybuffer,
                       size_t* byte_offset);
#define node_api_get_dataview_info napi_get_dataview_info

// version management
NODE_API_EXTERN node_api_status
napi_get_version(node_api_env env, uint32_t* result);
#define node_api_get_version napi_get_version

// Promises
NODE_API_EXTERN node_api_status
napi_create_promise(node_api_env env,
                    node_api_deferred* deferred,
                    node_api_value* promise);
#define node_api_create_promise napi_create_promise
NODE_API_EXTERN node_api_status
napi_resolve_deferred(node_api_env env,
                      node_api_deferred deferred,
                      node_api_value resolution);
#define node_api_resolve_deferred napi_resolve_deferred
NODE_API_EXTERN node_api_status
napi_reject_deferred(node_api_env env,
                     node_api_deferred deferred,
                     node_api_value rejection);
#define node_api_reject_deferred napi_reject_deferred
NODE_API_EXTERN node_api_status
napi_is_promise(node_api_env env,
                node_api_value value,
                bool* is_promise);
#define node_api_is_promise napi_is_promise

// Running a script
NODE_API_EXTERN node_api_status
napi_run_script(node_api_env env,
                node_api_value script,
                node_api_value* result);
#define node_api_run_script napi_run_script

// Memory management
NODE_API_EXTERN node_api_status
napi_adjust_external_memory(node_api_env env,
                            int64_t change_in_bytes,
                            int64_t* adjusted_value);
#define node_api_adjust_external_memory napi_adjust_external_memory

#if NAPI_VERSION >= 5

// Dates
NODE_API_EXTERN node_api_status
napi_create_date(node_api_env env, double time, node_api_value* result);
#define node_api_create_date napi_create_date

NODE_API_EXTERN node_api_status
napi_is_date(node_api_env env, node_api_value value, bool* is_date);
#define node_api_is_date napi_is_date

NODE_API_EXTERN node_api_status
napi_get_date_value(node_api_env env, node_api_value value, double* result);
#define node_api_get_date_value napi_get_date_value

// Add finalizer for pointer
NODE_API_EXTERN node_api_status
napi_add_finalizer(node_api_env env,
                   node_api_value js_object,
                   void* native_object,
                   napi_finalize finalize_cb,
                   void* finalize_hint,
                   napi_ref* result);
#define node_api_add_finalizer napi_add_finalizer

#endif  // NAPI_VERSION >= 5

#if NAPI_VERSION >= 6

// BigInt
NODE_API_EXTERN node_api_status
napi_create_bigint_int64(node_api_env env,
                         int64_t value,
                         node_api_value* result);
#define node_api_create_bigint_int64 napi_create_bigint_int64
NODE_API_EXTERN node_api_status
napi_create_bigint_uint64(node_api_env env,
                          uint64_t value,
                          node_api_value* result);
#define node_api_create_bigint_uint64 napi_create_bigint_uint64
NODE_API_EXTERN node_api_status
napi_create_bigint_words(node_api_env env,
                         int sign_bit,
                         size_t word_count,
                         const uint64_t* words,
                         node_api_value* result);
#define node_api_create_bigint_words napi_create_bigint_words
NODE_API_EXTERN node_api_status
napi_get_value_bigint_int64(node_api_env env,
                            node_api_value value,
                            int64_t* result,
                            bool* lossless);
#define node_api_get_value_bigint_int64 napi_get_value_bigint_int64
NODE_API_EXTERN node_api_status
napi_get_value_bigint_uint64(node_api_env env,
                             node_api_value value,
                             uint64_t* result,
                             bool* lossless);
#define node_api_get_value_bigint_uint64 napi_get_value_bigint_uint64
NODE_API_EXTERN node_api_status
napi_get_value_bigint_words(node_api_env env,
                            node_api_value value,
                            int* sign_bit,
                            size_t* word_count,
                            uint64_t* words);
#define node_api_get_value_bigint_words napi_get_value_bigint_words

// Object
NODE_API_EXTERN node_api_status
napi_get_all_property_names(node_api_env env,
                            node_api_value object,
                            node_api_key_collection_mode key_mode,
                            node_api_key_filter key_filter,
                            node_api_key_conversion key_conversion,
                            node_api_value* result);
#define node_api_get_all_property_names napi_get_all_property_names

// Instance data
NODE_API_EXTERN node_api_status
napi_set_instance_data(node_api_env env,
                       void* data,
                       node_api_finalize finalize_cb,
                       void* finalize_hint);
#define node_api_set_instance_data napi_set_instance_data

NODE_API_EXTERN node_api_status
napi_get_instance_data(node_api_env env, void** data);
#define node_api_get_instance_data napi_get_instance_data
#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7
// ArrayBuffer detaching
NODE_API_EXTERN node_api_status
napi_detach_arraybuffer(node_api_env env, node_api_value arraybuffer);
#define node_api_detach_arraybuffer napi_detach_arraybuffer

NODE_API_EXTERN node_api_status
napi_is_detached_arraybuffer(node_api_env env,
                             node_api_value value,
                             bool* result);
#define node_api_is_detached_arraybuffer napi_is_detached_arraybuffer
#endif  // NAPI_VERSION >= 7

#if defined(NAPI_EXPERIMENTAL) || defined(NODE_API_EXPERIMENTAL)
// Type tagging
NODE_API_EXTERN node_api_status
napi_type_tag_object(node_api_env env,
                     node_api_value value,
                     const node_api_type_tag* type_tag);
#define node_api_type_tag_object napi_type_tag_object

NODE_API_EXTERN node_api_status
napi_check_object_type_tag(node_api_env env,
                           node_api_value value,
                           const node_api_type_tag* type_tag,
                           bool* result);
#define node_api_check_object_type_tag napi_check_object_type_tag
NODE_API_EXTERN node_api_status
napi_object_freeze(node_api_env env, node_api_value object);
#define node_api_object_freeze napi_object_freeze
NODE_API_EXTERN node_api_status
napi_object_seal(node_api_env env, node_api_value object);
#define node_api_object_seal napi_object_seal
#endif  // NAPI_EXPERIMENTAL

EXTERN_C_END

#endif  // SRC_JS_NATIVE_API_H_
