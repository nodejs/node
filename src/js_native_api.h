#ifndef SRC_JS_NATIVE_API_H_
#define SRC_JS_NATIVE_API_H_

// This file needs to be compatible with C compilers.
#include <stdbool.h>  // NOLINT(modernize-deprecated-headers)
#include <stddef.h>   // NOLINT(modernize-deprecated-headers)

#include "js_native_api_types.h"

// If you need __declspec(dllimport), either include <node_api.h> instead, or
// define NAPI_EXTERN as __declspec(dllimport) on the compiler's command line.
#ifndef NAPI_EXTERN
#ifdef _WIN32
#define NAPI_EXTERN __declspec(dllexport)
#elif defined(__wasm__)
#define NAPI_EXTERN                                                            \
  __attribute__((visibility("default")))                                       \
  __attribute__((__import_module__("napi")))
#else
#define NAPI_EXTERN __attribute__((visibility("default")))
#endif
#endif

#define NAPI_AUTO_LENGTH SIZE_MAX

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

EXTERN_C_START

#ifndef NODE_API_MODULE_USE_VTABLE

NAPI_EXTERN napi_status NAPI_CDECL napi_get_last_error_info(
    node_api_basic_env env, const napi_extended_error_info** result);

// Getters for defined singletons
NAPI_EXTERN napi_status NAPI_CDECL napi_get_undefined(napi_env env,
                                                      napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_null(napi_env env,
                                                 napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_global(napi_env env,
                                                   napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_boolean(napi_env env,
                                                    bool value,
                                                    napi_value* result);

// Methods to create Primitive types/Objects
NAPI_EXTERN napi_status NAPI_CDECL napi_create_object(napi_env env,
                                                      napi_value* result);
#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_CREATE_OBJECT_WITH_PROPERTIES
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_object_with_properties(napi_env env,
                                   napi_value prototype_or_null,
                                   napi_value* property_names,
                                   napi_value* property_values,
                                   size_t property_count,
                                   napi_value* result);
#endif  // NAPI_EXPERIMENTAL

NAPI_EXTERN napi_status NAPI_CDECL napi_create_array(napi_env env,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_array_with_length(napi_env env, size_t length, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_double(napi_env env,
                                                      double value,
                                                      napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_int32(napi_env env,
                                                     int32_t value,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_uint32(napi_env env,
                                                      uint32_t value,
                                                      napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_int64(napi_env env,
                                                     int64_t value,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_latin1(
    napi_env env, const char* str, size_t length, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_utf8(napi_env env,
                                                           const char* str,
                                                           size_t length,
                                                           napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_string_utf16(napi_env env,
                                                            const char16_t* str,
                                                            size_t length,
                                                            napi_value* result);
#if NAPI_VERSION >= 10
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_external_string_latin1(
    napi_env env,
    char* str,
    size_t length,
    node_api_basic_finalize finalize_callback,
    void* finalize_hint,
    napi_value* result,
    bool* copied);
NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_external_string_utf16(napi_env env,
                                      char16_t* str,
                                      size_t length,
                                      node_api_basic_finalize finalize_callback,
                                      void* finalize_hint,
                                      napi_value* result,
                                      bool* copied);

NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_latin1(
    napi_env env, const char* str, size_t length, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_utf8(
    napi_env env, const char* str, size_t length, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_property_key_utf16(
    napi_env env, const char16_t* str, size_t length, napi_value* result);
#endif  // NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL napi_create_symbol(napi_env env,
                                                      napi_value description,
                                                      napi_value* result);
#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL
node_api_symbol_for(napi_env env,
                    const char* utf8description,
                    size_t length,
                    napi_value* result);
#endif  // NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL napi_create_function(napi_env env,
                                                        const char* utf8name,
                                                        size_t length,
                                                        napi_callback cb,
                                                        void* data,
                                                        napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_error(napi_env env,
                                                     napi_value code,
                                                     napi_value msg,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_type_error(napi_env env,
                                                          napi_value code,
                                                          napi_value msg,
                                                          napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_range_error(napi_env env,
                                                           napi_value code,
                                                           napi_value msg,
                                                           napi_value* result);
#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_syntax_error(
    napi_env env, napi_value code, napi_value msg, napi_value* result);
#endif  // NAPI_VERSION >= 9

// Methods to get the native napi_value from Primitive type
NAPI_EXTERN napi_status NAPI_CDECL napi_typeof(napi_env env,
                                               napi_value value,
                                               napi_valuetype* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_double(napi_env env,
                                                         napi_value value,
                                                         double* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_int32(napi_env env,
                                                        napi_value value,
                                                        int32_t* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_uint32(napi_env env,
                                                         napi_value value,
                                                         uint32_t* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_int64(napi_env env,
                                                        napi_value value,
                                                        int64_t* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bool(napi_env env,
                                                       napi_value value,
                                                       bool* result);

// Copies LATIN-1 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_latin1(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result);

// Copies UTF-8 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_utf8(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result);

// Copies UTF-16 encoded bytes from a string into a buffer.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_string_utf16(napi_env env,
                                                               napi_value value,
                                                               char16_t* buf,
                                                               size_t bufsize,
                                                               size_t* result);

// Methods to coerce values
// These APIs may execute user scripts
NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_bool(napi_env env,
                                                       napi_value value,
                                                       napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_number(napi_env env,
                                                         napi_value value,
                                                         napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_object(napi_env env,
                                                         napi_value value,
                                                         napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_coerce_to_string(napi_env env,
                                                         napi_value value,
                                                         napi_value* result);

// Methods to work with Objects
NAPI_EXTERN napi_status NAPI_CDECL napi_get_prototype(napi_env env,
                                                      napi_value object,
                                                      napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_property_names(napi_env env,
                                                           napi_value object,
                                                           napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_set_property(napi_env env,
                                                     napi_value object,
                                                     napi_value key,
                                                     napi_value value);
NAPI_EXTERN napi_status NAPI_CDECL napi_has_property(napi_env env,
                                                     napi_value object,
                                                     napi_value key,
                                                     bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_property(napi_env env,
                                                     napi_value object,
                                                     napi_value key,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_delete_property(napi_env env,
                                                        napi_value object,
                                                        napi_value key,
                                                        bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_has_own_property(napi_env env,
                                                         napi_value object,
                                                         napi_value key,
                                                         bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_set_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           napi_value value);
NAPI_EXTERN napi_status NAPI_CDECL napi_has_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_named_property(napi_env env,
                                                           napi_value object,
                                                           const char* utf8name,
                                                           napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_set_element(napi_env env,
                                                    napi_value object,
                                                    uint32_t index,
                                                    napi_value value);
NAPI_EXTERN napi_status NAPI_CDECL napi_has_element(napi_env env,
                                                    napi_value object,
                                                    uint32_t index,
                                                    bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_element(napi_env env,
                                                    napi_value object,
                                                    uint32_t index,
                                                    napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_delete_element(napi_env env,
                                                       napi_value object,
                                                       uint32_t index,
                                                       bool* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_define_properties(napi_env env,
                       napi_value object,
                       size_t property_count,
                       const napi_property_descriptor* properties);

// Methods to work with Arrays
NAPI_EXTERN napi_status NAPI_CDECL napi_is_array(napi_env env,
                                                 napi_value value,
                                                 bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_array_length(napi_env env,
                                                         napi_value value,
                                                         uint32_t* result);

// Methods to compare values
NAPI_EXTERN napi_status NAPI_CDECL napi_strict_equals(napi_env env,
                                                      napi_value lhs,
                                                      napi_value rhs,
                                                      bool* result);

// Methods to work with Functions
NAPI_EXTERN napi_status NAPI_CDECL napi_call_function(napi_env env,
                                                      napi_value recv,
                                                      napi_value func,
                                                      size_t argc,
                                                      const napi_value* argv,
                                                      napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_new_instance(napi_env env,
                                                     napi_value constructor,
                                                     size_t argc,
                                                     const napi_value* argv,
                                                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_instanceof(napi_env env,
                                                   napi_value object,
                                                   napi_value constructor,
                                                   bool* result);

// Methods to work with napi_callbacks

// Gets all callback info in a single call. (Ugly, but faster.)
NAPI_EXTERN napi_status NAPI_CDECL napi_get_cb_info(
    napi_env env,               // [in] Node-API environment handle
    napi_callback_info cbinfo,  // [in] Opaque callback-info handle
    size_t* argc,      // [in-out] Specifies the size of the provided argv array
                       // and receives the actual count of args.
    napi_value* argv,  // [out] Array of values
    napi_value* this_arg,  // [out] Receives the JS 'this' arg for the call
    void** data);          // [out] Receives the data pointer for the callback.

NAPI_EXTERN napi_status NAPI_CDECL napi_get_new_target(
    napi_env env, napi_callback_info cbinfo, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_define_class(napi_env env,
                  const char* utf8name,
                  size_t length,
                  napi_callback constructor,
                  void* data,
                  size_t property_count,
                  const napi_property_descriptor* properties,
                  napi_value* result);

// Methods to work with external data objects
NAPI_EXTERN napi_status NAPI_CDECL
napi_wrap(napi_env env,
          napi_value js_object,
          void* native_object,
          node_api_basic_finalize finalize_cb,
          void* finalize_hint,
          napi_ref* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_unwrap(napi_env env,
                                               napi_value js_object,
                                               void** result);
NAPI_EXTERN napi_status NAPI_CDECL napi_remove_wrap(napi_env env,
                                                    napi_value js_object,
                                                    void** result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_external(napi_env env,
                     void* data,
                     node_api_basic_finalize finalize_cb,
                     void* finalize_hint,
                     napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_external(napi_env env,
                                                           napi_value value,
                                                           void** result);

// Methods to control object lifespan

// Set initial_refcount to 0 for a weak reference, >0 for a strong reference.
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_reference(napi_env env,
                      napi_value value,
                      uint32_t initial_refcount,
                      napi_ref* result);

// Deletes a reference. The referenced value is released, and may
// be GC'd unless there are other references to it.
NAPI_EXTERN napi_status NAPI_CDECL napi_delete_reference(node_api_basic_env env,
                                                         napi_ref ref);

// Increments the reference count, optionally returning the resulting count.
// After this call the  reference will be a strong reference because its
// refcount is >0, and the referenced object is effectively "pinned".
// Calling this when the refcount is 0 and the object is unavailable
// results in an error.
NAPI_EXTERN napi_status NAPI_CDECL napi_reference_ref(napi_env env,
                                                      napi_ref ref,
                                                      uint32_t* result);

// Decrements the reference count, optionally returning the resulting count.
// If the result is 0 the reference is now weak and the object may be GC'd
// at any time if there are no other references. Calling this when the
// refcount is already 0 results in an error.
NAPI_EXTERN napi_status NAPI_CDECL napi_reference_unref(napi_env env,
                                                        napi_ref ref,
                                                        uint32_t* result);

// Attempts to get a referenced value. If the reference is weak,
// the value might no longer be available, in that case the call
// is still successful but the result is NULL.
NAPI_EXTERN napi_status NAPI_CDECL napi_get_reference_value(napi_env env,
                                                            napi_ref ref,
                                                            napi_value* result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_open_handle_scope(napi_env env, napi_handle_scope* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_close_handle_scope(napi_env env, napi_handle_scope scope);
NAPI_EXTERN napi_status NAPI_CDECL napi_open_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_close_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope scope);

NAPI_EXTERN napi_status NAPI_CDECL
napi_escape_handle(napi_env env,
                   napi_escapable_handle_scope scope,
                   napi_value escapee,
                   napi_value* result);

// Methods to support error handling
NAPI_EXTERN napi_status NAPI_CDECL napi_throw(napi_env env, napi_value error);
NAPI_EXTERN napi_status NAPI_CDECL napi_throw_error(napi_env env,
                                                    const char* code,
                                                    const char* msg);
NAPI_EXTERN napi_status NAPI_CDECL napi_throw_type_error(napi_env env,
                                                         const char* code,
                                                         const char* msg);
NAPI_EXTERN napi_status NAPI_CDECL napi_throw_range_error(napi_env env,
                                                          const char* code,
                                                          const char* msg);
#if NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL node_api_throw_syntax_error(napi_env env,
                                                               const char* code,
                                                               const char* msg);
#endif  // NAPI_VERSION >= 9
NAPI_EXTERN napi_status NAPI_CDECL napi_is_error(napi_env env,
                                                 napi_value value,
                                                 bool* result);

// Methods to support catching exceptions
NAPI_EXTERN napi_status NAPI_CDECL napi_is_exception_pending(napi_env env,
                                                             bool* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_and_clear_last_exception(napi_env env, napi_value* result);

// Methods to work with array buffers and typed arrays
NAPI_EXTERN napi_status NAPI_CDECL napi_is_arraybuffer(napi_env env,
                                                       napi_value value,
                                                       bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_create_arraybuffer(napi_env env,
                                                           size_t byte_length,
                                                           void** data,
                                                           napi_value* result);
#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_external_arraybuffer(napi_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 node_api_basic_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result);
#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
NAPI_EXTERN napi_status NAPI_CDECL napi_get_arraybuffer_info(
    napi_env env, napi_value arraybuffer, void** data, size_t* byte_length);
NAPI_EXTERN napi_status NAPI_CDECL napi_is_typedarray(napi_env env,
                                                      napi_value value,
                                                      bool* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_typedarray(napi_env env,
                       napi_typedarray_type type,
                       size_t length,
                       napi_value arraybuffer,
                       size_t byte_offset,
                       napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_typedarray_info(napi_env env,
                         napi_value typedarray,
                         napi_typedarray_type* type,
                         size_t* length,
                         void** data,
                         napi_value* arraybuffer,
                         size_t* byte_offset);

NAPI_EXTERN napi_status NAPI_CDECL napi_create_dataview(napi_env env,
                                                        size_t length,
                                                        napi_value arraybuffer,
                                                        size_t byte_offset,
                                                        napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_is_dataview(napi_env env,
                                                    napi_value value,
                                                    bool* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_dataview_info(napi_env env,
                       napi_value dataview,
                       size_t* bytelength,
                       void** data,
                       napi_value* arraybuffer,
                       size_t* byte_offset);

#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_SHAREDARRAYBUFFER
NAPI_EXTERN napi_status NAPI_CDECL
node_api_is_sharedarraybuffer(napi_env env, napi_value value, bool* result);
NAPI_EXTERN napi_status NAPI_CDECL node_api_create_sharedarraybuffer(
    napi_env env, size_t byte_length, void** data, napi_value* result);
#endif  // NAPI_EXPERIMENTAL

// version management
NAPI_EXTERN napi_status NAPI_CDECL napi_get_version(node_api_basic_env env,
                                                    uint32_t* result);

// Promises
NAPI_EXTERN napi_status NAPI_CDECL napi_create_promise(napi_env env,
                                                       napi_deferred* deferred,
                                                       napi_value* promise);
NAPI_EXTERN napi_status NAPI_CDECL napi_resolve_deferred(napi_env env,
                                                         napi_deferred deferred,
                                                         napi_value resolution);
NAPI_EXTERN napi_status NAPI_CDECL napi_reject_deferred(napi_env env,
                                                        napi_deferred deferred,
                                                        napi_value rejection);
NAPI_EXTERN napi_status NAPI_CDECL napi_is_promise(napi_env env,
                                                   napi_value value,
                                                   bool* is_promise);

// Running a script
NAPI_EXTERN napi_status NAPI_CDECL napi_run_script(napi_env env,
                                                   napi_value script,
                                                   napi_value* result);

// Memory management
NAPI_EXTERN napi_status NAPI_CDECL napi_adjust_external_memory(
    node_api_basic_env env, int64_t change_in_bytes, int64_t* adjusted_value);

#if NAPI_VERSION >= 5

// Dates
NAPI_EXTERN napi_status NAPI_CDECL napi_create_date(napi_env env,
                                                    double time,
                                                    napi_value* result);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_date(napi_env env,
                                                napi_value value,
                                                bool* is_date);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_date_value(napi_env env,
                                                       napi_value value,
                                                       double* result);

// Add finalizer for pointer
NAPI_EXTERN napi_status NAPI_CDECL
napi_add_finalizer(napi_env env,
                   napi_value js_object,
                   void* finalize_data,
                   node_api_basic_finalize finalize_cb,
                   void* finalize_hint,
                   napi_ref* result);

#endif  // NAPI_VERSION >= 5

#ifdef NAPI_EXPERIMENTAL
#define NODE_API_EXPERIMENTAL_HAS_POST_FINALIZER

NAPI_EXTERN napi_status NAPI_CDECL
node_api_post_finalizer(node_api_basic_env env,
                        napi_finalize finalize_cb,
                        void* finalize_data,
                        void* finalize_hint);

#endif  // NAPI_EXPERIMENTAL

#if NAPI_VERSION >= 6

// BigInt
NAPI_EXTERN napi_status NAPI_CDECL napi_create_bigint_int64(napi_env env,
                                                            int64_t value,
                                                            napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_bigint_uint64(napi_env env, uint64_t value, napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_bigint_words(napi_env env,
                         int sign_bit,
                         size_t word_count,
                         const uint64_t* words,
                         napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bigint_int64(napi_env env,
                                                               napi_value value,
                                                               int64_t* result,
                                                               bool* lossless);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_value_bigint_uint64(
    napi_env env, napi_value value, uint64_t* result, bool* lossless);
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_value_bigint_words(napi_env env,
                            napi_value value,
                            int* sign_bit,
                            size_t* word_count,
                            uint64_t* words);

// Object
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_all_property_names(napi_env env,
                            napi_value object,
                            napi_key_collection_mode key_mode,
                            napi_key_filter key_filter,
                            napi_key_conversion key_conversion,
                            napi_value* result);

// Instance data
NAPI_EXTERN napi_status NAPI_CDECL
napi_set_instance_data(node_api_basic_env env,
                       void* data,
                       napi_finalize finalize_cb,
                       void* finalize_hint);

NAPI_EXTERN napi_status NAPI_CDECL
napi_get_instance_data(node_api_basic_env env, void** data);
#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7
// ArrayBuffer detaching
NAPI_EXTERN napi_status NAPI_CDECL
napi_detach_arraybuffer(napi_env env, napi_value arraybuffer);

NAPI_EXTERN napi_status NAPI_CDECL
napi_is_detached_arraybuffer(napi_env env, napi_value value, bool* result);
#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8
// Type tagging
NAPI_EXTERN napi_status NAPI_CDECL napi_type_tag_object(
    napi_env env, napi_value value, const napi_type_tag* type_tag);

NAPI_EXTERN napi_status NAPI_CDECL
napi_check_object_type_tag(napi_env env,
                           napi_value value,
                           const napi_type_tag* type_tag,
                           bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_object_freeze(napi_env env,
                                                      napi_value object);
NAPI_EXTERN napi_status NAPI_CDECL napi_object_seal(napi_env env,
                                                    napi_value object);
#endif  // NAPI_VERSION >= 8

#else  // NODE_API_MODULE_USE_VTABLE

extern const node_api_js_native_vtable* g_node_api_js_native_vtable;

static inline napi_status NAPI_CDECL napi_get_last_error_info(
    node_api_basic_env env, const napi_extended_error_info** result) {
  return g_node_api_js_native_vtable->get_last_error_info(env, result);
}

static inline napi_status NAPI_CDECL napi_get_undefined(napi_env env,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->get_undefined(env, result);
}

static inline napi_status NAPI_CDECL napi_get_null(napi_env env,
                                                   napi_value* result) {
  return g_node_api_js_native_vtable->get_null(env, result);
}

static inline napi_status NAPI_CDECL napi_get_global(napi_env env,
                                                     napi_value* result) {
  return g_node_api_js_native_vtable->get_global(env, result);
}

static inline napi_status NAPI_CDECL napi_get_boolean(napi_env env,
                                                      bool value,
                                                      napi_value* result) {
  return g_node_api_js_native_vtable->get_boolean(env, value, result);
}

// Methods to create Primitive types/Objects
static inline napi_status NAPI_CDECL napi_create_object(napi_env env,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->create_object(env, result);
}

#ifdef NAPI_EXPERIMENTAL
static inline napi_status NAPI_CDECL
napi_create_object_with_properties(napi_env env,
                                   napi_value prototype_or_null,
                                   napi_value* property_names,
                                   napi_value* property_values,
                                   size_t property_count,
                                   napi_value* result) {
  return g_node_api_js_native_vtable->create_object_with_properties(
      env,
      prototype_or_null,
      property_names,
      property_values,
      property_count,
      result);
}
#endif  // NAPI_EXPERIMENTAL

static inline napi_status NAPI_CDECL napi_create_array(napi_env env,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->create_array(env, result);
}

static inline napi_status NAPI_CDECL
napi_create_array_with_length(napi_env env, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_array_with_length(
      env, length, result);
}

static inline napi_status NAPI_CDECL napi_create_double(napi_env env,
                                                        double value,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->create_double(env, value, result);
}

static inline napi_status NAPI_CDECL napi_create_int32(napi_env env,
                                                       int32_t value,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->create_int32(env, value, result);
}

static inline napi_status NAPI_CDECL napi_create_uint32(napi_env env,
                                                        uint32_t value,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->create_uint32(env, value, result);
}

static inline napi_status NAPI_CDECL napi_create_int64(napi_env env,
                                                       int64_t value,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->create_int64(env, value, result);
}

static inline napi_status NAPI_CDECL napi_create_string_latin1(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_string_latin1(
      env, str, length, result);
}

static inline napi_status NAPI_CDECL napi_create_string_utf8(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_string_utf8(
      env, str, length, result);
}

static inline napi_status NAPI_CDECL napi_create_string_utf16(
    napi_env env, const char16_t* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_string_utf16(
      env, str, length, result);
}

#if NAPI_VERSION >= 10
static inline napi_status NAPI_CDECL node_api_create_external_string_latin1(
    napi_env env,
    char* str,
    size_t length,
    node_api_basic_finalize finalize_callback,
    void* finalize_hint,
    napi_value* result,
    bool* copied) {
  return g_node_api_js_native_vtable->create_external_string_latin1(
      env, str, length, finalize_callback, finalize_hint, result, copied);
}

static inline napi_status NAPI_CDECL
node_api_create_external_string_utf16(napi_env env,
                                      char16_t* str,
                                      size_t length,
                                      node_api_basic_finalize finalize_callback,
                                      void* finalize_hint,
                                      napi_value* result,
                                      bool* copied) {
  return g_node_api_js_native_vtable->create_external_string_utf16(
      env, str, length, finalize_callback, finalize_hint, result, copied);
}

static inline napi_status NAPI_CDECL node_api_create_property_key_latin1(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_property_key_latin1(
      env, str, length, result);
}

static inline napi_status NAPI_CDECL node_api_create_property_key_utf8(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_property_key_utf8(
      env, str, length, result);
}

static inline napi_status NAPI_CDECL node_api_create_property_key_utf16(
    napi_env env, const char16_t* str, size_t length, napi_value* result) {
  return g_node_api_js_native_vtable->create_property_key_utf16(
      env, str, length, result);
}

#endif  // NAPI_VERSION >= 10

static inline napi_status NAPI_CDECL napi_create_symbol(napi_env env,
                                                        napi_value description,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->create_symbol(env, description, result);
}

#if NAPI_VERSION >= 9
static inline napi_status NAPI_CDECL
node_api_symbol_for(napi_env env,
                    const char* utf8description,
                    size_t length,
                    napi_value* result) {
  return g_node_api_js_native_vtable->symbol_for(
      env, utf8description, length, result);
}
#endif  // NAPI_VERSION >= 9
static inline napi_status NAPI_CDECL napi_create_function(napi_env env,
                                                          const char* utf8name,
                                                          size_t length,
                                                          napi_callback cb,
                                                          void* data,
                                                          napi_value* result) {
  return g_node_api_js_native_vtable->create_function(
      env, utf8name, length, cb, data, result);
}

static inline napi_status NAPI_CDECL napi_create_error(napi_env env,
                                                       napi_value code,
                                                       napi_value msg,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->create_error(env, code, msg, result);
}

static inline napi_status NAPI_CDECL napi_create_type_error(
    napi_env env, napi_value code, napi_value msg, napi_value* result) {
  return g_node_api_js_native_vtable->create_type_error(env, code, msg, result);
}

static inline napi_status NAPI_CDECL napi_create_range_error(
    napi_env env, napi_value code, napi_value msg, napi_value* result) {
  return g_node_api_js_native_vtable->create_range_error(
      env, code, msg, result);
}

#if NAPI_VERSION >= 9
static inline napi_status NAPI_CDECL node_api_create_syntax_error(
    napi_env env, napi_value code, napi_value msg, napi_value* result) {
  return g_node_api_js_native_vtable->create_syntax_error(
      env, code, msg, result);
}
#endif  // NAPI_VERSION >= 9

// Methods to get the native napi_value from Primitive type
static inline napi_status NAPI_CDECL napi_typeof(napi_env env,
                                                 napi_value value,
                                                 napi_valuetype* result) {
  return g_node_api_js_native_vtable->type_of(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_double(napi_env env,
                                                           napi_value value,
                                                           double* result) {
  return g_node_api_js_native_vtable->get_value_double(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_int32(napi_env env,
                                                          napi_value value,
                                                          int32_t* result) {
  return g_node_api_js_native_vtable->get_value_int32(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_uint32(napi_env env,
                                                           napi_value value,
                                                           uint32_t* result) {
  return g_node_api_js_native_vtable->get_value_uint32(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_int64(napi_env env,
                                                          napi_value value,
                                                          int64_t* result) {
  return g_node_api_js_native_vtable->get_value_int64(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_bool(napi_env env,
                                                         napi_value value,
                                                         bool* result) {
  return g_node_api_js_native_vtable->get_value_bool(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_value_string_latin1(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result) {
  return g_node_api_js_native_vtable->get_value_string_latin1(
      env, value, buf, bufsize, result);
}

static inline napi_status NAPI_CDECL napi_get_value_string_utf8(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result) {
  return g_node_api_js_native_vtable->get_value_string_utf8(
      env, value, buf, bufsize, result);
}

static inline napi_status NAPI_CDECL
napi_get_value_string_utf16(napi_env env,
                            napi_value value,
                            char16_t* buf,
                            size_t bufsize,
                            size_t* result) {
  return g_node_api_js_native_vtable->get_value_string_utf16(
      env, value, buf, bufsize, result);
}

static inline napi_status NAPI_CDECL napi_coerce_to_bool(napi_env env,
                                                         napi_value value,
                                                         napi_value* result) {
  return g_node_api_js_native_vtable->coerce_to_bool(env, value, result);
}

static inline napi_status NAPI_CDECL napi_coerce_to_number(napi_env env,
                                                           napi_value value,
                                                           napi_value* result) {
  return g_node_api_js_native_vtable->coerce_to_number(env, value, result);
}

static inline napi_status NAPI_CDECL napi_coerce_to_object(napi_env env,
                                                           napi_value value,
                                                           napi_value* result) {
  return g_node_api_js_native_vtable->coerce_to_object(env, value, result);
}

static inline napi_status NAPI_CDECL napi_coerce_to_string(napi_env env,
                                                           napi_value value,
                                                           napi_value* result) {
  return g_node_api_js_native_vtable->coerce_to_string(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_prototype(napi_env env,
                                                        napi_value object,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->get_prototype(env, object, result);
}

static inline napi_status NAPI_CDECL
napi_get_property_names(napi_env env, napi_value object, napi_value* result) {
  return g_node_api_js_native_vtable->get_property_names(env, object, result);
}

static inline napi_status NAPI_CDECL napi_set_property(napi_env env,
                                                       napi_value object,
                                                       napi_value key,
                                                       napi_value value) {
  return g_node_api_js_native_vtable->set_property(env, object, key, value);
}

static inline napi_status NAPI_CDECL napi_has_property(napi_env env,
                                                       napi_value object,
                                                       napi_value key,
                                                       bool* result) {
  return g_node_api_js_native_vtable->has_property(env, object, key, result);
}

static inline napi_status NAPI_CDECL napi_get_property(napi_env env,
                                                       napi_value object,
                                                       napi_value key,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->get_property(env, object, key, result);
}

static inline napi_status NAPI_CDECL napi_delete_property(napi_env env,
                                                          napi_value object,
                                                          napi_value key,
                                                          bool* result) {
  return g_node_api_js_native_vtable->delete_property(env, object, key, result);
}

static inline napi_status NAPI_CDECL napi_has_own_property(napi_env env,
                                                           napi_value object,
                                                           napi_value key,
                                                           bool* result) {
  return g_node_api_js_native_vtable->has_own_property(
      env, object, key, result);
}

static inline napi_status NAPI_CDECL napi_set_named_property(
    napi_env env, napi_value object, const char* utf8name, napi_value value) {
  return g_node_api_js_native_vtable->set_named_property(
      env, object, utf8name, value);
}

static inline napi_status NAPI_CDECL napi_has_named_property(
    napi_env env, napi_value object, const char* utf8name, bool* result) {
  return g_node_api_js_native_vtable->has_named_property(
      env, object, utf8name, result);
}

static inline napi_status NAPI_CDECL napi_get_named_property(
    napi_env env, napi_value object, const char* utf8name, napi_value* result) {
  return g_node_api_js_native_vtable->get_named_property(
      env, object, utf8name, result);
}

static inline napi_status NAPI_CDECL napi_set_element(napi_env env,
                                                      napi_value object,
                                                      uint32_t index,
                                                      napi_value value) {
  return g_node_api_js_native_vtable->set_element(env, object, index, value);
}

static inline napi_status NAPI_CDECL napi_has_element(napi_env env,
                                                      napi_value object,
                                                      uint32_t index,
                                                      bool* result) {
  return g_node_api_js_native_vtable->has_element(env, object, index, result);
}

static inline napi_status NAPI_CDECL napi_get_element(napi_env env,
                                                      napi_value object,
                                                      uint32_t index,
                                                      napi_value* result) {
  return g_node_api_js_native_vtable->get_element(env, object, index, result);
}

static inline napi_status NAPI_CDECL napi_delete_element(napi_env env,
                                                         napi_value object,
                                                         uint32_t index,
                                                         bool* result) {
  return g_node_api_js_native_vtable->delete_element(
      env, object, index, result);
}

static inline napi_status NAPI_CDECL
napi_define_properties(napi_env env,
                       napi_value object,
                       size_t property_count,
                       const napi_property_descriptor* properties) {
  return g_node_api_js_native_vtable->define_properties(
      env, object, property_count, properties);
}

static inline napi_status NAPI_CDECL napi_is_array(napi_env env,
                                                   napi_value value,
                                                   bool* result) {
  return g_node_api_js_native_vtable->is_array(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_array_length(napi_env env,
                                                           napi_value value,
                                                           uint32_t* result) {
  return g_node_api_js_native_vtable->get_array_length(env, value, result);
}

static inline napi_status NAPI_CDECL napi_strict_equals(napi_env env,
                                                        napi_value lhs,
                                                        napi_value rhs,
                                                        bool* result) {
  return g_node_api_js_native_vtable->strict_equals(env, lhs, rhs, result);
}

static inline napi_status NAPI_CDECL napi_call_function(napi_env env,
                                                        napi_value recv,
                                                        napi_value func,
                                                        size_t argc,
                                                        const napi_value* argv,
                                                        napi_value* result) {
  return g_node_api_js_native_vtable->call_function(
      env, recv, func, argc, argv, result);
}

static inline napi_status NAPI_CDECL napi_new_instance(napi_env env,
                                                       napi_value constructor,
                                                       size_t argc,
                                                       const napi_value* argv,
                                                       napi_value* result) {
  return g_node_api_js_native_vtable->new_instance(
      env, constructor, argc, argv, result);
}

static inline napi_status NAPI_CDECL napi_instanceof(napi_env env,
                                                     napi_value object,
                                                     napi_value constructor,
                                                     bool* result) {
  return g_node_api_js_native_vtable->instanceof
      (env, object, constructor, result);
}

static inline napi_status NAPI_CDECL napi_get_cb_info(napi_env env,
                                                      napi_callback_info cbinfo,
                                                      size_t* argc,
                                                      napi_value* argv,
                                                      napi_value* this_arg,
                                                      void** data) {
  return g_node_api_js_native_vtable->get_cb_info(
      env, cbinfo, argc, argv, this_arg, data);
}

static inline napi_status NAPI_CDECL napi_get_new_target(
    napi_env env, napi_callback_info cbinfo, napi_value* result) {
  return g_node_api_js_native_vtable->get_new_target(env, cbinfo, result);
}

static inline napi_status NAPI_CDECL
napi_define_class(napi_env env,
                  const char* utf8name,
                  size_t length,
                  napi_callback constructor,
                  void* data,
                  size_t property_count,
                  const napi_property_descriptor* properties,
                  napi_value* result) {
  return g_node_api_js_native_vtable->define_class(env,
                                                   utf8name,
                                                   length,
                                                   constructor,
                                                   data,
                                                   property_count,
                                                   properties,
                                                   result);
}

static inline napi_status NAPI_CDECL
napi_wrap(napi_env env,
          napi_value js_object,
          void* native_object,
          node_api_basic_finalize finalize_cb,
          void* finalize_hint,
          napi_ref* result) {
  return g_node_api_js_native_vtable->wrap(
      env, js_object, native_object, finalize_cb, finalize_hint, result);
}

static inline napi_status NAPI_CDECL napi_unwrap(napi_env env,
                                                 napi_value js_object,
                                                 void** result) {
  return g_node_api_js_native_vtable->unwrap(env, js_object, result);
}

static inline napi_status NAPI_CDECL napi_remove_wrap(napi_env env,
                                                      napi_value js_object,
                                                      void** result) {
  return g_node_api_js_native_vtable->remove_wrap(env, js_object, result);
}

static inline napi_status NAPI_CDECL
napi_create_external(napi_env env,
                     void* data,
                     node_api_basic_finalize finalize_cb,
                     void* finalize_hint,
                     napi_value* result) {
  return g_node_api_js_native_vtable->create_external(
      env, data, finalize_cb, finalize_hint, result);
}

static inline napi_status NAPI_CDECL napi_get_value_external(napi_env env,
                                                             napi_value value,
                                                             void** result) {
  return g_node_api_js_native_vtable->get_value_external(env, value, result);
}
static inline napi_status NAPI_CDECL
napi_create_reference(napi_env env,
                      napi_value value,
                      uint32_t initial_refcount,
                      napi_ref* result) {
  return g_node_api_js_native_vtable->create_reference(
      env, value, initial_refcount, result);
}

static inline napi_status NAPI_CDECL
napi_delete_reference(node_api_basic_env env, napi_ref ref) {
  return g_node_api_js_native_vtable->delete_reference(env, ref);
}

static inline napi_status NAPI_CDECL napi_reference_ref(napi_env env,
                                                        napi_ref ref,
                                                        uint32_t* result) {
  return g_node_api_js_native_vtable->reference_ref(env, ref, result);
}

static inline napi_status NAPI_CDECL napi_reference_unref(napi_env env,
                                                          napi_ref ref,
                                                          uint32_t* result) {
  return g_node_api_js_native_vtable->reference_unref(env, ref, result);
}

static inline napi_status NAPI_CDECL
napi_get_reference_value(napi_env env, napi_ref ref, napi_value* result) {
  return g_node_api_js_native_vtable->get_reference_value(env, ref, result);
}

static inline napi_status NAPI_CDECL
napi_open_handle_scope(napi_env env, napi_handle_scope* result) {
  return g_node_api_js_native_vtable->open_handle_scope(env, result);
}

static inline napi_status NAPI_CDECL
napi_close_handle_scope(napi_env env, napi_handle_scope scope) {
  return g_node_api_js_native_vtable->close_handle_scope(env, scope);
}

static inline napi_status NAPI_CDECL napi_open_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope* result) {
  return g_node_api_js_native_vtable->open_escapable_handle_scope(env, result);
}

static inline napi_status NAPI_CDECL napi_close_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope scope) {
  return g_node_api_js_native_vtable->close_escapable_handle_scope(env, scope);
}

static inline napi_status NAPI_CDECL
napi_escape_handle(napi_env env,
                   napi_escapable_handle_scope scope,
                   napi_value escapee,
                   napi_value* result) {
  return g_node_api_js_native_vtable->escape_handle(
      env, scope, escapee, result);
}

static inline napi_status NAPI_CDECL napi_throw(napi_env env,
                                                napi_value error) {
  return g_node_api_js_native_vtable->throw_value(env, error);
}

static inline napi_status NAPI_CDECL napi_throw_error(napi_env env,
                                                      const char* code,
                                                      const char* msg) {
  return g_node_api_js_native_vtable->throw_error(env, code, msg);
}

static inline napi_status NAPI_CDECL napi_throw_type_error(napi_env env,
                                                           const char* code,
                                                           const char* msg) {
  return g_node_api_js_native_vtable->throw_type_error(env, code, msg);
}

static inline napi_status NAPI_CDECL napi_throw_range_error(napi_env env,
                                                            const char* code,
                                                            const char* msg) {
  return g_node_api_js_native_vtable->throw_range_error(env, code, msg);
}

#if NAPI_VERSION >= 9
static inline napi_status NAPI_CDECL
node_api_throw_syntax_error(napi_env env, const char* code, const char* msg) {
  return g_node_api_js_native_vtable->throw_syntax_error(env, code, msg);
}
#endif  // NAPI_VERSION >= 9

static inline napi_status NAPI_CDECL napi_is_error(napi_env env,
                                                   napi_value value,
                                                   bool* result) {
  return g_node_api_js_native_vtable->is_error(env, value, result);
}

static inline napi_status NAPI_CDECL napi_is_exception_pending(napi_env env,
                                                               bool* result) {
  return g_node_api_js_native_vtable->is_exception_pending(env, result);
}

static inline napi_status NAPI_CDECL
napi_get_and_clear_last_exception(napi_env env, napi_value* result) {
  return g_node_api_js_native_vtable->get_and_clear_last_exception(env, result);
}

static inline napi_status NAPI_CDECL napi_is_arraybuffer(napi_env env,
                                                         napi_value value,
                                                         bool* result) {
  return g_node_api_js_native_vtable->is_arraybuffer(env, value, result);
}

static inline napi_status NAPI_CDECL napi_create_arraybuffer(
    napi_env env, size_t byte_length, void** data, napi_value* result) {
  return g_node_api_js_native_vtable->create_arraybuffer(
      env, byte_length, data, result);
}

#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
static inline napi_status NAPI_CDECL
napi_create_external_arraybuffer(napi_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 node_api_basic_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result) {
  return g_node_api_js_native_vtable->create_external_arraybuffer(
      env, external_data, byte_length, finalize_cb, finalize_hint, result);
}

#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

static inline napi_status NAPI_CDECL napi_get_arraybuffer_info(
    napi_env env, napi_value arraybuffer, void** data, size_t* byte_length) {
  return g_node_api_js_native_vtable->get_arraybuffer_info(
      env, arraybuffer, data, byte_length);
}

static inline napi_status NAPI_CDECL napi_is_typedarray(napi_env env,
                                                        napi_value value,
                                                        bool* result) {
  return g_node_api_js_native_vtable->is_typedarray(env, value, result);
}

static inline napi_status NAPI_CDECL
napi_create_typedarray(napi_env env,
                       napi_typedarray_type type,
                       size_t length,
                       napi_value arraybuffer,
                       size_t byte_offset,
                       napi_value* result) {
  return g_node_api_js_native_vtable->create_typedarray(
      env, type, length, arraybuffer, byte_offset, result);
}

static inline napi_status NAPI_CDECL
napi_get_typedarray_info(napi_env env,
                         napi_value typedarray,
                         napi_typedarray_type* type,
                         size_t* length,
                         void** data,
                         napi_value* arraybuffer,
                         size_t* byte_offset) {
  return g_node_api_js_native_vtable->get_typedarray_info(
      env, typedarray, type, length, data, arraybuffer, byte_offset);
}

static inline napi_status NAPI_CDECL
napi_create_dataview(napi_env env,
                     size_t length,
                     napi_value arraybuffer,
                     size_t byte_offset,
                     napi_value* result) {
  return g_node_api_js_native_vtable->create_dataview(
      env, length, arraybuffer, byte_offset, result);
}

static inline napi_status NAPI_CDECL napi_is_dataview(napi_env env,
                                                      napi_value value,
                                                      bool* result) {
  return g_node_api_js_native_vtable->is_dataview(env, value, result);
}

static inline napi_status NAPI_CDECL
napi_get_dataview_info(napi_env env,
                       napi_value dataview,
                       size_t* bytelength,
                       void** data,
                       napi_value* arraybuffer,
                       size_t* byte_offset) {
  return g_node_api_js_native_vtable->get_dataview_info(
      env, dataview, bytelength, data, arraybuffer, byte_offset);
}

#ifdef NAPI_EXPERIMENTAL
static inline napi_status NAPI_CDECL
node_api_is_sharedarraybuffer(napi_env env, napi_value value, bool* result) {
  return g_node_api_js_native_vtable->is_sharedarraybuffer(env, value, result);
}

static inline napi_status NAPI_CDECL node_api_create_sharedarraybuffer(
    napi_env env, size_t byte_length, void** data, napi_value* result) {
  return g_node_api_js_native_vtable->create_sharedarraybuffer(
      env, byte_length, data, result);
}
#endif  // NAPI_EXPERIMENTAL

static inline napi_status NAPI_CDECL napi_get_version(node_api_basic_env env,
                                                      uint32_t* result) {
  return g_node_api_js_native_vtable->get_version(env, result);
}

static inline napi_status NAPI_CDECL napi_create_promise(
    napi_env env, napi_deferred* deferred, napi_value* promise) {
  return g_node_api_js_native_vtable->create_promise(env, deferred, promise);
}

static inline napi_status NAPI_CDECL napi_resolve_deferred(
    napi_env env, napi_deferred deferred, napi_value resolution) {
  return g_node_api_js_native_vtable->resolve_deferred(
      env, deferred, resolution);
}

static inline napi_status NAPI_CDECL napi_reject_deferred(
    napi_env env, napi_deferred deferred, napi_value rejection) {
  return g_node_api_js_native_vtable->reject_deferred(env, deferred, rejection);
}

static inline napi_status NAPI_CDECL napi_is_promise(napi_env env,
                                                     napi_value value,
                                                     bool* is_promise) {
  return g_node_api_js_native_vtable->is_promise(env, value, is_promise);
}

static inline napi_status NAPI_CDECL napi_run_script(napi_env env,
                                                     napi_value script,
                                                     napi_value* result) {
  return g_node_api_js_native_vtable->run_script(env, script, result);
}

static inline napi_status NAPI_CDECL napi_adjust_external_memory(
    node_api_basic_env env, int64_t change_in_bytes, int64_t* adjusted_value) {
  return g_node_api_js_native_vtable->adjust_external_memory(
      env, change_in_bytes, adjusted_value);
}

#if NAPI_VERSION >= 5

static inline napi_status NAPI_CDECL napi_create_date(napi_env env,
                                                      double time,
                                                      napi_value* result) {
  return g_node_api_js_native_vtable->create_date(env, time, result);
}

static inline napi_status NAPI_CDECL napi_is_date(napi_env env,
                                                  napi_value value,
                                                  bool* is_date) {
  return g_node_api_js_native_vtable->is_date(env, value, is_date);
}

static inline napi_status NAPI_CDECL napi_get_date_value(napi_env env,
                                                         napi_value value,
                                                         double* result) {
  return g_node_api_js_native_vtable->get_date_value(env, value, result);
}

static inline napi_status NAPI_CDECL
napi_add_finalizer(napi_env env,
                   napi_value js_object,
                   void* finalize_data,
                   node_api_basic_finalize finalize_cb,
                   void* finalize_hint,
                   napi_ref* result) {
  return g_node_api_js_native_vtable->add_finalizer(
      env, js_object, finalize_data, finalize_cb, finalize_hint, result);
}

#endif  // NAPI_VERSION >= 5

#ifdef NAPI_EXPERIMENTAL

static inline napi_status NAPI_CDECL
node_api_post_finalizer(node_api_basic_env env,
                        napi_finalize finalize_cb,
                        void* finalize_data,
                        void* finalize_hint) {
  return g_node_api_js_native_vtable->post_finalizer(
      env, finalize_cb, finalize_data, finalize_hint);
}

#endif  // NAPI_EXPERIMENTAL

#if NAPI_VERSION >= 6

static inline napi_status NAPI_CDECL
napi_create_bigint_int64(napi_env env, int64_t value, napi_value* result) {
  return g_node_api_js_native_vtable->create_bigint_int64(env, value, result);
}

static inline napi_status NAPI_CDECL
napi_create_bigint_uint64(napi_env env, uint64_t value, napi_value* result) {
  return g_node_api_js_native_vtable->create_bigint_uint64(env, value, result);
}

static inline napi_status NAPI_CDECL
napi_create_bigint_words(napi_env env,
                         int sign_bit,
                         size_t word_count,
                         const uint64_t* words,
                         napi_value* result) {
  return g_node_api_js_native_vtable->create_bigint_words(
      env, sign_bit, word_count, words, result);
}

static inline napi_status NAPI_CDECL napi_get_value_bigint_int64(
    napi_env env, napi_value value, int64_t* result, bool* lossless) {
  return g_node_api_js_native_vtable->get_value_bigint_int64(
      env, value, result, lossless);
}

static inline napi_status NAPI_CDECL napi_get_value_bigint_uint64(
    napi_env env, napi_value value, uint64_t* result, bool* lossless) {
  return g_node_api_js_native_vtable->get_value_bigint_uint64(
      env, value, result, lossless);
}

static inline napi_status NAPI_CDECL
napi_get_value_bigint_words(napi_env env,
                            napi_value value,
                            int* sign_bit,
                            size_t* word_count,
                            uint64_t* words) {
  return g_node_api_js_native_vtable->get_value_bigint_words(
      env, value, sign_bit, word_count, words);
}

static inline napi_status NAPI_CDECL
napi_get_all_property_names(napi_env env,
                            napi_value object,
                            napi_key_collection_mode key_mode,
                            napi_key_filter key_filter,
                            napi_key_conversion key_conversion,
                            napi_value* result) {
  return g_node_api_js_native_vtable->get_all_property_names(
      env, object, key_mode, key_filter, key_conversion, result);
}

static inline napi_status NAPI_CDECL
napi_set_instance_data(node_api_basic_env env,
                       void* data,
                       napi_finalize finalize_cb,
                       void* finalize_hint) {
  return g_node_api_js_native_vtable->set_instance_data(
      env, data, finalize_cb, finalize_hint);
}

static inline napi_status NAPI_CDECL
napi_get_instance_data(node_api_basic_env env, void** data) {
  return g_node_api_js_native_vtable->get_instance_data(env, data);
}

#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7

static inline napi_status NAPI_CDECL
napi_detach_arraybuffer(napi_env env, napi_value arraybuffer) {
  return g_node_api_js_native_vtable->detach_arraybuffer(env, arraybuffer);
}

static inline napi_status NAPI_CDECL
napi_is_detached_arraybuffer(napi_env env, napi_value value, bool* result) {
  return g_node_api_js_native_vtable->is_detached_arraybuffer(
      env, value, result);
}

#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8

static inline napi_status NAPI_CDECL napi_type_tag_object(
    napi_env env, napi_value value, const napi_type_tag* type_tag) {
  return g_node_api_js_native_vtable->type_tag_object(env, value, type_tag);
}

static inline napi_status NAPI_CDECL
napi_check_object_type_tag(napi_env env,
                           napi_value value,
                           const napi_type_tag* type_tag,
                           bool* result) {
  return g_node_api_js_native_vtable->check_object_type_tag(
      env, value, type_tag, result);
}

static inline napi_status NAPI_CDECL napi_object_freeze(napi_env env,
                                                        napi_value object) {
  return g_node_api_js_native_vtable->object_freeze(env, object);
}

static inline napi_status NAPI_CDECL napi_object_seal(napi_env env,
                                                      napi_value object) {
  return g_node_api_js_native_vtable->object_seal(env, object);
}

#endif  // NAPI_VERSION >= 8

#endif  // NODE_API_MODULE_USE_VTABLE

EXTERN_C_END

#endif  // SRC_JS_NATIVE_API_H_
