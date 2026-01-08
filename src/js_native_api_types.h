#ifndef SRC_JS_NATIVE_API_TYPES_H_
#define SRC_JS_NATIVE_API_TYPES_H_

// Use INT_MAX, this should only be consumed by the pre-processor anyway.
#define NAPI_VERSION_EXPERIMENTAL 2147483647
#ifndef NAPI_VERSION
#ifdef NAPI_EXPERIMENTAL
#define NAPI_VERSION NAPI_VERSION_EXPERIMENTAL
#else
// The baseline version for Node-API.
// NAPI_VERSION controls which version is used by default when compiling
// a native addon. If the addon developer wants to use functions from a
// newer Node-API version not yet available in all LTS versions, they can
// set NAPI_VERSION to explicitly depend on that version.
#define NAPI_VERSION 8
#endif
#endif

#if defined(NAPI_EXPERIMENTAL) &&                                              \
    !defined(NODE_API_EXPERIMENTAL_NO_WARNING) &&                              \
    !defined(NODE_WANT_INTERNALS)
#ifdef _MSC_VER
#pragma message("NAPI_EXPERIMENTAL is enabled. "                               \
                "Experimental features may be unstable.")
#else
#warning "NAPI_EXPERIMENTAL is enabled. " \
       "Experimental features may be unstable."
#endif
#endif

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
// This file needs to be compatible with C compilers.
// This is a public include file, and these includes have essentially
// become part of its API.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#if !defined(__cplusplus) || (defined(_MSC_VER) && _MSC_VER < 1900)
typedef uint16_t char16_t;
#endif

#ifndef NAPI_CDECL
#ifdef _WIN32
#define NAPI_CDECL __cdecl
#else
#define NAPI_CDECL
#endif
#endif

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__* napi_env;

// We need to mark APIs which can be called during garbage collection (GC),
// meaning that they do not affect the state of the JS engine, and can
// therefore be called synchronously from a finalizer that itself runs
// synchronously during GC. Such APIs can receive either a `napi_env` or a
// `node_api_basic_env` as their first parameter, because we should be able to
// also call them during normal, non-garbage-collecting operations, whereas
// APIs that affect the state of the JS engine can only receive a `napi_env` as
// their first parameter, because we must not call them during GC. In lieu of
// inheritance, we use the properties of the const qualifier to accomplish
// this, because both a const and a non-const value can be passed to an API
// expecting a const value, but only a non-const value can be passed to an API
// expecting a non-const value.
//
// In conjunction with appropriate CFLAGS to warn us if we're passing a const
// (basic) environment into an API that expects a non-const environment, and
// the definition of basic finalizer function pointer types below, which
// receive a basic environment as their first parameter, and can thus only call
// basic APIs (unless the user explicitly casts the environment), we achieve
// the ability to ensure at compile time that we do not call APIs that affect
// the state of the JS engine from a synchronous (basic) finalizer.
#if !defined(NAPI_EXPERIMENTAL) ||                                             \
    (defined(NAPI_EXPERIMENTAL) &&                                             \
     (defined(NODE_API_EXPERIMENTAL_NOGC_ENV_OPT_OUT) ||                       \
      defined(NODE_API_EXPERIMENTAL_BASIC_ENV_OPT_OUT)))
typedef struct napi_env__* node_api_nogc_env;
#else
typedef const struct napi_env__* node_api_nogc_env;
#endif
typedef node_api_nogc_env node_api_basic_env;

typedef struct napi_value__* napi_value;
typedef struct napi_ref__* napi_ref;
typedef struct napi_handle_scope__* napi_handle_scope;
typedef struct napi_escapable_handle_scope__* napi_escapable_handle_scope;
typedef struct napi_callback_info__* napi_callback_info;
typedef struct napi_deferred__* napi_deferred;

