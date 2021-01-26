#ifndef SRC_JS_NATIVE_API_TYPES_H_
#define SRC_JS_NATIVE_API_TYPES_H_

// This file needs to be compatible with C compilers.
// This is a public include file, and these includes have essentially
// became part of it's API.
#include <stddef.h>  // NOLINT(modernize-deprecated-headers)
#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

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
typedef struct napi_callback_info__* napi_callback_info;
typedef struct napi_deferred__* napi_deferred;
#define node_api_env napi_env
#define node_api_value napi_value
#define node_api_ref napi_ref
#define node_api_handle_scope napi_handle_scope
#define node_api_escapable_handle_scope napi_escapable_handle_scope
#define node_api_callback_info napi_callback_info
#define node_api_deferred napi_deferred

typedef enum {
  napi_default = 0,
  napi_writable = 1 << 0,
  napi_enumerable = 1 << 1,
  napi_configurable = 1 << 2,

  // Used with napi_define_class to distinguish static properties
  // from instance properties. Ignored by napi_define_properties.
  napi_static = 1 << 10,

#ifdef NAPI_EXPERIMENTAL
  // Default for class methods.
  napi_default_method = napi_writable | napi_configurable,

  // Default for object properties, like in JS obj[prop].
  napi_default_jsproperty = napi_writable |
                            napi_enumerable |
                            napi_configurable,
#endif  // NAPI_EXPERIMENTAL
} napi_property_attributes;
#define node_api_default napi_default
#define node_api_writable napi_writable
#define node_api_enumerable napi_enumerable
#define node_api_configurable napi_configurable
#define node_api_static napi_static
#ifdef NAPI_EXPERIMENTAL
#define node_api_default_method napi_default_method
#define node_api_default_jsproperty napi_default_jsproperty
#endif  // NAPI_EXPERIMENTAL
#define node_api_property_attributes napi_property_attributes

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
#define node_api_undefined napi_undefined
#define node_api_null napi_null
#define node_api_boolean napi_boolean
#define node_api_number napi_number
#define node_api_string napi_string
#define node_api_symbol napi_symbol
#define node_api_object napi_object
#define node_api_function napi_function
#define node_api_external napi_external
#define node_api_bigint napi_bigint
#define node_api_valuetype napi_valuetype

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
#define node_api_int8_array napi_int8_array
#define node_api_uint8_array napi_uint8_array
#define node_api_uint8_clamped_array napi_uint8_clamped_array
#define node_api_int16_array napi_int16_array
#define node_api_uint16_array napi_uint16_array
#define node_api_int32_array napi_int32_array
#define node_api_uint32_array napi_uint32_array
#define node_api_float32_array napi_float32_array
#define node_api_float64_array napi_float64_array
#define node_api_bigint64_array napi_bigint64_array
#define node_api_biguint64_array napi_biguint64_array
#define node_api_typedarray_type napi_typedarray_type

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
  napi_would_deadlock  // unused
} napi_status;
// Note: when adding a new enum value to `napi_status`, please also update
//   * `const int last_status` in the definition of `napi_get_last_error_info()'
//     in file js_native_api_v8.cc.
//   * `const char* error_messages[]` in file js_native_api_v8.cc with a brief
//     message explaining the error.
//   * the definition of `napi_status` in doc/api/n-api.md to reflect the newly
//     added value(s).

#define node_api_ok napi_ok
#define node_api_invalid_arg napi_invalid_arg
#define node_api_object_expected napi_object_expected
#define node_api_string_expected napi_string_expected
#define node_api_name_expected napi_name_expected
#define node_api_function_expected napi_function_expected
#define node_api_number_expected napi_number_expected
#define node_api_boolean_expected napi_boolean_expected
#define node_api_array_expected napi_array_expected
#define node_api_generic_failure napi_generic_failure
#define node_api_pending_exception napi_pending_exception
#define node_api_cancelled napi_cancelled
#define node_api_escape_called_twice napi_escape_called_twice
#define node_api_handle_scope_mismatch napi_handle_scope_mismatch
#define node_api_callback_scope_mismatch napi_callback_scope_mismatch
#define node_api_queue_full napi_queue_full
#define node_api_closing napi_closing
#define node_api_bigint_expected napi_bigint_expected
#define node_api_date_expected napi_date_expected
#define node_api_arraybuffer_expected napi_arraybuffer_expected
#define node_api_detachable_arraybuffer_expected \
  napi_detachable_arraybuffer_expected
#define node_api_would_deadlock napi_would_deadlock
#define node_api_status napi_status

typedef node_api_value (*napi_callback)(node_api_env env,
                                        node_api_callback_info info);
#define node_api_callback napi_callback
typedef void (*napi_finalize)(node_api_env env,
                              void* finalize_data,
                              void* finalize_hint);
#define node_api_finalize napi_finalize

typedef struct {
  // One of utf8name or name should be NULL.
  const char* utf8name;
  node_api_value name;

  node_api_callback method;
  node_api_callback getter;
  node_api_callback setter;
  node_api_value value;

  node_api_property_attributes attributes;
  void* data;
} napi_property_descriptor;
#define node_api_property_descriptor napi_property_descriptor

typedef struct {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  node_api_status error_code;
} napi_extended_error_info;
#define node_api_extended_error_info napi_extended_error_info

#if NAPI_VERSION >= 6
typedef enum {
  napi_key_include_prototypes,
  napi_key_own_only
} napi_key_collection_mode;
#define node_api_key_include_prototypes napi_key_include_prototypes
#define node_api_key_own_only napi_key_own_only
#define node_api_key_collection_mode napi_key_collection_mode

typedef enum {
  napi_key_all_properties = 0,
  napi_key_writable = 1,
  napi_key_enumerable = 1 << 1,
  napi_key_configurable = 1 << 2,
  napi_key_skip_strings = 1 << 3,
  napi_key_skip_symbols = 1 << 4
} napi_key_filter;
#define node_api_key_all_properties napi_key_all_properties
#define node_api_key_writable napi_key_writable
#define node_api_key_enumerable napi_key_enumerable
#define node_api_key_configurable napi_key_configurable
#define node_api_key_skip_strings napi_key_skip_strings
#define node_api_key_skip_symbols napi_key_skip_symbols
#define node_api_key_filter napi_key_filter

typedef enum {
  napi_key_keep_numbers,
  napi_key_numbers_to_strings
} napi_key_conversion;
#define node_api_key_keep_numbers napi_key_keep_numbers
#define node_api_key_numbers_to_strings napi_key_numbers_to_strings
#define node_api_key_conversion napi_key_conversion
#endif  // NAPI_VERSION >= 6

#if defined(NAPI_EXPERIMENTAL) || defined(NODE_API_EXPERIMENTAL)
typedef struct {
  uint64_t lower;
  uint64_t upper;
} napi_type_tag;
#define node_api_type_tag napi_type_tag
#endif  // NAPI_EXPERIMENTAL || NODE_API_EXPERIMENTAL

#endif  // SRC_JS_NATIVE_API_TYPES_H_
