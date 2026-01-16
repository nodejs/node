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

#define NAPI_MODULE_INITIALIZER                                                \
  NAPI_MODULE_INITIALIZER_X(NAPI_MODULE_INITIALIZER_BASE, NAPI_MODULE_VERSION)

#define NODE_API_MODULE_GET_API_VERSION                                        \
  NAPI_MODULE_INITIALIZER_X(NODE_API_MODULE_GET_API_VERSION_BASE,              \
                            NAPI_MODULE_VERSION)

#ifdef NODE_API_MODULE_USE_VTABLE_IMPL

#ifndef NODE_API_MODULE_NO_VTABLE_FALLBACK
#define NODE_API_VTABLE_FALLBACK_GLOBALS                                       \
  node_api_js_vtable g_node_api_js_vtable_fallback = {0};                      \
  node_api_module_vtable g_node_api_module_vtable_fallback = {0};
#else
#define NODE_API_VTABLE_FALLBACK_GLOBALS
#endif

#define NODE_API_MODULE_INITIALIZER_IMPL NAPI_MODULE_INITIALIZER##_impl

// NOLINTBEGIN (readability/null_usage) - must be compilable by C compiler
#define NODE_API_MODULE_INITIALIZER_IMPL_EX                                    \
  NODE_API_VTABLE_FALLBACK_GLOBALS                                             \
  const node_api_module_vtable* g_node_api_module_vtable = NULL;               \
  napi_value NODE_API_MODULE_INITIALIZER_IMPL(napi_env env,                    \
                                              napi_value exports);             \
  napi_value NAPI_MODULE_INITIALIZER(napi_env env, napi_value exports) {       \
    if (env && env->sentinel == NODE_API_VT_SENTINEL) {                        \
      g_node_api_module_vtable = env->module_vtable;                           \
    }                                                                          \
    return NODE_API_MODULE_INITIALIZER_IMPL(env, exports);                     \
  }
// NOLINTEND (readability/null_usage)

#else  // NODE_API_MODULE_USE_VTABLE_IMPL
#define NODE_API_MODULE_INITIALIZER_IMPL_EX
#define NODE_API_MODULE_INITIALIZER_IMPL NAPI_MODULE_INITIALIZER
#endif  // NODE_API_MODULE_USE_VTABLE_IMPL

#define NAPI_MODULE_INIT()                                                     \
  EXTERN_C_START                                                               \
  NAPI_MODULE_EXPORT int32_t NODE_API_MODULE_GET_API_VERSION(void) {           \
    return NAPI_VERSION;                                                       \
  }                                                                            \
  NAPI_MODULE_EXPORT napi_value NAPI_MODULE_INITIALIZER(napi_env env,          \
                                                        napi_value exports);   \
  EXTERN_C_END                                                                 \
  NODE_API_MODULE_INITIALIZER_IMPL_EX                                          \
  napi_value NODE_API_MODULE_INITIALIZER_IMPL(napi_env env, napi_value exports)

#define NAPI_MODULE(modname, regfunc)                                          \
  NAPI_MODULE_INIT() { return regfunc(env, exports); }

// Deprecated. Use NAPI_MODULE.
#define NAPI_MODULE_X(modname, regfunc, priv, flags)                           \
  NAPI_MODULE(modname, regfunc)

EXTERN_C_START

#ifdef NODE_API_MODULE_USE_VTABLE_IMPL

extern const node_api_module_vtable* g_node_api_module_vtable;

#ifndef NODE_API_MODULE_NO_VTABLE_FALLBACK
extern node_api_module_vtable g_node_api_module_vtable_fallback;
#endif  // NODE_API_MODULE_NO_VTABLE_FALLBACK

#define NODE_API_GLOBAL_MODULE_VTABLE_IMPL(func_name, method_name, ...)        \
  {                                                                            \
    NODE_API_VTABLE_IMPL_FALLBACK(                                             \
        , module_vtable, func_name, method_name, __VA_ARGS__);                 \
  }

#define NODE_API_MODULE_VTABLE_IMPL(func_name, method_name, obj, ...)          \
  NODE_API_VTABLE_IMPL_BASE(                                                   \
      module_vtable, func_name, method_name, obj, obj, __VA_ARGS__)

#define NODE_API_MODULE_VTABLE_IMPL_NOARGS(func_name, method_name, obj)        \
  NODE_API_VTABLE_IMPL_BASE(module_vtable, func_name, method_name, obj, obj)

#else  // NODE_API_MODULE_USE_VTABLE_IMPL

#define NODE_API_GLOBAL_MODULE_VTABLE_IMPL(...)
#define NODE_API_MODULE_VTABLE_IMPL(...)
#define NODE_API_MODULE_VTABLE_IMPL_NOARGS(...)

#endif  // NODE_API_MODULE_USE_VTABLE_IMPL

// Deprecated. Replaced by symbol-based registration defined by NAPI_MODULE
// and NAPI_MODULE_INIT macros.
NAPI_EXTERN void NAPI_CDECL napi_module_register(napi_module* mod)
    NODE_API_GLOBAL_MODULE_VTABLE_IMPL(napi_module_register,
                                       module_register,
                                       mod);