typedef enum {
  napi_default = 0,
  napi_writable = 1 << 0,
  napi_enumerable = 1 << 1,
  napi_configurable = 1 << 2,

  // Used with napi_define_class to distinguish static properties
  // from instance properties. Ignored by napi_define_properties.
  napi_static = 1 << 10,

#if NAPI_VERSION >= 8
  // Default for class methods.
  napi_default_method = napi_writable | napi_configurable,

  // Default for object properties, like in JS obj[prop].
  napi_default_jsproperty = napi_writable | napi_enumerable | napi_configurable,
#endif  // NAPI_VERSION >= 8
} napi_property_attributes;

typedef enum {
  // ES6 types (corresponds to typeof)
  napi_undefined,
  napi_null,
  napi_boolean,
  napi_number,
  napi_string,
  napi_symbol,
  napi_object,
  napi_function,
  napi_external,
  napi_bigint,
} napi_valuetype;

typedef enum {
  napi_int8_array,
  napi_uint8_array,
  napi_uint8_clamped_array,
  napi_int16_array,
  napi_uint16_array,
  napi_int32_array,
  napi_uint32_array,
  napi_float32_array,
  napi_float64_array,
  napi_bigint64_array,
  napi_biguint64_array,
#define NODE_API_HAS_FLOAT16_ARRAY
  napi_float16_array,
} napi_typedarray_type;

typedef enum {
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_name_expected,
  napi_function_expected,
  napi_number_expected,
  napi_boolean_expected,
  napi_array_expected,
  napi_generic_failure,
  napi_pending_exception,
  napi_cancelled,
  napi_escape_called_twice,
  napi_handle_scope_mismatch,
  napi_callback_scope_mismatch,
  napi_queue_full,
  napi_closing,
  napi_bigint_expected,
  napi_date_expected,
  napi_arraybuffer_expected,
  napi_detachable_arraybuffer_expected,
  napi_would_deadlock,  // unused
  napi_no_external_buffers_allowed,
  napi_cannot_run_js,
} napi_status;
// Note: when adding a new enum value to `napi_status`, please also update
//   * `const int last_status` in the definition of `napi_get_last_error_info()'
//     in file js_native_api_v8.cc.
//   * `const char* error_messages[]` in file js_native_api_v8.cc with a brief
//     message explaining the error.
//   * the definition of `napi_status` in doc/api/n-api.md to reflect the newly
//     added value(s).

typedef napi_value(NAPI_CDECL* napi_callback)(napi_env env,
                                              napi_callback_info info);
typedef void(NAPI_CDECL* napi_finalize)(napi_env env,
                                        void* finalize_data,
                                        void* finalize_hint);

#if !defined(NAPI_EXPERIMENTAL) ||                                             \
    (defined(NAPI_EXPERIMENTAL) &&                                             \
     (defined(NODE_API_EXPERIMENTAL_NOGC_ENV_OPT_OUT) ||                       \
      defined(NODE_API_EXPERIMENTAL_BASIC_ENV_OPT_OUT)))
typedef napi_finalize node_api_nogc_finalize;
#else
typedef void(NAPI_CDECL* node_api_nogc_finalize)(node_api_nogc_env env,
                                                 void* finalize_data,
                                                 void* finalize_hint);
#endif
typedef node_api_nogc_finalize node_api_basic_finalize;

typedef struct {
  // One of utf8name or name should be NULL.
  const char* utf8name;
  napi_value name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
} napi_property_descriptor;

typedef struct {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
} napi_extended_error_info;

#if NAPI_VERSION >= 6
typedef enum {
  napi_key_include_prototypes,
  napi_key_own_only
} napi_key_collection_mode;

typedef enum {
  napi_key_all_properties = 0,
  napi_key_writable = 1,
  napi_key_enumerable = 1 << 1,
  napi_key_configurable = 1 << 2,
  napi_key_skip_strings = 1 << 3,
  napi_key_skip_symbols = 1 << 4
} napi_key_filter;

typedef enum {
  napi_key_keep_numbers,
  napi_key_numbers_to_strings
} napi_key_conversion;
#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 8
typedef struct {
  uint64_t lower;
  uint64_t upper;
} napi_type_tag;
#endif  // NAPI_VERSION >= 8

