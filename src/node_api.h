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
#include "js_native_api.h"
#include "node_api_types.h"

struct uv_loop_s;  // Forward declaration.

#ifdef _WIN32
# define NAPI_MODULE_EXPORT __declspec(dllexport)
#else
# define NAPI_MODULE_EXPORT __attribute__((visibility("default")))
#endif

#if defined(__GNUC__)
# define NAPI_NO_RETURN __attribute__((noreturn))
#elif defined(_WIN32)
# define NAPI_NO_RETURN __declspec(noreturn)
#else
# define NAPI_NO_RETURN
#endif

typedef napi_value (*napi_addon_register_func)(napi_env env,
                                               napi_value exports);

typedef struct napi_module {
  int nm_version;
  unsigned int nm_flags;
  const char* nm_filename;
  napi_addon_register_func nm_register_func;
  const char* nm_modname;
  void* nm_priv;
  void* reserved[4];
} napi_module;

#define NAPI_MODULE_VERSION  1

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

#define NAPI_MODULE_X(modname, regfunc, priv, flags)                  \
  EXTERN_C_START                                                      \
    static napi_module _module =                                      \
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
      napi_module_register(&_module);                                 \
    }                                                                 \
  EXTERN_C_END

#define NAPI_MODULE_INITIALIZER_X(base, version)                               \
  NAPI_MODULE_INITIALIZER_X_HELPER(base, version)
#define NAPI_MODULE_INITIALIZER_X_HELPER(base, version) base##version

#ifdef __wasm32__
#define NAPI_WASM_INITIALIZER                                                  \
  NAPI_MODULE_INITIALIZER_X(napi_register_wasm_v, NAPI_MODULE_VERSION)
#define NAPI_MODULE(modname, regfunc)                                          \
  EXTERN_C_START                                                               \
  NAPI_MODULE_EXPORT napi_value NAPI_WASM_INITIALIZER(napi_env env,            \
                                                      napi_value exports) {    \
    return regfunc(env, exports);                                              \
  }                                                                            \
  EXTERN_C_END
#else
#define NAPI_MODULE(modname, regfunc)                                 \
  NAPI_MODULE_X(modname, regfunc, NULL, 0)  // NOLINT (readability/null_usage)
#endif

#define NAPI_MODULE_INITIALIZER_BASE napi_register_module_v

#define NAPI_MODULE_INITIALIZER                                       \
  NAPI_MODULE_INITIALIZER_X(NAPI_MODULE_INITIALIZER_BASE,             \
      NAPI_MODULE_VERSION)

#define NAPI_MODULE_INIT()                                            \
  EXTERN_C_START                                                      \
  NAPI_MODULE_EXPORT napi_value                                       \
  NAPI_MODULE_INITIALIZER(napi_env env, napi_value exports);          \
  EXTERN_C_END                                                        \
  NAPI_MODULE(NODE_GYP_MODULE_NAME, NAPI_MODULE_INITIALIZER)          \
  napi_value NAPI_MODULE_INITIALIZER(napi_env env,                    \
                                     napi_value exports)

EXTERN_C_START

NAPI_EXTERN void napi_module_register(napi_module* mod);

NAPI_EXTERN NAPI_NO_RETURN void napi_fatal_error(const char* location,
                                                 size_t location_len,
                                                 const char* message,
                                                 size_t message_len);

// Methods for custom handling of async operations
NAPI_EXTERN napi_status napi_async_init(napi_env env,
                                        napi_value async_resource,
                                        napi_value async_resource_name,
                                        napi_async_context* result);

NAPI_EXTERN napi_status napi_async_destroy(napi_env env,
                                           napi_async_context async_context);

NAPI_EXTERN napi_status napi_make_callback(napi_env env,
                                           napi_async_context async_context,
                                           napi_value recv,
                                           napi_value func,
                                           size_t argc,
                                           const napi_value* argv,
                                           napi_value* result);

// Methods to provide node::Buffer functionality with napi types
NAPI_EXTERN napi_status napi_create_buffer(napi_env env,
                                           size_t length,
                                           void** data,
                                           napi_value* result);
