#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#if !defined __cplusplus || (defined(_MSC_VER) && _MSC_VER < 1900)
    typedef uint16_t char16_t;
#endif

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__* napi_env;
typedef struct napi_value__* napi_value;
typedef struct napi_ref__* napi_ref;
typedef struct napi_handle_scope__* napi_handle_scope;
typedef struct napi_escapable_handle_scope__* napi_escapable_handle_scope;
typedef struct napi_callback_scope__* napi_callback_scope;
typedef struct napi_callback_info__* napi_callback_info;
typedef struct napi_async_context__* napi_async_context;
typedef struct napi_async_work__* napi_async_work;
typedef struct napi_deferred__* napi_deferred;
#ifdef NAPI_EXPERIMENTAL
typedef struct napi_threadsafe_function__* napi_threadsafe_function;
#endif  // NAPI_EXPERIMENTAL

typedef enum {
  napi_default = 0,
  napi_writable = 1 << 0,
  napi_enumerable = 1 << 1,
  napi_configurable = 1 << 2,

  // Used with napi_define_class to distinguish static properties
  // from instance properties. Ignored by napi_define_properties.
  napi_static = 1 << 10,
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
} napi_status;

#ifdef NAPI_EXPERIMENTAL
typedef enum {
  napi_tsfn_release,
  napi_tsfn_abort
} napi_threadsafe_function_release_mode;

typedef enum {
  napi_tsfn_nonblocking,
  napi_tsfn_blocking
} napi_threadsafe_function_call_mode;
#endif  // NAPI_EXPERIMENTAL

typedef napi_value (*napi_callback)(napi_env env,
                                    napi_callback_info info);
typedef void (*napi_finalize)(napi_env env,
                              void* finalize_data,
                              void* finalize_hint);
typedef void (*napi_async_execute_callback)(napi_env env,
                                            void* data);
typedef void (*napi_async_complete_callback)(napi_env env,
                                             napi_status status,
                                             void* data);

#ifdef NAPI_EXPERIMENTAL
typedef void (*napi_threadsafe_function_call_js)(napi_env env,
                                                 napi_value js_callback,
                                                 void* context,
                                                 void* data);
#endif  // NAPI_EXPERIMENTAL

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

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;

#endif  // SRC_NODE_API_TYPES_H_