#if defined(NODE_API_MODULE_USE_VTABLE) || defined(NODE_API_RUNTIME_USE_VTABLE)

// Vtable for JavaScript to native interop functions.
// New functions must be added at the end to maintain backward compatibility.
typedef struct node_api_js_vtable {
  napi_status(NAPI_CDECL* get_last_error_info)(
      node_api_basic_env env, const napi_extended_error_info** result);

  napi_status(NAPI_CDECL* get_undefined)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_null)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_global)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_boolean)(napi_env env,
                                       bool value,
                                       napi_value* result);

  napi_status(NAPI_CDECL* create_object)(napi_env env, napi_value* result);

  napi_status(NAPI_CDECL* create_array)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* create_array_with_length)(napi_env env,
                                                    size_t length,
                                                    napi_value* result);
  napi_status(NAPI_CDECL* create_double)(napi_env env,
                                         double value,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_int32)(napi_env env,
                                        int32_t value,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_uint32)(napi_env env,
                                         uint32_t value,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_int64)(napi_env env,
                                        int64_t value,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_string_latin1)(napi_env env,
                                                const char* str,
                                                size_t length,
                                                napi_value* result);
  napi_status(NAPI_CDECL* create_string_utf8)(napi_env env,
                                              const char* str,
                                              size_t length,
                                              napi_value* result);
  napi_status(NAPI_CDECL* create_string_utf16)(napi_env env,
                                               const char16_t* str,
                                               size_t length,
                                               napi_value* result);

  napi_status(NAPI_CDECL* create_symbol)(napi_env env,
                                         napi_value description,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_function)(napi_env env,
                                           const char* utf8name,
                                           size_t length,
                                           napi_callback cb,
                                           void* data,
                                           napi_value* result);
  napi_status(NAPI_CDECL* create_error)(napi_env env,
                                        napi_value code,
                                        napi_value msg,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_type_error)(napi_env env,
                                             napi_value code,
                                             napi_value msg,
                                             napi_value* result);
  napi_status(NAPI_CDECL* create_range_error)(napi_env env,
                                              napi_value code,
                                              napi_value msg,
                                              napi_value* result);

  // The name is changed to avoid conflict with the `typeof` keyword in C
  napi_status(NAPI_CDECL* type_of)(napi_env env,
                                   napi_value value,
                                   napi_valuetype* result);
  napi_status(NAPI_CDECL* get_value_double)(napi_env env,
                                            napi_value value,
                                            double* result);
  napi_status(NAPI_CDECL* get_value_int32)(napi_env env,
                                           napi_value value,
                                           int32_t* result);
  napi_status(NAPI_CDECL* get_value_uint32)(napi_env env,
                                            napi_value value,
                                            uint32_t* result);
  napi_status(NAPI_CDECL* get_value_int64)(napi_env env,
                                           napi_value value,
                                           int64_t* result);
  napi_status(NAPI_CDECL* get_value_bool)(napi_env env,
                                          napi_value value,
                                          bool* result);

  napi_status(NAPI_CDECL* get_value_string_latin1)(napi_env env,
                                                   napi_value value,
                                                   char* buf,
                                                   size_t bufsize,
                                                   size_t* result);

  napi_status(NAPI_CDECL* get_value_string_utf8)(napi_env env,
                                                 napi_value value,
                                                 char* buf,
                                                 size_t bufsize,
                                                 size_t* result);

  napi_status(NAPI_CDECL* get_value_string_utf16)(napi_env env,
                                                  napi_value value,
                                                  char16_t* buf,
                                                  size_t bufsize,
                                                  size_t* result);

  napi_status(NAPI_CDECL* coerce_to_bool)(napi_env env,
                                          napi_value value,
                                          napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_number)(napi_env env,
                                            napi_value value,
                                            napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_object)(napi_env env,
                                            napi_value value,
                                            napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_string)(napi_env env,
                                            napi_value value,
                                            napi_value* result);

  napi_status(NAPI_CDECL* get_prototype)(napi_env env,
                                         napi_value object,
                                         napi_value* result);
  napi_status(NAPI_CDECL* get_property_names)(napi_env env,
                                              napi_value object,
                                              napi_value* result);
  napi_status(NAPI_CDECL* set_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        napi_value value);
  napi_status(NAPI_CDECL* has_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        bool* result);
  napi_status(NAPI_CDECL* get_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        napi_value* result);
  napi_status(NAPI_CDECL* delete_property)(napi_env env,
                                           napi_value object,
                                           napi_value key,
                                           bool* result);
  napi_status(NAPI_CDECL* has_own_property)(napi_env env,
                                            napi_value object,
                                            napi_value key,
                                            bool* result);
  napi_status(NAPI_CDECL* set_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              napi_value value);
  napi_status(NAPI_CDECL* has_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              bool* result);
  napi_status(NAPI_CDECL* get_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              napi_value* result);
  napi_status(NAPI_CDECL* set_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       napi_value value);
  napi_status(NAPI_CDECL* has_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       bool* result);
  napi_status(NAPI_CDECL* get_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       napi_value* result);
  napi_status(NAPI_CDECL* delete_element)(napi_env env,
                                          napi_value object,
                                          uint32_t index,
                                          bool* result);
  napi_status(NAPI_CDECL* define_properties)(
      napi_env env,
      napi_value object,
      size_t property_count,
      const napi_property_descriptor* properties);

  napi_status(NAPI_CDECL* is_array)(napi_env env,
                                    napi_value value,
                                    bool* result);
  napi_status(NAPI_CDECL* get_array_length)(napi_env env,
                                            napi_value value,
                                            uint32_t* result);

  napi_status(NAPI_CDECL* strict_equals)(napi_env env,
                                         napi_value lhs,
                                         napi_value rhs,
                                         bool* result);

  napi_status(NAPI_CDECL* call_function)(napi_env env,
                                         napi_value recv,
                                         napi_value func,
                                         size_t argc,
                                         const napi_value* argv,
                                         napi_value* result);
  napi_status(NAPI_CDECL* new_instance)(napi_env env,
                                        napi_value constructor,
                                        size_t argc,
                                        const napi_value* argv,
                                        napi_value* result);
  napi_status(NAPI_CDECL* instanceof)(napi_env env,
                                      napi_value object,
                                      napi_value constructor,
                                      bool* result);

  napi_status(NAPI_CDECL* get_cb_info)(napi_env env,
                                       napi_callback_info cbinfo,
                                       size_t* argc,
                                       napi_value* argv,
                                       napi_value* this_arg,
                                       void** data);

  napi_status(NAPI_CDECL* get_new_target)(napi_env env,
                                          napi_callback_info cbinfo,
                                          napi_value* result);
  napi_status(NAPI_CDECL* define_class)(
      napi_env env,
      const char* utf8name,
      size_t length,
      napi_callback constructor,
      void* data,
      size_t property_count,
      const napi_property_descriptor* properties,
      napi_value* result);

  napi_status(NAPI_CDECL* wrap)(napi_env env,
                                napi_value js_object,
                                void* native_object,
                                node_api_basic_finalize finalize_cb,
                                void* finalize_hint,
                                napi_ref* result);
  napi_status(NAPI_CDECL* unwrap)(napi_env env,
                                  napi_value js_object,
                                  void** result);
  napi_status(NAPI_CDECL* remove_wrap)(napi_env env,
                                       napi_value js_object,
                                       void** result);
  napi_status(NAPI_CDECL* create_external)(napi_env env,
                                           void* data,
                                           node_api_basic_finalize finalize_cb,
                                           void* finalize_hint,
                                           napi_value* result);
  napi_status(NAPI_CDECL* get_value_external)(napi_env env,
                                              napi_value value,
                                              void** result);

  napi_status(NAPI_CDECL* create_reference)(napi_env env,
                                            napi_value value,
                                            uint32_t initial_refcount,
                                            napi_ref* result);

  napi_status(NAPI_CDECL* delete_reference)(node_api_basic_env env,
                                            napi_ref ref);

  napi_status(NAPI_CDECL* reference_ref)(napi_env env,
                                         napi_ref ref,
                                         uint32_t* result);

  napi_status(NAPI_CDECL* reference_unref)(napi_env env,
                                           napi_ref ref,
                                           uint32_t* result);

  napi_status(NAPI_CDECL* get_reference_value)(napi_env env,
                                               napi_ref ref,
                                               napi_value* result);

  napi_status(NAPI_CDECL* open_handle_scope)(napi_env env,
                                             napi_handle_scope* result);
  napi_status(NAPI_CDECL* close_handle_scope)(napi_env env,
                                              napi_handle_scope scope);
  napi_status(NAPI_CDECL* open_escapable_handle_scope)(
      napi_env env, napi_escapable_handle_scope* result);
  napi_status(NAPI_CDECL* close_escapable_handle_scope)(
      napi_env env, napi_escapable_handle_scope scope);

  napi_status(NAPI_CDECL* escape_handle)(napi_env env,
                                         napi_escapable_handle_scope scope,
                                         napi_value escapee,
                                         napi_value* result);

  // The name is changed to avoid conflict with the `throw` keyword in C++
  napi_status(NAPI_CDECL* throw_value)(napi_env env, napi_value error);
  napi_status(NAPI_CDECL* throw_error)(napi_env env,
                                       const char* code,
                                       const char* msg);
  napi_status(NAPI_CDECL* throw_type_error)(napi_env env,
                                            const char* code,
                                            const char* msg);
  napi_status(NAPI_CDECL* throw_range_error)(napi_env env,
                                             const char* code,
                                             const char* msg);
  napi_status(NAPI_CDECL* is_error)(napi_env env,
                                    napi_value value,
                                    bool* result);

  napi_status(NAPI_CDECL* is_exception_pending)(napi_env env, bool* result);
  napi_status(NAPI_CDECL* get_and_clear_last_exception)(napi_env env,
                                                        napi_value* result);

  napi_status(NAPI_CDECL* is_arraybuffer)(napi_env env,
                                          napi_value value,
                                          bool* result);
  napi_status(NAPI_CDECL* create_arraybuffer)(napi_env env,
                                              size_t byte_length,
                                              void** data,
                                              napi_value* result);
  napi_status(NAPI_CDECL* create_external_arraybuffer)(
      napi_env env,
      void* external_data,
      size_t byte_length,
      node_api_basic_finalize finalize_cb,
      void* finalize_hint,
      napi_value* result);
  napi_status(NAPI_CDECL* get_arraybuffer_info)(napi_env env,
                                                napi_value arraybuffer,
                                                void** data,
                                                size_t* byte_length);
  napi_status(NAPI_CDECL* is_typedarray)(napi_env env,
                                         napi_value value,
                                         bool* result);
  napi_status(NAPI_CDECL* create_typedarray)(napi_env env,
                                             napi_typedarray_type type,
                                             size_t length,
                                             napi_value arraybuffer,
                                             size_t byte_offset,
                                             napi_value* result);
  napi_status(NAPI_CDECL* get_typedarray_info)(napi_env env,
                                               napi_value typedarray,
                                               napi_typedarray_type* type,
                                               size_t* length,
                                               void** data,
                                               napi_value* arraybuffer,
                                               size_t* byte_offset);

  napi_status(NAPI_CDECL* create_dataview)(napi_env env,
                                           size_t length,
                                           napi_value arraybuffer,
                                           size_t byte_offset,
                                           napi_value* result);
  napi_status(NAPI_CDECL* is_dataview)(napi_env env,
                                       napi_value value,
                                       bool* result);
  napi_status(NAPI_CDECL* get_dataview_info)(napi_env env,
                                             napi_value dataview,
                                             size_t* bytelength,
                                             void** data,
                                             napi_value* arraybuffer,
                                             size_t* byte_offset);

  napi_status(NAPI_CDECL* get_version)(node_api_basic_env env,
                                       uint32_t* result);

  napi_status(NAPI_CDECL* create_promise)(napi_env env,
                                          napi_deferred* deferred,
                                          napi_value* promise);
  napi_status(NAPI_CDECL* resolve_deferred)(napi_env env,
                                            napi_deferred deferred,
                                            napi_value resolution);
  napi_status(NAPI_CDECL* reject_deferred)(napi_env env,
                                           napi_deferred deferred,
                                           napi_value rejection);
  napi_status(NAPI_CDECL* is_promise)(napi_env env,
                                      napi_value value,
                                      bool* is_promise);

  napi_status(NAPI_CDECL* run_script)(napi_env env,
                                      napi_value script,
                                      napi_value* result);

  napi_status(NAPI_CDECL* adjust_external_memory)(node_api_basic_env env,
                                                  int64_t change_in_bytes,
                                                  int64_t* adjusted_value);

