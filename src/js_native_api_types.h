#ifndef SRC_JS_NATIVE_API_TYPES_H_
#define SRC_JS_NATIVE_API_TYPES_H_

// This file needs to be compatible with C compilers.
// This is a public include file, and these includes have essentially
// became part of it's API.
#include <stddef.h>  // NOLINT(modernize-deprecated-headers)
#include <stdint.h>  // NOLINT(modernize-deprecated-headers)

#if !defined __cplusplus || (defined(_MSC_VER) && _MSC_VER < 1900)
    using char16_t = uint16_t;
#endif

// JSVM API types are all opaque pointers for ABI stability
// using undefined structs instead of void* for compile time type safety
using napi_env = struct napi_env__*;
using napi_value = struct napi_value__*;
using napi_ref = struct napi_ref__*;
using napi_handle_scope = struct napi_handle_scope__*;
using napi_escapable_handle_scope = struct napi_escapable_handle_scope__*;
using napi_callback_info = struct napi_callback_info__*;
using napi_deferred = struct napi_deferred__*;

using napi_property_attributes = enum {
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
  napi_default_jsproperty = napi_writable |
                            napi_enumerable |
                            napi_configurable,
#endif  // NAPI_VERSION >= 8
};

using napi_valuetype = enum {
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
};

using napi_typedarray_type = enum {
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
};

using napi_status = enum {
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
};
// Note: when adding a new enum value to `napi_status`, please also update
//   * `const int last_status` in the definition of `napi_get_last_error_info()'
//     in file js_native_api_v8.cc.
//   * `const char* error_messages[]` in file js_native_api_v8.cc with a brief
//     message explaining the error.
//   * the definition of `napi_status` in doc/api/n-api.md to reflect the newly
//     added value(s).

using napi_callback = napi_value (*)(napi_env env,
                                    napi_callback_info info);
using napi_finalize = void (*)(napi_env env,
                              void* finalize_data,
                              void* finalize_hint);

using napi_property_descriptor = struct {
  // One of utf8name or name should be NULL.
  const char* utf8name;
  napi_value name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
};

using napi_extended_error_info = struct {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
};

#if NAPI_VERSION >= 6
using napi_key_collection_mode = enum {
  napi_key_include_prototypes,
  napi_key_own_only
};

using napi_key_filter = enum {
  napi_key_all_properties = 0,
  napi_key_writable = 1,
  napi_key_enumerable = 1 << 1,
  napi_key_configurable = 1 << 2,
  napi_key_skip_strings = 1 << 3,
  napi_key_skip_symbols = 1 << 4
};

using napi_key_conversion = enum {
  napi_key_keep_numbers,
  napi_key_numbers_to_strings
};
#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 8
using napi_type_tag = struct {
  uint64_t lower;
  uint64_t upper;
};
#endif  // NAPI_VERSION >= 8

#endif  // SRC_JS_NATIVE_API_TYPES_H_