NAPI_EXTERN void NAPI_NO_RETURN NAPI_CDECL napi_fatal_error(
    const char* location,
    size_t location_len,
    const char* message,
    size_t message_len) NODE_API_GLOBAL_MODULE_VTABLE_IMPL(napi_fatal_error,
                                                           fatal_error,
                                                           location,
                                                           location_len,
                                                           message,
                                                           message_len);

// Methods for custom handling of async operations
NAPI_EXTERN napi_status NAPI_CDECL napi_async_init(
    napi_env env,
    napi_value async_resource,
    napi_value async_resource_name,
    napi_async_context* result) NODE_API_MODULE_VTABLE_IMPL(napi_async_init,
                                                            async_init,
                                                            env,
                                                            async_resource,
                                                            async_resource_name,
                                                            result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_async_destroy(napi_env env, napi_async_context async_context)
    NODE_API_MODULE_VTABLE_IMPL(napi_async_destroy,
                                async_destroy,
                                env,
                                async_context);

NAPI_EXTERN napi_status NAPI_CDECL napi_make_callback(
    napi_env env,
    napi_async_context async_context,
    napi_value recv,
    napi_value func,
    size_t argc,
    const napi_value* argv,
    napi_value* result) NODE_API_MODULE_VTABLE_IMPL(napi_make_callback,
                                                    make_callback,
                                                    env,
                                                    async_context,
                                                    recv,
                                                    func,
                                                    argc,
                                                    argv,
                                                    result);

// Methods to provide node::Buffer functionality with napi types
NAPI_EXTERN napi_status NAPI_CDECL
napi_create_buffer(napi_env env, size_t length, void** data, napi_value* result)
    NODE_API_MODULE_VTABLE_IMPL(
        napi_create_buffer, create_buffer, env, length, data, result);

#ifndef NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
NAPI_EXTERN napi_status NAPI_CDECL napi_create_external_buffer(
    napi_env env,
    size_t length,
    void* data,
    node_api_basic_finalize finalize_cb,
    void* finalize_hint,
    napi_value* result) NODE_API_MODULE_VTABLE_IMPL(napi_create_external_buffer,
                                                    create_external_buffer,
                                                    env,
                                                    length,
                                                    data,
                                                    finalize_cb,
                                                    finalize_hint,
                                                    result);
#endif  // NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED

#if NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL
node_api_create_buffer_from_arraybuffer(napi_env env,
                                        napi_value arraybuffer,
                                        size_t byte_offset,
                                        size_t byte_length,
                                        napi_value* result)
    NODE_API_MODULE_VTABLE_IMPL(node_api_create_buffer_from_arraybuffer,
                                create_buffer_from_arraybuffer,
                                env,
                                arraybuffer,
                                byte_offset,
                                byte_length,
                                result);
#endif  // NAPI_VERSION >= 10

NAPI_EXTERN napi_status NAPI_CDECL napi_create_buffer_copy(napi_env env,
                                                           size_t length,
                                                           const void* data,
                                                           void** result_data,
                                                           napi_value* result)
    NODE_API_MODULE_VTABLE_IMPL(napi_create_buffer_copy,
                                create_buffer_copy,
                                env,
                                length,
                                data,
                                result_data,
                                result);

NAPI_EXTERN napi_status NAPI_CDECL napi_is_buffer(napi_env env,
                                                  napi_value value,
                                                  bool* result)
    NODE_API_MODULE_VTABLE_IMPL(napi_is_buffer, is_buffer, env, value, result);

NAPI_EXTERN napi_status NAPI_CDECL napi_get_buffer_info(napi_env env,
                                                        napi_value value,
                                                        void** data,
                                                        size_t* length)
    NODE_API_MODULE_VTABLE_IMPL(
        napi_get_buffer_info, get_buffer_info, env, value, data, length);

// Methods to manage simple async operations
NAPI_EXTERN napi_status NAPI_CDECL napi_create_async_work(
    napi_env env,
    napi_value async_resource,
    napi_value async_resource_name,
    napi_async_execute_callback execute,
    napi_async_complete_callback complete,
    void* data,
    napi_async_work* result) NODE_API_MODULE_VTABLE_IMPL(napi_create_async_work,
                                                         create_async_work,
                                                         env,
                                                         async_resource,
                                                         async_resource_name,
                                                         execute,
                                                         complete,
                                                         data,
                                                         result);

NAPI_EXTERN napi_status NAPI_CDECL napi_delete_async_work(napi_env env,
                                                          napi_async_work work)
    NODE_API_MODULE_VTABLE_IMPL(napi_delete_async_work,
                                delete_async_work,
                                env,
                                work);

NAPI_EXTERN napi_status NAPI_CDECL napi_queue_async_work(node_api_basic_env env,
                                                         napi_async_work work)
    NODE_API_MODULE_VTABLE_IMPL(napi_queue_async_work,
                                queue_async_work,
                                env,
                                work);

NAPI_EXTERN napi_status NAPI_CDECL
napi_cancel_async_work(node_api_basic_env env, napi_async_work work)
    NODE_API_MODULE_VTABLE_IMPL(napi_cancel_async_work,
                                cancel_async_work,
                                env,
                                work);

// version management
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_node_version(node_api_basic_env env, const napi_node_version** version)
    NODE_API_MODULE_VTABLE_IMPL(napi_get_node_version,
                                get_node_version,
                                env,
                                version);

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
NAPI_EXTERN napi_status NAPI_CDECL
napi_get_uv_event_loop(node_api_basic_env env, struct uv_loop_s** loop)
    NODE_API_MODULE_VTABLE_IMPL(napi_get_uv_event_loop,
                                get_uv_event_loop,
                                env,
                                loop);

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

NAPI_EXTERN napi_status NAPI_CDECL napi_fatal_exception(napi_env env,
                                                        napi_value err)
    NODE_API_MODULE_VTABLE_IMPL(napi_fatal_exception,
                                fatal_exception,
                                env,
                                err);

NAPI_EXTERN napi_status NAPI_CDECL napi_add_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg)
    NODE_API_MODULE_VTABLE_IMPL(
        napi_add_env_cleanup_hook, add_env_cleanup_hook, env, fun, arg);