#if NAPI_VERSION >= 5

  napi_status(NAPI_CDECL* create_date)(napi_env env,
                                       double time,
                                       napi_value* result);

  napi_status(NAPI_CDECL* is_date)(napi_env env,
                                   napi_value value,
                                   bool* is_date);

  napi_status(NAPI_CDECL* get_date_value)(napi_env env,
                                          napi_value value,
                                          double* result);

  napi_status(NAPI_CDECL* add_finalizer)(napi_env env,
                                         napi_value js_object,
                                         void* finalize_data,
                                         node_api_basic_finalize finalize_cb,
                                         void* finalize_hint,
                                         napi_ref* result);

#endif  // NAPI_VERSION >= 5

#if NAPI_VERSION >= 6

  napi_status(NAPI_CDECL* create_bigint_int64)(napi_env env,
                                               int64_t value,
                                               napi_value* result);
  napi_status(NAPI_CDECL* create_bigint_uint64)(napi_env env,
                                                uint64_t value,
                                                napi_value* result);
  napi_status(NAPI_CDECL* create_bigint_words)(napi_env env,
                                               int sign_bit,
                                               size_t word_count,
                                               const uint64_t* words,
                                               napi_value* result);
  napi_status(NAPI_CDECL* get_value_bigint_int64)(napi_env env,
                                                  napi_value value,
                                                  int64_t* result,
                                                  bool* lossless);
  napi_status(NAPI_CDECL* get_value_bigint_uint64)(napi_env env,
                                                   napi_value value,
                                                   uint64_t* result,
                                                   bool* lossless);
  napi_status(NAPI_CDECL* get_value_bigint_words)(napi_env env,
                                                  napi_value value,
                                                  int* sign_bit,
                                                  size_t* word_count,
                                                  uint64_t* words);

  napi_status(NAPI_CDECL* get_all_property_names)(
      napi_env env,
      napi_value object,
      napi_key_collection_mode key_mode,
      napi_key_filter key_filter,
      napi_key_conversion key_conversion,
      napi_value* result);

  napi_status(NAPI_CDECL* set_instance_data)(node_api_basic_env env,
                                             void* data,
                                             napi_finalize finalize_cb,
                                             void* finalize_hint);

  napi_status(NAPI_CDECL* get_instance_data)(node_api_basic_env env,
                                             void** data);

