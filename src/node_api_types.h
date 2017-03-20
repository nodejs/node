#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#if !defined __cplusplus || (defined(_MSC_VER) && _MSC_VER < 1900)
    typedef uint16_t char16_t;
#endif

// JSVM API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_env__ *napi_env;
typedef struct napi_value__ *napi_value;
typedef struct napi_ref__ *napi_ref;
typedef struct napi_handle_scope__ *napi_handle_scope;
typedef struct napi_escapable_handle_scope__ *napi_escapable_handle_scope;
typedef struct napi_callback_info__ *napi_callback_info;

typedef void (*napi_callback)(napi_env, napi_callback_info);
typedef void (*napi_finalize)(void* finalize_data, void* finalize_hint);

enum napi_property_attributes {
  napi_default = 0,
  napi_read_only = 1 << 0,
  napi_dont_enum = 1 << 1,
  napi_dont_delete = 1 << 2,

  // Used with napi_define_class to distinguish static properties
  // from instance properties. Ignored by napi_define_properties.
  napi_static_property = 1 << 10,
};

struct napi_property_descriptor {
  const char* utf8name;

  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;

  napi_property_attributes attributes;
  void* data;
};

#define DEFAULT_ATTR 0, 0, 0, napi_default, 0

enum napi_valuetype {
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
};

enum napi_typedarray_type {
  napi_int8,
  napi_uint8,
  napi_uint8_clamped,
  napi_int16,
  napi_uint16,
  napi_int32,
  napi_uint32,
  napi_float32,
  napi_float64,
};

enum napi_status {
  napi_ok,
  napi_invalid_arg,
  napi_object_expected,
  napi_string_expected,
  napi_function_expected,
  napi_number_expected,
  napi_boolean_expected,
  napi_generic_failure,
  napi_pending_exception,
  napi_status_last
};

struct napi_extended_error_info {
  const char* error_message;
  void* engine_reserved;
  uint32_t engine_error_code;
  napi_status error_code;
};

#endif  // SRC_NODE_API_TYPES_H_