NAPI_EXTERN napi_status NAPI_CDECL napi_remove_env_cleanup_hook(
    node_api_basic_env env, napi_cleanup_hook fun, void* arg)
    NODE_API_MODULE_VTABLE_IMPL(
        napi_remove_env_cleanup_hook, remove_env_cleanup_hook, env, fun, arg);

NAPI_EXTERN napi_status NAPI_CDECL
napi_open_callback_scope(napi_env env,
                         napi_value resource_object,
                         napi_async_context context,
                         napi_callback_scope* result)
    NODE_API_MODULE_VTABLE_IMPL(napi_open_callback_scope,
                                open_callback_scope,
                                env,
                                resource_object,
                                context,
                                result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_close_callback_scope(napi_env env, napi_callback_scope scope)
    NODE_API_MODULE_VTABLE_IMPL(napi_close_callback_scope,
                                close_callback_scope,
                                env,
                                scope);

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
                                napi_threadsafe_function* result)
    NODE_API_MODULE_VTABLE_IMPL(napi_create_threadsafe_function,
                                create_threadsafe_function,
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

NAPI_EXTERN napi_status NAPI_CDECL napi_get_threadsafe_function_context(
    napi_threadsafe_function func, void** result)
    NODE_API_MODULE_VTABLE_IMPL(napi_get_threadsafe_function_context,
                                get_threadsafe_function_context,
                                func,
                                result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking)
    NODE_API_MODULE_VTABLE_IMPL(napi_call_threadsafe_function,
                                call_threadsafe_function,
                                func,
                                data,
                                is_blocking);

NAPI_EXTERN napi_status NAPI_CDECL
napi_acquire_threadsafe_function(napi_threadsafe_function func)
    NODE_API_MODULE_VTABLE_IMPL_NOARGS(napi_acquire_threadsafe_function,
                                       acquire_threadsafe_function,
                                       func);

NAPI_EXTERN napi_status NAPI_CDECL napi_release_threadsafe_function(
    napi_threadsafe_function func, napi_threadsafe_function_release_mode mode)
    NODE_API_MODULE_VTABLE_IMPL(napi_release_threadsafe_function,
                                release_threadsafe_function,
                                func,
                                mode);

NAPI_EXTERN napi_status NAPI_CDECL napi_unref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func)
    NODE_API_MODULE_VTABLE_IMPL(napi_unref_threadsafe_function,
                                unref_threadsafe_function,
                                env,
                                func);

NAPI_EXTERN napi_status NAPI_CDECL napi_ref_threadsafe_function(
    node_api_basic_env env, napi_threadsafe_function func)
    NODE_API_MODULE_VTABLE_IMPL(napi_ref_threadsafe_function,
                                ref_threadsafe_function,
                                env,
                                func);

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

NAPI_EXTERN napi_status NAPI_CDECL
napi_add_async_cleanup_hook(node_api_basic_env env,
                            napi_async_cleanup_hook hook,
                            void* arg,
                            napi_async_cleanup_hook_handle* remove_handle)
    NODE_API_MODULE_VTABLE_IMPL(napi_add_async_cleanup_hook,
                                add_async_cleanup_hook,
                                env,
                                hook,
                                arg,
                                remove_handle);

NAPI_EXTERN napi_status NAPI_CDECL
napi_remove_async_cleanup_hook(napi_async_cleanup_hook_handle remove_handle)
    NODE_API_MODULE_VTABLE_IMPL_NOARGS(napi_remove_async_cleanup_hook,
                                       remove_async_cleanup_hook,
                                       remove_handle);

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

NAPI_EXTERN napi_status NAPI_CDECL
node_api_get_module_file_name(node_api_basic_env env, const char** result)
    NODE_API_MODULE_VTABLE_IMPL(node_api_get_module_file_name,
                                get_module_file_name,
                                env,
                                result);

#endif  // NAPI_VERSION >= 9

EXTERN_C_END

#endif  // SRC_NODE_API_H_
