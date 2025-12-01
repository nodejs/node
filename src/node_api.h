#ifndef SRC_NODE_API_H_
#define SRC_NODE_API_H_

#if defined(BUILDING_NODE_EXTENSION) && !defined(NAPI_EXTERN)
#ifdef _WIN32
#define NAPI_EXTERN
#elif defined(__wasm__)
#define NAPI_EXTERN __attribute__((__import_module__("napi")))
#endif
#endif
#include "js_native_api.h"
#include "node_api_types.h"

struct uv_loop_s;  // Forward declaration.

#ifdef _WIN32
#define NAPI_MODULE_EXPORT __declspec(dllexport)
#else
#ifdef __EMSCRIPTEN__
#define NAPI_MODULE_EXPORT                                                     \
  __attribute__((visibility("default"))) __attribute__((used))
#else
#define NAPI_MODULE_EXPORT __attribute__((visibility("default")))
#endif
#endif

#if defined(__GNUC__)
#define NAPI_NO_RETURN __attribute__((noreturn))
#elif defined(_WIN32)
#define NAPI_NO_RETURN __declspec(noreturn)
#else
#define NAPI_NO_RETURN
#endif

#define NAPI_MODULE_VERSION 1

#define NAPI_MODULE_INITIALIZER_X(base, version)                               \
  NAPI_MODULE_INITIALIZER_X_HELPER(base, version)
#define NAPI_MODULE_INITIALIZER_X_HELPER(base, version) base##version

#ifdef __wasm__
#define NAPI_MODULE_INITIALIZER_BASE napi_register_wasm_v
#else
#define NAPI_MODULE_INITIALIZER_BASE napi_register_module_v
#endif

#define NODE_API_MODULE_GET_API_VERSION_BASE node_api_module_get_api_version_v
#define NODE_API_MODULE_SET_VTABLE_BASE node_api_module_set_vtable_v

#define NAPI_MODULE_INITIALIZER                                                \
  NAPI_MODULE_INITIALIZER_X(NAPI_MODULE_INITIALIZER_BASE, NAPI_MODULE_VERSION)

#define NODE_API_MODULE_GET_API_VERSION                                        \
  NAPI_MODULE_INITIALIZER_X(NODE_API_MODULE_GET_API_VERSION_BASE,              \
                            NAPI_MODULE_VERSION)

#define NODE_API_MODULE_SET_VTABLE                                             \
  NAPI_MODULE_INITIALIZER_X(NODE_API_MODULE_SET_VTABLE_BASE,                   \
                            NAPI_MODULE_VERSION)

#define NAPI_MODULE_INIT()                                                     \
  EXTERN_C_START                                                               \
  NAPI_MODULE_EXPORT int32_t NODE_API_MODULE_GET_API_VERSION(void) {           \
    return NAPI_VERSION;                                                       \
  }                                                                            \
  node_api_vtable* g_vtable = NULL; /* NOLINT(readability/null_usage) */       \
  NAPI_MODULE_EXPORT void NODE_API_MODULE_SET_VTABLE(                          \
      node_api_vtable* vtable) {                                               \
    g_vtable = vtable;                                                         \
  }                                                                            \
  NAPI_MODULE_EXPORT napi_value NAPI_MODULE_INITIALIZER(napi_env env,          \
                                                        napi_value exports);   \
  EXTERN_C_END                                                                 \
  napi_value NAPI_MODULE_INITIALIZER(napi_env env, napi_value exports)

#define NAPI_MODULE(modname, regfunc)                                          \
  NAPI_MODULE_INIT() { return regfunc(env, exports); }

// Deprecated. Use NAPI_MODULE.
#define NAPI_MODULE_X(modname, regfunc, priv, flags)                           \
  NAPI_MODULE(modname, regfunc)

EXTERN_C_START

// Deprecated. Replaced by symbol-based registration defined by NAPI_MODULE
// and NAPI_MODULE_INIT macros.
NAPI_EXTERN void NAPI_CDECL napi_module_register(napi_module* mod);

NAPI_EXTERN NAPI_NO_RETURN void NAPI_CDECL
napi_fatal_error(const char* location,
                 size_t location_len,
                 const char* message,
                 size_t message_len);

// Methods for custom handling of async operations
NAPI_EXTERN napi_status NAPI_CDECL
napi_async_init(napi_env env,
                napi_value async_resource,
                napi_value async_resource_name,
                napi_async_context* result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_async_destroy(napi_env env, napi_async_context async_context);

NAPI_EXTERN napi_status NAPI_CDECL
napi_make_callback(napi_env env,
                   napi_async_context async_context,
                   napi_value recv,
                   napi_value func,
                   size_t argc,
                   const napi_value* argv,
                   napi_value* result);

// Methods to provide node::Buffer functionality with napi types
NAPI_EXTERN napi_status NAPI_CDECL napi_create_buffer(napi_env env,
                                                      size_t length,
                                                      void** data,
                                                      napi_value* result);
#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_external_buffer(napi_env env,
                            size_t length,
                            void* data,
                            node_api_basic_finalize finalize_cb,
                            void* finalize_hint,
                            napi_value* result);
#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

#if NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_buffer_from_arraybuffer(napi_env env,
                                        napi_value arraybuffer,
                                        size_t byte_offset,
                                        size_t byte_length,
                                        napi_value* result);
