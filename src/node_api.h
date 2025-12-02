#ifndef SRC_NODE_API_H_
#define SRC_NODE_API_H_

#if defined(BUILDING_NODE_EXTENSION) && !defined(NAPI_EXTERN)
#ifdef _WIN32
// Building native addon against node
#define NAPI_EXTERN __declspec(dllimport)
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

#ifdef NODE_API_MODULE_USE_VTABLE
#define NODE_API_MODULE_SET_VTABLE_DEFINITION                                  \
  const node_api_module_vtable* g_node_api_module_vtable =                     \
      NULL; /* NOLINT(readability/null_usage) */                               \
  const node_api_js_native_vtable* g_node_api_js_native_vtable =               \
      NULL; /* NOLINT(readability/null_usage) */                               \
  NAPI_MODULE_EXPORT void NODE_API_MODULE_SET_VTABLE(                          \
      const node_api_module_vtable* module_vtable,                             \
      const node_api_js_native_vtable* js_native_vtable) {                     \
    g_node_api_module_vtable = module_vtable;                                  \
    g_node_api_js_native_vtable = js_native_vtable;                            \
  }
#else
#define NODE_API_MODULE_SET_VTABLE_DEFINITION /* No-op */
#endif

#define NAPI_MODULE_INIT()                                                     \
  EXTERN_C_START                                                               \
  NAPI_MODULE_EXPORT int32_t NODE_API_MODULE_GET_API_VERSION(void) {           \
    return NAPI_VERSION;                                                       \
  }                                                                            \
  NODE_API_MODULE_SET_VTABLE_DEFINITION                                        \
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

#ifndef NODE_API_MODULE_USE_VTABLE

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

#else  // NODE_API_MODULE_USE_VTABLE

extern const node_api_module_vtable* g_node_api_module_vtable;

static inline void NAPI_CDECL napi_module_register(napi_module* mod) {
  g_node_api_module_vtable->module_register(mod);
}

static inline NAPI_NO_RETURN void NAPI_CDECL
napi_fatal_error(const char* location,
                 size_t location_len,
                 const char* message,
                 size_t message_len) {
  g_node_api_module_vtable->fatal_error(
      location, location_len, message, message_len);
}

static inline napi_status NAPI_CDECL
napi_async_init(napi_env env,
                napi_value async_resource,
                napi_value async_resource_name,
                napi_async_context* result) {
  return g_node_api_module_vtable->async_init(
      env, async_resource, async_resource_name, result);
}

static inline napi_status NAPI_CDECL
napi_async_destroy(napi_env env, napi_async_context async_context) {
  return g_node_api_module_vtable->async_destroy(env, async_context);
}

static inline napi_status NAPI_CDECL
napi_make_callback(napi_env env,
                   napi_async_context async_context,
                   napi_value recv,
                   napi_value func,
                   size_t argc,
                   const napi_value* argv,
                   napi_value* result) {
  return g_node_api_module_vtable->make_callback(
      env, async_context, recv, func, argc, argv, result);
}

static inline napi_status NAPI_CDECL napi_create_buffer(napi_env env,
                                                        size_t length,
                                                        void** data,
                                                        napi_value* result) {
  return g_node_api_module_vtable->create_buffer(env, length, data, result);
}
#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
static inline napi_status NAPI_CDECL
napi_create_external_buffer(napi_env env,
                            size_t length,
                            void* data,
                            node_api_basic_finalize finalize_cb,
                            void* finalize_hint,
                            napi_value* result) {
  return g_node_api_module_vtable->create_external_buffer(
      env, length, data, finalize_cb, finalize_hint, result);
}
#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

#if NAPI_VERSION >= 10

static inline napi_status NAPI_CDECL
node_api_create_buffer_from_arraybuffer(napi_env env,
                                        napi_value arraybuffer,
                                        size_t byte_offset,
                                        size_t byte_length,
                                        napi_value* result) {
  return g_node_api_module_vtable->create_buffer_from_arraybuffer(
      env, arraybuffer, byte_offset, byte_length, result);
}
#endif  // NAPI_VERSION >= 10

static inline napi_status NAPI_CDECL
napi_create_buffer_copy(napi_env env,
                        size_t length,
                        const void* data,
                        void** result_data,
                        napi_value* result) {
  return g_node_api_module_vtable->create_buffer_copy(
      env, length, data, result_data, result);
}

static inline napi_status NAPI_CDECL napi_is_buffer(napi_env env,
                                                    napi_value value,
                                                    bool* result) {
  return g_node_api_module_vtable->is_buffer(env, value, result);
}

static inline napi_status NAPI_CDECL napi_get_buffer_info(napi_env env,
                                                          napi_value value,
                                                          void** data,
                                                          size_t* length) {
  return g_node_api_module_vtable->get_buffer_info(env, value, data, length);
}

