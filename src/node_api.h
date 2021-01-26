#ifndef SRC_NODE_API_H_
#define SRC_NODE_API_H_

#ifdef BUILDING_NODE_EXTENSION
  #ifdef _WIN32
    // Building native module against node
    #define NAPI_EXTERN __declspec(dllimport)
  #elif defined(__wasm32__)
    #define NAPI_EXTERN __attribute__((__import_module__("napi")))
  #endif
#endif
#define NODE_API_EXTERN NAPI_EXTERN

#include "js_native_api.h"
#include "node_api_types.h"

struct uv_loop_s;  // Forward declaration.

#ifdef _WIN32
# define NAPI_MODULE_EXPORT __declspec(dllexport)
#else
# define NAPI_MODULE_EXPORT __attribute__((visibility("default")))
#endif
#define NODE_API_MODULE_EXPORT NAPI_MODULE_EXPORT

#if defined(__GNUC__)
# define NAPI_NO_RETURN __attribute__((noreturn))
#elif defined(_WIN32)
# define NAPI_NO_RETURN __declspec(noreturn)
#else
# define NAPI_NO_RETURN
#endif
#define NODE_API_NO_RETURN NAPI_NO_RETURN

typedef node_api_value (*napi_addon_register_func)(node_api_env env,
                                                   node_api_value exports);
#define node_api_addon_register_func napi_addon_register_func

typedef struct napi_module {
  int nm_version;
  unsigned int nm_flags;
  const char* nm_filename;
  node_api_addon_register_func nm_register_func;
  const char* nm_modname;
  void* nm_priv;
  void* reserved[4];
} napi_module;
#define node_api_module napi_module

#define NAPI_MODULE_VERSION  1
#define NODE_API_MODULE_VERSION NAPI_MODULE_VERSION