#endif  // NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL napi_create_buffer_copy(napi_env env,
                                                           size_t length,
                                                           const void* data,
                                                           void** result_data,
                                                           napi_value* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_is_buffer(napi_env env,
                                                  napi_value value,
                                                  bool* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_get_buffer_info(napi_env env,
                                                        napi_value value,
                                                        void** data,
                                                        size_t* length);

// Methods to manage simple async operations
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_async_work(napi_env env,
                       napi_value async_resource,
                       napi_value async_resource_name,
                       napi_async_execute_callback execute,
                       napi_async_complete_callback complete,
                       void* data,
                       napi_async_work* result);
NAPI_EXTERN napi_status NAPI_CDECL napi_delete_async_work(napi_env env,
                                                          napi_async_work work);
NAPI_EXTERN napi_status NAPI_CDECL napi_queue_async_work(node_api_basic_env env,
                                                         napi_async_work work);
NAPI_EXTERN napi_status NAPI_CDECL
napi_cancel_async_work(node_api_basic_env env, napi_async_work work);

// version management
NAPI_EXTERN napi_status NAPI_CDECL napi_get_node_version(
    node_api_basic_env env, const napi_node_version** version);

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_uv_event_loop(node_api_basic_env env, struct uv_loop_s** loop);

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

NAPI_EXTERN napi_status NAPI_CDECL napi_fatal_exception(napi_env env,
                                                        napi_value err);

NAPI_EXTERN napi_status NAPI_CDECL napi_add_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg);

NAPI_EXTERN napi_status NAPI_CDECL napi_remove_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg);

NAPI_EXTERN napi_status NAPI_CDECL
napi_open_callback_scope(napi_env env,
                         napi_value resource_object,
                         napi_async_context context,
                         napi_callback_scope* result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_close_callback_scope(napi_env env, napi_callback_scope scope);

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

// Calling into JS from other threads
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_threadsafe_function(napi_env env,
                                napi_value func,
                                napi_value async_resource,
                                napi_value async_resource_name,
                                size_t max_queue_size,
                                size_t initial_thread_count,
                                void* thread_finalize_data,
                                napi_finalize thread_finalize_cb,
                                void* context,
                                napi_threadsafe_function_call_js call_js_cb,
                                napi_threadsafe_function* result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_threadsafe_function_context(
    napi_threadsafe_function func, void** result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking);

NAPI_EXTERN napi_status NAPI_CDECL
napi_acquire_threadsafe_function(napi_threadsafe_function func);

NAPI_EXTERN napi_status NAPI_CDECL napi_release_threadsafe_function(
    napi_threadsafe_function func, napi_threadsafe_function_release_mode mode);

NAPI_EXTERN napi_status NAPI_CDECL napi_unref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func);

NAPI_EXTERN napi_status NAPI_CDECL napi_ref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func);

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

NAPI_EXTERN napi_status NAPI_CDECL
napi_add_async_cleanup_hook(node_api_basic_env env,
                            napi_async_cleanup_hook hook,
                            void* arg,
                            napi_async_cleanup_hook_handle* remove_handle);

NAPI_EXTERN napi_status NAPI_CDECL
napi_remove_async_cleanup_hook(napi_async_cleanup_hook_handle remove_handle);

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

NAPI_EXTERN napi_status NAPI_CDECL
node_api_get_module_file_name(node_api_basic_env env, const char** result);

#endif  // NAPI_VERSION >= 9

#ifndef NODE_WANT_INTERNALS

extern node_api_vtable* g_vtable;

inline napi_status NAPI_CDECL napi_get_last_error_info(
    node_api_basic_env env, const napi_extended_error_info** result) {
  return g_vtable->get_last_error_info(env, result);
}

inline napi_status NAPI_CDECL napi_get_undefined(napi_env env,
                                                 napi_value* result) {
  return g_vtable->get_undefined(env, result);
}

inline napi_status NAPI_CDECL napi_get_null(napi_env env, napi_value* result) {
  return g_vtable->get_null(env, result);
}

inline napi_status NAPI_CDECL napi_get_global(napi_env env,
                                              napi_value* result) {
  return g_vtable->get_global(env, result);
}

inline napi_status NAPI_CDECL napi_get_boolean(napi_env env,
                                               bool value,
                                               napi_value* result) {
  return g_vtable->get_boolean(env, value, result);
}

// Methods to create Primitive types/Objects
inline napi_status NAPI_CDECL napi_create_object(napi_env env,
                                                 napi_value* result) {
  return g_vtable->create_object(env, result);
}

#ifdef NAPI_EXPERIMENTAL
inline napi_status NAPI_CDECL
napi_create_object_with_properties(napi_env env,
                                   napi_value prototype_or_null,
                                   napi_value* property_names,
                                   napi_value* property_values,
                                   size_t property_count,
                                   napi_value* result) {
  return g_vtable->create_object_with_properties(env,
                                                 prototype_or_null,
                                                 property_names,
                                                 property_values,
                                                 property_count,
                                                 result);
}
#endif  // NAPI_EXPERIMENTAL

inline napi_status NAPI_CDECL napi_create_array(napi_env env,
                                                napi_value* result) {
  return g_vtable->create_array(env, result);
}

inline napi_status NAPI_CDECL
napi_create_array_with_length(napi_env env, size_t length, napi_value* result) {
  return g_vtable->create_array_with_length(env, length, result);
}