static inline napi_status NAPI_CDECL
napi_create_async_work(napi_env env,
                       napi_value async_resource,
                       napi_value async_resource_name,
                       napi_async_execute_callback execute,
                       napi_async_complete_callback complete,
                       void* data,
                       napi_async_work* result) {
  return g_node_api_module_vtable->create_async_work(env,
                                                     async_resource,
                                                     async_resource_name,
                                                     execute,
                                                     complete,
                                                     data,
                                                     result);
}

static inline napi_status NAPI_CDECL
napi_delete_async_work(napi_env env, napi_async_work work) {
  return g_node_api_module_vtable->delete_async_work(env, work);
}

static inline napi_status NAPI_CDECL
napi_queue_async_work(node_api_basic_env env, napi_async_work work) {
  return g_node_api_module_vtable->queue_async_work(env, work);
}

static inline napi_status NAPI_CDECL
napi_cancel_async_work(node_api_basic_env env, napi_async_work work) {
  return g_node_api_module_vtable->cancel_async_work(env, work);
}

// version management
static inline napi_status NAPI_CDECL napi_get_node_version(
    node_api_basic_env env, const napi_node_version** version) {
  return g_node_api_module_vtable->get_node_version(env, version);
}

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
static inline napi_status NAPI_CDECL
napi_get_uv_event_loop(node_api_basic_env env, struct uv_loop_s** loop) {
  return g_node_api_module_vtable->get_uv_event_loop(env, loop);
}

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

static inline napi_status NAPI_CDECL napi_fatal_exception(napi_env env,
                                                          napi_value err) {
  return g_node_api_module_vtable->fatal_exception(env, err);
}

static inline napi_status NAPI_CDECL napi_add_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg) {
  return g_node_api_module_vtable->add_env_cleanup_hook(env, fun, arg);
}

static inline napi_status NAPI_CDECL napi_remove_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg) {
  return g_node_api_module_vtable->remove_env_cleanup_hook(env, fun, arg);
}

static inline napi_status NAPI_CDECL
napi_open_callback_scope(napi_env env,
                         napi_value resource_object,
                         napi_async_context context,
                         napi_callback_scope* result) {
  return g_node_api_module_vtable->open_callback_scope(
      env, resource_object, context, result);
}

static inline napi_status NAPI_CDECL
napi_close_callback_scope(napi_env env, napi_callback_scope scope) {
  return g_node_api_module_vtable->close_callback_scope(env, scope);
}

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

// Calling into JS from other threads
static inline napi_status NAPI_CDECL
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
  return g_node_api_module_vtable->create_threadsafe_function(
      env,
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

static inline napi_status NAPI_CDECL napi_get_threadsafe_function_context(
    napi_threadsafe_function func, void** result) {
  return g_node_api_module_vtable->get_threadsafe_function_context(func,
                                                                   result);
}

static inline napi_status NAPI_CDECL
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking) {
  return g_node_api_module_vtable->call_threadsafe_function(
      func, data, is_blocking);
}

static inline napi_status NAPI_CDECL
napi_acquire_threadsafe_function(napi_threadsafe_function func) {
  return g_node_api_module_vtable->acquire_threadsafe_function(func);
}

static inline napi_status NAPI_CDECL napi_release_threadsafe_function(
    napi_threadsafe_function func, napi_threadsafe_function_release_mode mode) {
  return g_node_api_module_vtable->release_threadsafe_function(func, mode);
}

static inline napi_status NAPI_CDECL napi_unref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func) {
  return g_node_api_module_vtable->unref_threadsafe_function(env, func);
}

static inline napi_status NAPI_CDECL napi_ref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func) {
  return g_node_api_module_vtable->ref_threadsafe_function(env, func);
}

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

static inline napi_status NAPI_CDECL
napi_add_async_cleanup_hook(node_api_basic_env env,
                            napi_async_cleanup_hook hook,
                            void* arg,
                            napi_async_cleanup_hook_handle* remove_handle) {
  return g_node_api_module_vtable->add_async_cleanup_hook(
      env, hook, arg, remove_handle);
}

static inline napi_status NAPI_CDECL
napi_remove_async_cleanup_hook(napi_async_cleanup_hook_handle remove_handle) {
  return g_node_api_module_vtable->remove_async_cleanup_hook(remove_handle);
}

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

static inline napi_status NAPI_CDECL
node_api_get_module_file_name(node_api_basic_env env, const char** result) {
  return g_node_api_module_vtable->get_module_file_name(env, result);
}

#endif  // NAPI_VERSION >= 9

#endif  // NODE_API_MODULE_USE_VTABLE

EXTERN_C_END

#endif  // SRC_NODE_API_H_