#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
#define NAPI_C_CTOR(fn)                                                     \
  static void __cdecl fn(void);                                             \
  __declspec(dllexport, allocate(".CRT$XCU")) void(__cdecl * fn##_)(void) = \
      fn;                                                                   \
  static void __cdecl fn(void)
#else
#define NAPI_C_CTOR(fn)                              \
  static void fn(void) __attribute__((constructor)); \
  static void fn(void)
#endif
#define NODE_API_C_CTOR NAPI_C_CTOR

#define NAPI_MODULE_X(modname, regfunc, priv, flags)                  \
  EXTERN_C_START                                                      \
    static node_api_module _module =                                  \
    {                                                                 \
      NAPI_MODULE_VERSION,                                            \
      flags,                                                          \
      __FILE__,                                                       \
      regfunc,                                                        \
      #modname,                                                       \
      priv,                                                           \
      {0},                                                            \
    };                                                                \
    NAPI_C_CTOR(_register_ ## modname) {                              \
      node_api_module_register(&_module);                             \
    }                                                                 \
  EXTERN_C_END
#define NODE_API_MODULE_X NAPI_MODULE_X

#define NAPI_MODULE_INITIALIZER_X(base, version)                               \
  NAPI_MODULE_INITIALIZER_X_HELPER(base, version)
#define NAPI_MODULE_INITIALIZER_X_HELPER(base, version) base##version

#define NODE_API_MODULE_INITIALIZER_X NAPI_MODULE_INITIALIZER_X

#ifdef __wasm32__
#define NAPI_WASM_INITIALIZER                                                  \
  NODE_API_MODULE_INITIALIZER_X(napi_register_wasm_v, NODE_API_MODULE_VERSION)
#define NODE_API_WASM_INITIALIZER NAPI_WASM_INITIALIZER
#define NAPI_MODULE(modname, regfunc)                                          \
  EXTERN_C_START                                                               \
  NODE_API_MODULE_EXPORT node_api_value                                        \
  NODE_API_WASM_INITIALIZER(node_api_env env, node_api_value exports) {        \
    return regfunc(env, exports);                                              \
  }                                                                            \
  EXTERN_C_END
#else
#define NAPI_MODULE(modname, regfunc)                                 \
  NODE_API_MODULE_X(modname, regfunc, NULL, 0)  // NOLINT (readability/null_usage)
#endif
#define NODE_API_MODULE NAPI_MODULE

#define NAPI_MODULE_INITIALIZER_BASE napi_register_module_v
#define NODE_API_MODULE_INITIALIZER_BASE NAPI_MODULE_INITIALIZER_BASE

#define NAPI_MODULE_INITIALIZER                                       \
  NODE_API_MODULE_INITIALIZER_X(NODE_API_MODULE_INITIALIZER_BASE,     \
      NODE_API_MODULE_VERSION)
#define NODE_API_MODULE_INITIALIZER NAPI_MODULE_INITIALIZER

#define NAPI_MODULE_INIT()                                               \
  EXTERN_C_START                                                         \
  NODE_API_MODULE_EXPORT node_api_value                                  \
  NODE_API_MODULE_INITIALIZER(node_api_env env, node_api_value exports); \
  EXTERN_C_END                                                           \
  NODE_API_MODULE(NODE_GYP_MODULE_NAME, NODE_API_MODULE_INITIALIZER)     \
  node_api_value NODE_API_MODULE_INITIALIZER(node_api_env env,           \
                                             node_api_value exports)
#define NODE_API_MODULE_INIT NAPI_MODULE_INIT

EXTERN_C_START

NODE_API_EXTERN void napi_module_register(node_api_module* mod);
#define node_api_module_register napi_module_register

NODE_API_EXTERN NAPI_NO_RETURN void napi_fatal_error(const char* location,
                                                     size_t location_len,
                                                     const char* message,
                                                     size_t message_len);
#define node_api_fatal_error napi_fatal_error

// Methods for custom handling of async operations
NODE_API_EXTERN node_api_status napi_async_init(node_api_env env,
                                             node_api_value async_resource,
                                             node_api_value async_resource_name,
                                             node_api_async_context* result);
#define node_api_async_init napi_async_init

NAPI_EXTERN node_api_status
napi_async_destroy(node_api_env env, node_api_async_context async_context);
#define node_api_async_destroy napi_async_destroy

NODE_API_EXTERN node_api_status
napi_make_callback(node_api_env env,
                   node_api_async_context async_context,
                   node_api_value recv,
                   node_api_value func,
                   size_t argc,
                   const node_api_value* argv,
                   node_api_value* result);
#define node_api_make_callback napi_make_callback

// Methods to provide node::Buffer functionality with node_api types
NODE_API_EXTERN node_api_status napi_create_buffer(node_api_env env,
                                                   size_t length,
                                                   void** data,
                                                   node_api_value* result);
#define node_api_create_buffer napi_create_buffer
NODE_API_EXTERN node_api_status
napi_create_external_buffer(node_api_env env,
                            size_t length,
                            void* data,
                            node_api_finalize finalize_cb,
                            void* finalize_hint,
                            node_api_value* result);
#define node_api_create_external_buffer napi_create_external_buffer
NODE_API_EXTERN node_api_status
napi_create_buffer_copy(node_api_env env,
                        size_t length,
                        const void* data,
                        void** result_data,
                        node_api_value* result);
#define node_api_create_buffer_copy napi_create_buffer_copy
NODE_API_EXTERN node_api_status
napi_is_buffer(node_api_env env,
               node_api_value value,
               bool* result);
#define node_api_is_buffer napi_is_buffer
NODE_API_EXTERN node_api_status
napi_get_buffer_info(node_api_env env,
                     node_api_value value,
                     void** data,
                     size_t* length);
#define node_api_get_buffer_info napi_get_buffer_info

// Methods to manage simple async operations
NODE_API_EXTERN node_api_status
napi_create_async_work(node_api_env env,
                       node_api_value async_resource,
                       node_api_value async_resource_name,
                       node_api_async_execute_callback execute,
                       node_api_async_complete_callback complete,
                       void* data,
                       node_api_async_work* result);
#define node_api_create_async_work napi_create_async_work
NODE_API_EXTERN node_api_status
napi_delete_async_work(node_api_env env, node_api_async_work work);
#define node_api_delete_async_work napi_delete_async_work
NODE_API_EXTERN node_api_status
napi_queue_async_work(node_api_env env, node_api_async_work work);
#define node_api_queue_async_work napi_queue_async_work
NODE_API_EXTERN node_api_status
napi_cancel_async_work(node_api_env env, node_api_async_work work);
#define node_api_cancel_async_work napi_cancel_async_work

// version management
NODE_API_EXTERN node_api_status
napi_get_node_version(node_api_env env, const node_api_node_version** version);
#define node_api_get_node_version napi_get_node_version

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
NODE_API_EXTERN node_api_status
napi_get_uv_event_loop(node_api_env env, struct uv_loop_s** loop);
#define node_api_get_uv_event_loop napi_get_uv_event_loop

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

NODE_API_EXTERN node_api_status
napi_fatal_exception(node_api_env env, node_api_value err);
#define node_api_fatal_exception napi_fatal_exception

NODE_API_EXTERN node_api_status
napi_add_env_cleanup_hook(node_api_env env, void (*fun)(void* arg), void* arg);
#define node_api_add_env_cleanup_hook napi_add_env_cleanup_hook

NODE_API_EXTERN node_api_status
napi_remove_env_cleanup_hook(node_api_env env,
                             void (*fun)(void* arg),
                             void* arg);
#define node_api_remove_env_cleanup_hook napi_remove_env_cleanup_hook

NODE_API_EXTERN node_api_status
napi_open_callback_scope(node_api_env env,
                         node_api_value resource_object,
                         node_api_async_context context,
                         node_api_callback_scope* result);
#define node_api_open_callback_scope napi_open_callback_scope

NODE_API_EXTERN node_api_status
napi_close_callback_scope(node_api_env env, node_api_callback_scope scope);
#define node_api_close_callback_scope napi_close_callback_scope

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

#ifndef __wasm32__
// Calling into JS from other threads
NODE_API_EXTERN node_api_status
napi_create_threadsafe_function(node_api_env env,
                               node_api_value func,
                               node_api_value async_resource,
                               node_api_value async_resource_name,
                               size_t max_queue_size,
                               size_t initial_thread_count,
                               void* thread_finalize_data,
                               node_api_finalize thread_finalize_cb,
                               void* context,
                               node_api_threadsafe_function_call_js call_js_cb,
                               node_api_threadsafe_function* result);
#define node_api_create_threadsafe_function \
  napi_create_threadsafe_function

NODE_API_EXTERN node_api_status
napi_get_threadsafe_function_context(node_api_threadsafe_function func,
                                     void** result);
#define node_api_get_threadsafe_function_context \
  napi_get_threadsafe_function_context

NODE_API_EXTERN node_api_status
napi_call_threadsafe_function(node_api_threadsafe_function func,
                           void* data,
                           node_api_threadsafe_function_call_mode is_blocking);
#define node_api_call_threadsafe_function napi_call_threadsafe_function

NODE_API_EXTERN node_api_status
napi_acquire_threadsafe_function(node_api_threadsafe_function func);
#define node_api_acquire_threadsafe_function napi_acquire_threadsafe_function

NODE_API_EXTERN node_api_status
napi_release_threadsafe_function(node_api_threadsafe_function func,
                               node_api_threadsafe_function_release_mode mode);
#define node_api_release_threadsafe_function napi_release_threadsafe_function

NODE_API_EXTERN node_api_status
napi_unref_threadsafe_function(node_api_env env,
                               node_api_threadsafe_function func);
#define node_api_unref_threadsafe_function napi_unref_threadsafe_function

NODE_API_EXTERN node_api_status
napi_ref_threadsafe_function(node_api_env env,
                             node_api_threadsafe_function func);
#define node_api_ref_threadsafe_function napi_ref_threadsafe_function
#endif  // __wasm32__

#endif  // NAPI_VERSION >= 4

#if defined(NAPI_EXPERIMENTAL) || defined(NODE_API_EXPERIMENTAL)

NODE_API_EXTERN node_api_status napi_add_async_cleanup_hook(
    node_api_env env,
    node_api_async_cleanup_hook hook,
    void* arg,
    node_api_async_cleanup_hook_handle* remove_handle);
#define node_api_add_async_cleanup_hook napi_add_async_cleanup_hook

NODE_API_EXTERN node_api_status napi_remove_async_cleanup_hook(
    node_api_async_cleanup_hook_handle remove_handle);
#define node_api_remove_async_cleanup_hook napi_remove_async_cleanup_hook

#endif  // NAPI_EXPERIMENTAL || NODE_API_EXPERIMENTAL

EXTERN_C_END

#endif  // SRC_NODE_API_H_