#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7

  napi_status(NAPI_CDECL* detach_arraybuffer)(napi_env env,
                                              napi_value arraybuffer);

  napi_status(NAPI_CDECL* is_detached_arraybuffer)(napi_env env,
                                                   napi_value value,
                                                   bool* result);
#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8

  napi_status(NAPI_CDECL* type_tag_object)(napi_env env,
                                           napi_value value,
                                           const napi_type_tag* type_tag);

  napi_status(NAPI_CDECL* check_object_type_tag)(napi_env env,
                                                 napi_value value,
                                                 const napi_type_tag* type_tag,
                                                 bool* result);
  napi_status(NAPI_CDECL* object_freeze)(napi_env env, napi_value object);
  napi_status(NAPI_CDECL* object_seal)(napi_env env, napi_value object);

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

  napi_status(NAPI_CDECL* symbol_for)(napi_env env,
                                      const char* utf8description,
                                      size_t length,
                                      napi_value* result);

  napi_status(NAPI_CDECL* create_syntax_error)(napi_env env,
                                               napi_value code,
                                               napi_value msg,
                                               napi_value* result);

  napi_status(NAPI_CDECL* throw_syntax_error)(napi_env env,
                                              const char* code,
                                              const char* msg);

#endif  // NAPI_VERSION >= 9