inline napi_status NAPI_CDECL napi_create_double(napi_env env,
                                                 double value,
                                                 napi_value* result) {
  return g_vtable->create_double(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_int32(napi_env env,
                                                int32_t value,
                                                napi_value* result) {
  return g_vtable->create_int32(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_uint32(napi_env env,
                                                 uint32_t value,
                                                 napi_value* result) {
  return g_vtable->create_uint32(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_int64(napi_env env,
                                                int64_t value,
                                                napi_value* result) {
  return g_vtable->create_int64(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_string_latin1(napi_env env,
                                                        const char* str,
                                                        size_t length,
                                                        napi_value* result) {
  return g_vtable->create_string_latin1(env, str, length, result);
}

inline napi_status NAPI_CDECL napi_create_string_utf8(napi_env env,
                                                      const char* str,
                                                      size_t length,
                                                      napi_value* result) {
  return g_vtable->create_string_utf8(env, str, length, result);
}

inline napi_status NAPI_CDECL napi_create_string_utf16(napi_env env,
                                                       const char16_t* str,
                                                       size_t length,
                                                       napi_value* result) {
  return g_vtable->create_string_utf16(env, str, length, result);
}

#if NAPI_VERSION >= 10
inline napi_status NAPI_CDECL node_api_create_external_string_latin1(
    napi_env env,
    char* str,
    size_t length,
    node_api_basic_finalize finalize_callback,
    void* finalize_hint,
    napi_value* result,
    bool* copied) {
  return g_vtable->create_external_string_latin1(
      env, str, length, finalize_callback, finalize_hint, result, copied);
}

inline napi_status NAPI_CDECL
node_api_create_external_string_utf16(napi_env env,
                                      char16_t* str,
                                      size_t length,
                                      node_api_basic_finalize finalize_callback,
                                      void* finalize_hint,
                                      napi_value* result,
                                      bool* copied) {
  return g_vtable->create_external_string_utf16(
      env, str, length, finalize_callback, finalize_hint, result, copied);
}

inline napi_status NAPI_CDECL node_api_create_property_key_latin1(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_vtable->create_property_key_latin1(env, str, length, result);
}

inline napi_status NAPI_CDECL node_api_create_property_key_utf8(
    napi_env env, const char* str, size_t length, napi_value* result) {
  return g_vtable->create_property_key_utf8(env, str, length, result);
}

inline napi_status NAPI_CDECL node_api_create_property_key_utf16(
    napi_env env, const char16_t* str, size_t length, napi_value* result) {
  return g_vtable->create_property_key_utf16(env, str, length, result);
}

#endif  // NAPI_VERSION >= 10

inline napi_status NAPI_CDECL napi_create_symbol(napi_env env,
                                                 napi_value description,
                                                 napi_value* result) {
  return g_vtable->create_symbol(env, description, result);
}

#if NAPI_VERSION >= 9
inline napi_status NAPI_CDECL node_api_symbol_for(napi_env env,
                                                  const char* utf8description,
                                                  size_t length,
                                                  napi_value* result) {
  return g_vtable->symbol_for(env, utf8description, length, result);
}
#endif  // NAPI_VERSION >= 9
inline napi_status NAPI_CDECL napi_create_function(napi_env env,
                                                   const char* utf8name,
                                                   size_t length,
                                                   napi_callback cb,
                                                   void* data,
                                                   napi_value* result) {
  return g_vtable->create_function(env, utf8name, length, cb, data, result);
}

inline napi_status NAPI_CDECL napi_create_error(napi_env env,
                                                napi_value code,
                                                napi_value msg,
                                                napi_value* result) {
  return g_vtable->create_error(env, code, msg, result);
}

inline napi_status NAPI_CDECL napi_create_type_error(napi_env env,
                                                     napi_value code,
                                                     napi_value msg,
                                                     napi_value* result) {
  return g_vtable->create_type_error(env, code, msg, result);
}

inline napi_status NAPI_CDECL napi_create_range_error(napi_env env,
                                                      napi_value code,
                                                      napi_value msg,
                                                      napi_value* result) {
  return g_vtable->create_range_error(env, code, msg, result);
}

#if NAPI_VERSION >= 9
inline napi_status NAPI_CDECL node_api_create_syntax_error(napi_env env,
                                                           napi_value code,
                                                           napi_value msg,
                                                           napi_value* result) {
  return g_vtable->create_syntax_error(env, code, msg, result);
}
#endif  // NAPI_VERSION >= 9

// Methods to get the native napi_value from Primitive type
inline napi_status NAPI_CDECL napi_typeof(napi_env env,
                                          napi_value value,
                                          napi_valuetype* result) {
  return g_vtable->typeof(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_double(napi_env env,
                                                    napi_value value,
                                                    double* result) {
  return g_vtable->get_value_double(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_int32(napi_env env,
                                                   napi_value value,
                                                   int32_t* result) {
  return g_vtable->get_value_int32(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_uint32(napi_env env,
                                                    napi_value value,
                                                    uint32_t* result) {
  return g_vtable->get_value_uint32(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_int64(napi_env env,
                                                   napi_value value,
                                                   int64_t* result) {
  return g_vtable->get_value_int64(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_bool(napi_env env,
                                                  napi_value value,
                                                  bool* result) {
  return g_vtable->get_value_bool(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_value_string_latin1(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result) {
  return g_vtable->get_value_string_latin1(env, value, buf, bufsize, result);
}

inline napi_status NAPI_CDECL napi_get_value_string_utf8(
    napi_env env, napi_value value, char* buf, size_t bufsize, size_t* result) {
  return g_vtable->get_value_string_utf8(env, value, buf, bufsize, result);
}

inline napi_status NAPI_CDECL napi_get_value_string_utf16(napi_env env,
                                                          napi_value value,
                                                          char16_t* buf,
                                                          size_t bufsize,
                                                          size_t* result) {
  return g_vtable->get_value_string_utf16(env, value, buf, bufsize, result);
}

inline napi_status NAPI_CDECL napi_coerce_to_bool(napi_env env,
                                                  napi_value value,
                                                  napi_value* result) {
  return g_vtable->coerce_to_bool(env, value, result);
}

inline napi_status NAPI_CDECL napi_coerce_to_number(napi_env env,
                                                    napi_value value,
                                                    napi_value* result) {
  return g_vtable->coerce_to_number(env, value, result);
}

inline napi_status NAPI_CDECL napi_coerce_to_object(napi_env env,
                                                    napi_value value,
                                                    napi_value* result) {
  return g_vtable->coerce_to_object(env, value, result);
}

inline napi_status NAPI_CDECL napi_coerce_to_string(napi_env env,
                                                    napi_value value,
                                                    napi_value* result) {
  return g_vtable->coerce_to_string(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_prototype(napi_env env,
                                                 napi_value object,
                                                 napi_value* result) {
  return g_vtable->get_prototype(env, object, result);
}

inline napi_status NAPI_CDECL napi_get_property_names(napi_env env,
                                                      napi_value object,
                                                      napi_value* result) {
  return g_vtable->get_property_names(env, object, result);
}

inline napi_status NAPI_CDECL napi_set_property(napi_env env,
                                                napi_value object,
                                                napi_value key,
                                                napi_value value) {
  return g_vtable->set_property(env, object, key, value);
}

inline napi_status NAPI_CDECL napi_has_property(napi_env env,
                                                napi_value object,
                                                napi_value key,
                                                bool* result) {
  return g_vtable->has_property(env, object, key, result);
}

inline napi_status NAPI_CDECL napi_get_property(napi_env env,
                                                napi_value object,
                                                napi_value key,
                                                napi_value* result) {
  return g_vtable->get_property(env, object, key, result);
}

inline napi_status NAPI_CDECL napi_delete_property(napi_env env,
                                                   napi_value object,
                                                   napi_value key,
                                                   bool* result) {
  return g_vtable->delete_property(env, object, key, result);
}

inline napi_status NAPI_CDECL napi_has_own_property(napi_env env,
                                                    napi_value object,
                                                    napi_value key,
                                                    bool* result) {
  return g_vtable->has_own_property(env, object, key, result);
}

inline napi_status NAPI_CDECL napi_set_named_property(napi_env env,
                                                      napi_value object,
                                                      const char* utf8name,
                                                      napi_value value) {
  return g_vtable->set_named_property(env, object, utf8name, value);
}

inline napi_status NAPI_CDECL napi_has_named_property(napi_env env,
                                                      napi_value object,
                                                      const char* utf8name,
                                                      bool* result) {
  return g_vtable->has_named_property(env, object, utf8name, result);
}

inline napi_status NAPI_CDECL napi_get_named_property(napi_env env,
                                                      napi_value object,
                                                      const char* utf8name,
                                                      napi_value* result) {
  return g_vtable->get_named_property(env, object, utf8name, result);
}

inline napi_status NAPI_CDECL napi_set_element(napi_env env,
                                               napi_value object,
                                               uint32_t index,
                                               napi_value value) {
  return g_vtable->set_element(env, object, index, value);
}

inline napi_status NAPI_CDECL napi_has_element(napi_env env,
                                               napi_value object,
                                               uint32_t index,
                                               bool* result) {
  return g_vtable->has_element(env, object, index, result);
}

inline napi_status NAPI_CDECL napi_get_element(napi_env env,
                                               napi_value object,
                                               uint32_t index,
                                               napi_value* result) {
  return g_vtable->get_element(env, object, index, result);
}

inline napi_status NAPI_CDECL napi_delete_element(napi_env env,
                                                  napi_value object,
                                                  uint32_t index,
                                                  bool* result) {
  return g_vtable->delete_element(env, object, index, result);
}

inline napi_status NAPI_CDECL
napi_define_properties(napi_env env,
                       napi_value object,
                       size_t property_count,
                       const napi_property_descriptor* properties) {
  return g_vtable->define_properties(env, object, property_count, properties);
}

inline napi_status NAPI_CDECL napi_is_array(napi_env env,
                                            napi_value value,
                                            bool* result) {
  return g_vtable->is_array(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_array_length(napi_env env,
                                                    napi_value value,
                                                    uint32_t* result) {
  return g_vtable->get_array_length(env, value, result);
}

inline napi_status NAPI_CDECL napi_strict_equals(napi_env env,
                                                 napi_value lhs,
                                                 napi_value rhs,
                                                 bool* result) {
  return g_vtable->strict_equals(env, lhs, rhs, result);
}

inline napi_status NAPI_CDECL napi_call_function(napi_env env,
                                                 napi_value recv,
                                                 napi_value func,
                                                 size_t argc,
                                                 const napi_value* argv,
                                                 napi_value* result) {
  return g_vtable->call_function(env, recv, func, argc, argv, result);
}

inline napi_status NAPI_CDECL napi_new_instance(napi_env env,
                                                napi_value constructor,
                                                size_t argc,
                                                const napi_value* argv,
                                                napi_value* result) {
  return g_vtable->new_instance(env, constructor, argc, argv, result);
}

inline napi_status NAPI_CDECL napi_instanceof(napi_env env,
                                              napi_value object,
                                              napi_value constructor,
                                              bool* result) {
  return g_vtable->instanceof (env, object, constructor, result);
}

inline napi_status NAPI_CDECL napi_get_cb_info(napi_env env,
                                               napi_callback_info cbinfo,
                                               size_t* argc,
                                               napi_value* argv,
                                               napi_value* this_arg,
                                               void** data) {
  return g_vtable->get_cb_info(env, cbinfo, argc, argv, this_arg, data);
}

inline napi_status NAPI_CDECL napi_get_new_target(napi_env env,
                                                  napi_callback_info cbinfo,
                                                  napi_value* result) {
  return g_vtable->get_new_target(env, cbinfo, result);
}

inline napi_status NAPI_CDECL
napi_define_class(napi_env env,
                  const char* utf8name,
                  size_t length,
                  napi_callback constructor,
                  void* data,
                  size_t property_count,
                  const napi_property_descriptor* properties,
                  napi_value* result) {
  return g_vtable->define_class(env,
                                utf8name,
                                length,
                                constructor,
                                data,
                                property_count,
                                properties,
                                result);
}

inline napi_status NAPI_CDECL napi_wrap(napi_env env,
                                        napi_value js_object,
                                        void* native_object,
                                        node_api_basic_finalize finalize_cb,
                                        void* finalize_hint,
                                        napi_ref* result) {
  return g_vtable->wrap(
      env, js_object, native_object, finalize_cb, finalize_hint, result);
}

inline napi_status NAPI_CDECL napi_unwrap(napi_env env,
                                          napi_value js_object,
                                          void** result) {
  return g_vtable->unwrap(env, js_object, result);
}

inline napi_status NAPI_CDECL napi_remove_wrap(napi_env env,
                                               napi_value js_object,
                                               void** result) {
  return g_vtable->remove_wrap(env, js_object, result);
}

inline napi_status NAPI_CDECL
napi_create_external(napi_env env,
                     void* data,
                     node_api_basic_finalize finalize_cb,
                     void* finalize_hint,
                     napi_value* result) {
  return g_vtable->create_external(
      env, data, finalize_cb, finalize_hint, result);
}

inline napi_status NAPI_CDECL napi_get_value_external(napi_env env,
                                                      napi_value value,
                                                      void** result) {
  return g_vtable->get_value_external(env, value, result);
}
inline napi_status NAPI_CDECL napi_create_reference(napi_env env,
                                                    napi_value value,
                                                    uint32_t initial_refcount,
                                                    napi_ref* result) {
  return g_vtable->create_reference(env, value, initial_refcount, result);
}

inline napi_status NAPI_CDECL napi_delete_reference(node_api_basic_env env,
                                                    napi_ref ref) {
  return g_vtable->delete_reference(env, ref);
}

inline napi_status NAPI_CDECL napi_reference_ref(napi_env env,
                                                 napi_ref ref,
                                                 uint32_t* result) {
  return g_vtable->reference_ref(env, ref, result);
}

inline napi_status NAPI_CDECL napi_reference_unref(napi_env env,
                                                   napi_ref ref,
                                                   uint32_t* result) {
  return g_vtable->reference_unref(env, ref, result);
}

inline napi_status NAPI_CDECL napi_get_reference_value(napi_env env,
                                                       napi_ref ref,
                                                       napi_value* result) {
  return g_vtable->get_reference_value(env, ref, result);
}

inline napi_status NAPI_CDECL
napi_open_handle_scope(napi_env env, napi_handle_scope* result) {
  return g_vtable->open_handle_scope(env, result);
}

inline napi_status NAPI_CDECL napi_close_handle_scope(napi_env env,
                                                      napi_handle_scope scope) {
  return g_vtable->close_handle_scope(env, scope);
}

inline napi_status NAPI_CDECL napi_open_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope* result) {
  return g_vtable->open_escapable_handle_scope(env, result);
}

inline napi_status NAPI_CDECL napi_close_escapable_handle_scope(
    napi_env env, napi_escapable_handle_scope scope) {
  return g_vtable->close_escapable_handle_scope(env, scope);
}

inline napi_status NAPI_CDECL
napi_escape_handle(napi_env env,
                   napi_escapable_handle_scope scope,
                   napi_value escapee,
                   napi_value* result) {
  return g_vtable->escape_handle(env, scope, escapee, result);
}

inline napi_status NAPI_CDECL napi_throw(napi_env env, napi_value error) {
  return g_vtable->throw_error(env, error);
}

inline napi_status NAPI_CDECL napi_throw_error(napi_env env,
                                               const char* code,
                                               const char* msg) {
  return g_vtable->throw_js_error(env, code, msg);
}

inline napi_status NAPI_CDECL napi_throw_type_error(napi_env env,
                                                    const char* code,
                                                    const char* msg) {
  return g_vtable->throw_type_error(env, code, msg);
}

inline napi_status NAPI_CDECL napi_throw_range_error(napi_env env,
                                                     const char* code,
                                                     const char* msg) {
  return g_vtable->throw_range_error(env, code, msg);
}

#if NAPI_VERSION >= 9
inline napi_status NAPI_CDECL node_api_throw_syntax_error(napi_env env,
                                                          const char* code,
                                                          const char* msg) {
  return g_vtable->throw_syntax_error(env, code, msg);
}
#endif  // NAPI_VERSION >= 9

inline napi_status NAPI_CDECL napi_is_error(napi_env env,
                                            napi_value value,
                                            bool* result) {
  return g_vtable->is_error(env, value, result);
}

inline napi_status NAPI_CDECL napi_is_exception_pending(napi_env env,
                                                        bool* result) {
  return g_vtable->is_exception_pending(env, result);
}

inline napi_status NAPI_CDECL
napi_get_and_clear_last_exception(napi_env env, napi_value* result) {
  return g_vtable->get_and_clear_last_exception(env, result);
}

inline napi_status NAPI_CDECL napi_is_arraybuffer(napi_env env,
                                                  napi_value value,
                                                  bool* result) {
  return g_vtable->is_arraybuffer(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_arraybuffer(napi_env env,
                                                      size_t byte_length,
                                                      void** data,
                                                      napi_value* result) {
  return g_vtable->create_arraybuffer(env, byte_length, data, result);
}

#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
inline napi_status NAPI_CDECL
napi_create_external_arraybuffer(napi_env env,
                                 void* external_data,
                                 size_t byte_length,
                                 node_api_basic_finalize finalize_cb,
                                 void* finalize_hint,
                                 napi_value* result) {
  return g_vtable->create_external_arraybuffer(
      env, external_data, byte_length, finalize_cb, finalize_hint, result);
}

#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

inline napi_status NAPI_CDECL napi_get_arraybuffer_info(napi_env env,
                                                        napi_value arraybuffer,
                                                        void** data,
                                                        size_t* byte_length) {
  return g_vtable->get_arraybuffer_info(env, arraybuffer, data, byte_length);
}

inline napi_status NAPI_CDECL napi_is_typedarray(napi_env env,
                                                 napi_value value,
                                                 bool* result) {
  return g_vtable->is_typedarray(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_typedarray(napi_env env,
                                                     napi_typedarray_type type,
                                                     size_t length,
                                                     napi_value arraybuffer,
                                                     size_t byte_offset,
                                                     napi_value* result) {
  return g_vtable->create_typedarray(
      env, type, length, arraybuffer, byte_offset, result);
}

inline napi_status NAPI_CDECL
napi_get_typedarray_info(napi_env env,
                         napi_value typedarray,
                         napi_typedarray_type* type,
                         size_t* length,
                         void** data,
                         napi_value* arraybuffer,
                         size_t* byte_offset) {
  return g_vtable->get_typedarray_info(
      env, typedarray, type, length, data, arraybuffer, byte_offset);
}

inline napi_status NAPI_CDECL napi_create_dataview(napi_env env,
                                                   size_t length,
                                                   napi_value arraybuffer,
                                                   size_t byte_offset,
                                                   napi_value* result) {
  return g_vtable->create_dataview(
      env, length, arraybuffer, byte_offset, result);
}

inline napi_status NAPI_CDECL napi_is_dataview(napi_env env,
                                               napi_value value,
                                               bool* result) {
  return g_vtable->is_dataview(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_dataview_info(napi_env env,
                                                     napi_value dataview,
                                                     size_t* bytelength,
                                                     void** data,
                                                     napi_value* arraybuffer,
                                                     size_t* byte_offset) {
  return g_vtable->get_dataview_info(
      env, dataview, bytelength, data, arraybuffer, byte_offset);
}

#ifdef NAPI_EXPERIMENTAL
inline napi_status NAPI_CDECL node_api_is_sharedarraybuffer(napi_env env,
                                                            napi_value value,
                                                            bool* result) {
  return g_vtable->is_sharedarraybuffer(env, value, result);
}

inline napi_status NAPI_CDECL node_api_create_sharedarraybuffer(
    napi_env env, size_t byte_length, void** data, napi_value* result) {
  return g_vtable->create_sharedarraybuffer(env, byte_length, data, result);
}
#endif  // NAPI_EXPERIMENTAL

inline napi_status NAPI_CDECL napi_get_version(node_api_basic_env env,
                                               uint32_t* result) {
  return g_vtable->get_version(env, result);
}

inline napi_status NAPI_CDECL napi_create_promise(napi_env env,
                                                  napi_deferred* deferred,
                                                  napi_value* promise) {
  return g_vtable->create_promise(env, deferred, promise);
}

inline napi_status NAPI_CDECL napi_resolve_deferred(napi_env env,
                                                    napi_deferred deferred,
                                                    napi_value resolution) {
  return g_vtable->resolve_deferred(env, deferred, resolution);
}

inline napi_status NAPI_CDECL napi_reject_deferred(napi_env env,
                                                   napi_deferred deferred,
                                                   napi_value rejection) {
  return g_vtable->reject_deferred(env, deferred, rejection);
}

inline napi_status NAPI_CDECL napi_is_promise(napi_env env,
                                              napi_value value,
                                              bool* is_promise) {
  return g_vtable->is_promise(env, value, is_promise);
}

inline napi_status NAPI_CDECL napi_run_script(napi_env env,
                                              napi_value script,
                                              napi_value* result) {
  return g_vtable->run_script(env, script, result);
}

inline napi_status NAPI_CDECL napi_adjust_external_memory(
    node_api_basic_env env, int64_t change_in_bytes, int64_t* adjusted_value) {
  return g_vtable->adjust_external_memory(env, change_in_bytes, adjusted_value);
}

#if NAPI_VERSION >= 5

inline napi_status NAPI_CDECL napi_create_date(napi_env env,
                                               double time,
                                               napi_value* result) {
  return g_vtable->create_date(env, time, result);
}

inline napi_status NAPI_CDECL napi_is_date(napi_env env,
                                           napi_value value,
                                           bool* is_date) {
  return g_vtable->is_date(env, value, is_date);
}

inline napi_status NAPI_CDECL napi_get_date_value(napi_env env,
                                                  napi_value value,
                                                  double* result) {
  return g_vtable->get_date_value(env, value, result);
}

inline napi_status NAPI_CDECL
napi_add_finalizer(napi_env env,
                   napi_value js_object,
                   void* finalize_data,
                   node_api_basic_finalize finalize_cb,
                   void* finalize_hint,
                   napi_ref* result) {
  return g_vtable->add_finalizer(
      env, js_object, finalize_data, finalize_cb, finalize_hint, result);
}

#endif  // NAPI_VERSION >= 5

#ifdef NAPI_EXPERIMENTAL

inline napi_status NAPI_CDECL node_api_post_finalizer(node_api_basic_env env,
                                                      napi_finalize finalize_cb,
                                                      void* finalize_data,
                                                      void* finalize_hint) {
  return g_vtable->post_finalizer(
      env, finalize_cb, finalize_data, finalize_hint);
}

#endif  // NAPI_EXPERIMENTAL

#if NAPI_VERSION >= 6

inline napi_status NAPI_CDECL napi_create_bigint_int64(napi_env env,
                                                       int64_t value,
                                                       napi_value* result) {
  return g_vtable->create_bigint_int64(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_bigint_uint64(napi_env env,
                                                        uint64_t value,
                                                        napi_value* result) {
  return g_vtable->create_bigint_uint64(env, value, result);
}

inline napi_status NAPI_CDECL napi_create_bigint_words(napi_env env,
                                                       int sign_bit,
                                                       size_t word_count,
                                                       const uint64_t* words,
                                                       napi_value* result) {
  return g_vtable->create_bigint_words(
      env, sign_bit, word_count, words, result);
}

inline napi_status NAPI_CDECL napi_get_value_bigint_int64(napi_env env,
                                                          napi_value value,
                                                          int64_t* result,
                                                          bool* lossless) {
  return g_vtable->get_value_bigint_int64(env, value, result, lossless);
}

inline napi_status NAPI_CDECL napi_get_value_bigint_uint64(napi_env env,
                                                           napi_value value,
                                                           uint64_t* result,
                                                           bool* lossless) {
  return g_vtable->get_value_bigint_uint64(env, value, result, lossless);
}

inline napi_status NAPI_CDECL napi_get_value_bigint_words(napi_env env,
                                                          napi_value value,
                                                          int* sign_bit,
                                                          size_t* word_count,
                                                          uint64_t* words) {
  return g_vtable->get_value_bigint_words(
      env, value, sign_bit, word_count, words);
}

inline napi_status NAPI_CDECL
napi_get_all_property_names(napi_env env,
                            napi_value object,
                            napi_key_collection_mode key_mode,
                            napi_key_filter key_filter,
                            napi_key_conversion key_conversion,
                            napi_value* result) {
  return g_vtable->get_all_property_names(
      env, object, key_mode, key_filter, key_conversion, result);
}

inline napi_status NAPI_CDECL napi_set_instance_data(node_api_basic_env env,
                                                     void* data,
                                                     napi_finalize finalize_cb,
                                                     void* finalize_hint) {
  return g_vtable->set_instance_data(env, data, finalize_cb, finalize_hint);
}

inline napi_status NAPI_CDECL napi_get_instance_data(node_api_basic_env env,
                                                     void** data) {
  return g_vtable->get_instance_data(env, data);
}

#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7

inline napi_status NAPI_CDECL napi_detach_arraybuffer(napi_env env,
                                                      napi_value arraybuffer) {
  return g_vtable->detach_arraybuffer(env, arraybuffer);
}

inline napi_status NAPI_CDECL napi_is_detached_arraybuffer(napi_env env,
                                                           napi_value value,
                                                           bool* result) {
  return g_vtable->is_detached_arraybuffer(env, value, result);
}

#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8

inline napi_status NAPI_CDECL napi_type_tag_object(
    napi_env env, napi_value value, const napi_type_tag* type_tag) {
  return g_vtable->type_tag_object(env, value, type_tag);
}

inline napi_status NAPI_CDECL
napi_check_object_type_tag(napi_env env,
                           napi_value value,
                           const napi_type_tag* type_tag,
                           bool* result) {
  return g_vtable->check_object_type_tag(env, value, type_tag, result);
}

inline napi_status NAPI_CDECL napi_object_freeze(napi_env env,
                                                 napi_value object) {
  return g_vtable->object_freeze(env, object);
}

inline napi_status NAPI_CDECL napi_object_seal(napi_env env,
                                               napi_value object) {
  return g_vtable->object_seal(env, object);
}

#endif  // NAPI_VERSION >= 8

inline void NAPI_CDECL napi_module_register(napi_module* mod) {
  g_vtable->module_register(mod);
}

inline NAPI_NO_RETURN void NAPI_CDECL napi_fatal_error(const char* location,
                                                       size_t location_len,
                                                       const char* message,
                                                       size_t message_len) {
  g_vtable->fatal_error(location, location_len, message, message_len);
}

inline napi_status NAPI_CDECL napi_async_init(napi_env env,
                                              napi_value async_resource,
                                              napi_value async_resource_name,
                                              napi_async_context* result) {
  return g_vtable->async_init(env, async_resource, async_resource_name, result);
}

inline napi_status NAPI_CDECL
napi_async_destroy(napi_env env, napi_async_context async_context) {
  return g_vtable->async_destroy(env, async_context);
}

inline napi_status NAPI_CDECL
napi_make_callback(napi_env env,
                   napi_async_context async_context,
                   napi_value recv,
                   napi_value func,
                   size_t argc,
                   const napi_value* argv,
                   napi_value* result) {
  return g_vtable->make_callback(
      env, async_context, recv, func, argc, argv, result);
}

inline napi_status NAPI_CDECL napi_create_buffer(napi_env env,
                                                 size_t length,
                                                 void** data,
                                                 napi_value* result) {
  return g_vtable->create_buffer(env, length, data, result);
}
#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
inline napi_status NAPI_CDECL
napi_create_external_buffer(napi_env env,
                            size_t length,
                            void* data,
                            node_api_basic_finalize finalize_cb,
                            void* finalize_hint,
                            napi_value* result) {
  return g_vtable->create_external_buffer(
      env, length, data, finalize_cb, finalize_hint, result);
}
#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

#if NAPI_VERSION >= 10

inline napi_status NAPI_CDECL
node_api_create_buffer_from_arraybuffer(napi_env env,
                                        napi_value arraybuffer,
                                        size_t byte_offset,
                                        size_t byte_length,
                                        napi_value* result) {
  return g_vtable->create_buffer_from_arraybuffer(
      env, arraybuffer, byte_offset, byte_length, result);
}
#endif  // NAPI_VERSION >= 10

inline napi_status NAPI_CDECL napi_create_buffer_copy(napi_env env,
                                                      size_t length,
                                                      const void* data,
                                                      void** result_data,
                                                      napi_value* result) {
  return g_vtable->create_buffer_copy(env, length, data, result_data, result);
}

inline napi_status NAPI_CDECL napi_is_buffer(napi_env env,
                                             napi_value value,
                                             bool* result) {
  return g_vtable->is_buffer(env, value, result);
}

inline napi_status NAPI_CDECL napi_get_buffer_info(napi_env env,
                                                   napi_value value,
                                                   void** data,
                                                   size_t* length) {
  return g_vtable->get_buffer_info(env, value, data, length);
}

inline napi_status NAPI_CDECL
napi_create_async_work(napi_env env,
                       napi_value async_resource,
                       napi_value async_resource_name,
                       napi_async_execute_callback execute,
                       napi_async_complete_callback complete,
                       void* data,
                       napi_async_work* result) {
  return g_vtable->create_async_work(env,
                                     async_resource,
                                     async_resource_name,
                                     execute,
                                     complete,
                                     data,
                                     result);
}

inline napi_status NAPI_CDECL napi_delete_async_work(napi_env env,
                                                     napi_async_work work) {
  return g_vtable->delete_async_work(env, work);
}

inline napi_status NAPI_CDECL napi_queue_async_work(node_api_basic_env env,
                                                    napi_async_work work) {
  return g_vtable->queue_async_work(env, work);
}

inline napi_status NAPI_CDECL napi_cancel_async_work(node_api_basic_env env,
                                                     napi_async_work work) {
  return g_vtable->cancel_async_work(env, work);
}

// version management
inline napi_status NAPI_CDECL napi_get_node_version(
    node_api_basic_env env, const napi_node_version** version) {
  return g_vtable->get_node_version(env, version);
}

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
inline napi_status NAPI_CDECL napi_get_uv_event_loop(node_api_basic_env env,
                                                     struct uv_loop_s** loop) {
  return g_vtable->get_uv_event_loop(env, loop);
}

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

inline napi_status NAPI_CDECL napi_fatal_exception(napi_env env,
                                                   napi_value err) {
  return g_vtable->fatal_exception(env, err);
}

inline napi_status NAPI_CDECL napi_add_env_cleanup_hook(node_api_basic_env env,
                                                        napi_cleanup_hook fun,
                                                        void* arg) {
  return g_vtable->add_env_cleanup_hook(env, fun, arg);
}

inline napi_status NAPI_CDECL napi_remove_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg) {
  return g_vtable->remove_env_cleanup_hook(env, fun, arg);
}

inline napi_status NAPI_CDECL
napi_open_callback_scope(napi_env env,
                         napi_value resource_object,
                         napi_async_context context,
                         napi_callback_scope* result) {
  return g_vtable->open_callback_scope(env, resource_object, context, result);
}

inline napi_status NAPI_CDECL
napi_close_callback_scope(napi_env env, napi_callback_scope scope) {
  return g_vtable->close_callback_scope(env, scope);
}

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

// Calling into JS from other threads
inline napi_status NAPI_CDECL
napi_create_threadsafe_function(napi_env env,
                                napi_value func,
                                napi_value async_resource,
                                napi_value async_resource_name,
                                size_t max_queue_size,
                                size_t initial_thread_count,
                                void* thread_finalize_data,
                                napi_finalize thread_finalize_cb,
                                void* context,
                                napi_threadsafe_function_call_js call_js_cb,
                                napi_threadsafe_function* result) {
  return g_vtable->create_threadsafe_function(env,
                                              func,
                                              async_resource,
                                              async_resource_name,
                                              max_queue_size,
                                              initial_thread_count,
                                              thread_finalize_data,
                                              thread_finalize_cb,
                                              context,
                                              call_js_cb,
                                              result);
}

inline napi_status NAPI_CDECL napi_get_threadsafe_function_context(
    napi_threadsafe_function func, void** result) {
  return g_vtable->get_threadsafe_function_context(func, result);
}

inline napi_status NAPI_CDECL
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking) {
  return g_vtable->call_threadsafe_function(func, data, is_blocking);
}

inline napi_status NAPI_CDECL
napi_acquire_threadsafe_function(napi_threadsafe_function func) {
  return g_vtable->acquire_threadsafe_function(func);
}

inline napi_status NAPI_CDECL napi_release_threadsafe_function(
    napi_threadsafe_function func, napi_threadsafe_function_release_mode mode) {
  return g_vtable->release_threadsafe_function(func, mode);
}

inline napi_status NAPI_CDECL napi_unref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func) {
  return g_vtable->unref_threadsafe_function(env, func);
}

inline napi_status NAPI_CDECL napi_ref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func) {
  return g_vtable->ref_threadsafe_function(env, func);
}

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

inline napi_status NAPI_CDECL
napi_add_async_cleanup_hook(node_api_basic_env env,
                            napi_async_cleanup_hook hook,
                            void* arg,
                            napi_async_cleanup_hook_handle* remove_handle) {
  return g_vtable->add_async_cleanup_hook(env, hook, arg, remove_handle);
}

inline napi_status NAPI_CDECL
napi_remove_async_cleanup_hook(napi_async_cleanup_hook_handle remove_handle) {
  return g_vtable->remove_async_cleanup_hook(remove_handle);
}

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

inline napi_status NAPI_CDECL
node_api_get_module_file_name(node_api_basic_env env, const char** result) {
  return g_vtable->get_module_file_name(env, result);
}

#endif  // NAPI_VERSION >= 9

#endif  // BUILDING_NODE_EXTENSION

EXTERN_C_END

#endif  // SRC_NODE_API_H_