NAPI_EXTERN napi_status napi_create_external_buffer(napi_env env,
                                                    size_t length,
                                                    void* data,
                                                    napi_finalize finalize_cb,
                                                    void* finalize_hint,
                                                    napi_value* result);
NAPI_EXTERN napi_status napi_create_buffer_copy(napi_env env,
                                                size_t length,
                                                const void* data,
                                                void** result_data,
                                                napi_value* result);
NAPI_EXTERN napi_status napi_is_buffer(napi_env env,
                                       napi_value value,
                                       bool* result);
NAPI_EXTERN napi_status napi_get_buffer_info(napi_env env,
                                             napi_value value,
                                             void** data,
                                             size_t* length);

// Methods to manage simple async operations
NAPI_EXTERN
napi_status napi_create_async_work(napi_env env,
                                   napi_value async_resource,
                                   napi_value async_resource_name,
                                   napi_async_execute_callback execute,
                                   napi_async_complete_callback complete,
                                   void* data,
                                   napi_async_work* result);
NAPI_EXTERN napi_status napi_delete_async_work(napi_env env,
                                               napi_async_work work);
NAPI_EXTERN napi_status napi_queue_async_work(napi_env env,
                                              napi_async_work work);
NAPI_EXTERN napi_status napi_cancel_async_work(napi_env env,
                                               napi_async_work work);

// version management
NAPI_EXTERN
napi_status napi_get_node_version(napi_env env,
                                  const napi_node_version** version);

#if NAPI_VERSION >= 2

// Return the current libuv event loop for a given environment
NAPI_EXTERN napi_status napi_get_uv_event_loop(napi_env env,
                                               struct uv_loop_s** loop);

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

NAPI_EXTERN napi_status napi_fatal_exception(napi_env env, napi_value err);

NAPI_EXTERN napi_status napi_add_env_cleanup_hook(napi_env env,
                                                  void (*fun)(void* arg),
                                                  void* arg);

NAPI_EXTERN napi_status napi_remove_env_cleanup_hook(napi_env env,
                                                     void (*fun)(void* arg),
                                                     void* arg);

NAPI_EXTERN napi_status napi_open_callback_scope(napi_env env,
                                                 napi_value resource_object,
                                                 napi_async_context context,
                                                 napi_callback_scope* result);

NAPI_EXTERN napi_status napi_close_callback_scope(napi_env env,
                                                  napi_callback_scope scope);

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

#ifndef __wasm32__
// Calling into JS from other threads
NAPI_EXTERN napi_status
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

NAPI_EXTERN napi_status
napi_get_threadsafe_function_context(napi_threadsafe_function func,
                                     void** result);

NAPI_EXTERN napi_status
napi_call_threadsafe_function(napi_threadsafe_function func,
                              void* data,
                              napi_threadsafe_function_call_mode is_blocking);

NAPI_EXTERN napi_status
napi_acquire_threadsafe_function(napi_threadsafe_function func);

NAPI_EXTERN napi_status
napi_release_threadsafe_function(napi_threadsafe_function func,
                                 napi_threadsafe_function_release_mode mode);

NAPI_EXTERN napi_status
napi_unref_threadsafe_function(napi_env env, napi_threadsafe_function func);

NAPI_EXTERN napi_status
napi_ref_threadsafe_function(napi_env env, napi_threadsafe_function func);
#endif  // __wasm32__

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

NAPI_EXTERN napi_status napi_add_async_cleanup_hook(
    napi_env env,
    napi_async_cleanup_hook hook,
    void* arg,
    napi_async_cleanup_hook_handle* remove_handle);

NAPI_EXTERN napi_status napi_remove_async_cleanup_hook(
    napi_async_cleanup_hook_handle remove_handle);

#endif  // NAPI_VERSION >= 8

#ifdef NAPI_EXPERIMENTAL

NAPI_EXTERN napi_status
node_api_get_module_file_name(napi_env env, const char** result);

#endif  // NAPI_EXPERIMENTAL

EXTERN_C_END

#endif  // SRC_NODE_API_H_