#if NAPI_VERSION >= 10

  napi_status(NAPI_CDECL* create_external_string_latin1)(
      napi_env env,
      char* str,
      size_t length,
      node_api_basic_finalize finalize_callback,
      void* finalize_hint,
      napi_value* result,
      bool* copied);
  napi_status(NAPI_CDECL* create_external_string_utf16)(
      napi_env env,
      char16_t* str,
      size_t length,
      node_api_basic_finalize finalize_callback,
      void* finalize_hint,
      napi_value* result,
      bool* copied);

  napi_status(NAPI_CDECL* create_property_key_latin1)(napi_env env,
                                                      const char* str,
                                                      size_t length,
                                                      napi_value* result);
  napi_status(NAPI_CDECL* create_property_key_utf8)(napi_env env,
                                                    const char* str,
                                                    size_t length,
                                                    napi_value* result);
  napi_status(NAPI_CDECL* create_property_key_utf16)(napi_env env,
                                                     const char16_t* str,
                                                     size_t length,
                                                     napi_value* result);

#endif  // NAPI_VERSION >= 10

#ifdef NAPI_EXPERIMENTAL

  napi_status(NAPI_CDECL* post_finalizer)(node_api_basic_env env,
                                          napi_finalize finalize_cb,
                                          void* finalize_data,
                                          void* finalize_hint);

  napi_status(NAPI_CDECL* create_object_with_properties)(
      napi_env env,
      napi_value prototype_or_null,
      napi_value* property_names,
      napi_value* property_values,
      size_t property_count,
      napi_value* result);

  napi_status(NAPI_CDECL* is_sharedarraybuffer)(napi_env env,
                                                napi_value value,
                                                bool* result);
  napi_status(NAPI_CDECL* create_sharedarraybuffer)(napi_env env,
                                                    size_t byte_length,
                                                    void** data,
                                                    napi_value* result);

  napi_status(NAPI_CDECL* set_prototype)(napi_env env,
                                         napi_value object,
                                         napi_value value);

#endif  // NAPI_EXPERIMENTAL
} node_api_js_vtable;

// Sentinel format: "NODE_VT" (7 bytes) + marker byte.
// Marker byte = (version << 1) | 1
//   - Bit 0 is always 1: ensures the sentinel can never match a C++ vtable
//     pointer (which is always pointer-aligned, thus bit 0 = 0).
//   - Bits 1-7: struct version number (0-127).
#define NODE_API_VT_SENTINEL_VERSION 0
#define NODE_API_VT_SENTINEL_MAKE(version)                                     \
  (0x4E4F44455F565400ULL | (((version) << 1) | 1))
#define NODE_API_VT_SENTINEL                                                   \
  NODE_API_VT_SENTINEL_MAKE(NODE_API_VT_SENTINEL_VERSION)

struct napi_env__ {
  uint64_t sentinel;  // Should be NODE_API_VT_SENTINEL
  const struct node_api_js_vtable* js_vtable;
  const struct node_api_module_vtable* module_vtable;
};

#endif  // defined(NODE_API_MODULE_USE_VTABLE) ||
        // defined(NODE_API_RUNTIME_USE_VTABLE)

#endif  // SRC_JS_NATIVE_API_TYPES_H_
