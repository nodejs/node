#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include "js_native_api_types.h"

struct uv_loop_s;  // Forward declaration.

typedef napi_value(NAPI_CDECL* napi_addon_register_func)(napi_env env,
                                                         napi_value exports);
// False positive: https://github.com/cpplint/cpplint/issues/409
// NOLINTNEXTLINE (readability/casting)
typedef int32_t(NAPI_CDECL* node_api_addon_get_api_version_func)(void);

typedef struct napi_callback_scope__* napi_callback_scope;
typedef struct napi_async_context__* napi_async_context;
typedef struct napi_async_work__* napi_async_work;

#if NAPI_VERSION >= 3
typedef void(NAPI_CDECL* napi_cleanup_hook)(void* arg);
#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4
typedef struct napi_threadsafe_function__* napi_threadsafe_function;
#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 4
typedef enum {
  napi_tsfn_release,
  napi_tsfn_abort
} napi_threadsafe_function_release_mode;

typedef enum {
  napi_tsfn_nonblocking,
  napi_tsfn_blocking
} napi_threadsafe_function_call_mode;
#endif  // NAPI_VERSION >= 4

typedef void(NAPI_CDECL* napi_async_execute_callback)(napi_env env, void* data);
typedef void(NAPI_CDECL* napi_async_complete_callback)(napi_env env,
                                                       napi_status status,
                                                       void* data);
#if NAPI_VERSION >= 4
typedef void(NAPI_CDECL* napi_threadsafe_function_call_js)(
    napi_env env, napi_value js_callback, void* context, void* data);
#endif  // NAPI_VERSION >= 4

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;

#if NAPI_VERSION >= 8
typedef struct napi_async_cleanup_hook_handle__* napi_async_cleanup_hook_handle;
typedef void(NAPI_CDECL* napi_async_cleanup_hook)(
    napi_async_cleanup_hook_handle handle, void* data);
#endif  // NAPI_VERSION >= 8

// Used by deprecated registration method napi_module_register.
typedef struct napi_module {
  int nm_version;
  unsigned int nm_flags;
  const char* nm_filename;
  napi_addon_register_func nm_register_func;
  const char* nm_modname;
  void* nm_priv;
  void* reserved[4];
} napi_module;

#if defined(NODE_API_MODULE_USE_VTABLE) || defined(NODE_API_RUNTIME_USE_VTABLE)

// v-table for Node.js module-specific functions
// New functions must be added at the end to maintain backward compatibility.
typedef struct node_api_module_vtable {
  void(NAPI_CDECL* module_register)(napi_module* mod);

  void(NAPI_CDECL* fatal_error)(const char* location,
                                size_t location_len,
                                const char* message,
                                size_t message_len);

  napi_status(NAPI_CDECL* async_init)(napi_env env,
                                      napi_value async_resource,
                                      napi_value async_resource_name,
                                      napi_async_context* result);

  napi_status(NAPI_CDECL* async_destroy)(napi_env env,
                                         napi_async_context async_context);

  napi_status(NAPI_CDECL* make_callback)(napi_env env,
                                         napi_async_context async_context,
                                         napi_value recv,
                                         napi_value func,
                                         size_t argc,
                                         const napi_value* argv,
                                         napi_value* result);

  napi_status(NAPI_CDECL* create_buffer)(napi_env env,
                                         size_t length,
                                         void** data,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_external_buffer)(
      napi_env env,
      size_t length,
      void* data,
      node_api_basic_finalize finalize_cb,
      void* finalize_hint,
      napi_value* result);

  napi_status(NAPI_CDECL* create_buffer_copy)(napi_env env,
                                              size_t length,
                                              const void* data,
                                              void** result_data,
                                              napi_value* result);
  napi_status(NAPI_CDECL* is_buffer)(napi_env env,
                                     napi_value value,
                                     bool* result);
  napi_status(NAPI_CDECL* get_buffer_info)(napi_env env,
                                           napi_value value,
                                           void** data,
                                           size_t* length);

  napi_status(NAPI_CDECL* create_async_work)(
      napi_env env,
      napi_value async_resource,
      napi_value async_resource_name,
      napi_async_execute_callback execute,
      napi_async_complete_callback complete,
      void* data,
      napi_async_work* result);
  napi_status(NAPI_CDECL* delete_async_work)(napi_env env,
                                             napi_async_work work);
  napi_status(NAPI_CDECL* queue_async_work)(node_api_basic_env env,
                                            napi_async_work work);
  napi_status(NAPI_CDECL* cancel_async_work)(node_api_basic_env env,
                                             napi_async_work work);

  napi_status(NAPI_CDECL* get_node_version)(node_api_basic_env env,
                                            const napi_node_version** version);

#if NAPI_VERSION >= 2

  napi_status(NAPI_CDECL* get_uv_event_loop)(node_api_basic_env env,
                                             struct uv_loop_s** loop);

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

  napi_status(NAPI_CDECL* fatal_exception)(napi_env env, napi_value err);

  napi_status(NAPI_CDECL* add_env_cleanup_hook)(node_api_basic_env env,
                                                napi_cleanup_hook fun,
                                                void* arg);

  napi_status(NAPI_CDECL* remove_env_cleanup_hook)(node_api_basic_env env,
                                                   napi_cleanup_hook fun,
                                                   void* arg);

  napi_status(NAPI_CDECL* open_callback_scope)(napi_env env,
                                               napi_value resource_object,
                                               napi_async_context context,
                                               napi_callback_scope* result);

  napi_status(NAPI_CDECL* close_callback_scope)(napi_env env,
                                                napi_callback_scope scope);

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

  napi_status(NAPI_CDECL* create_threadsafe_function)(
      napi_env env,
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

  napi_status(NAPI_CDECL* get_threadsafe_function_context)(
      napi_threadsafe_function func, void** result);

  napi_status(NAPI_CDECL* call_threadsafe_function)(
      napi_threadsafe_function func,
      void* data,
      napi_threadsafe_function_call_mode is_blocking);

  napi_status(NAPI_CDECL* acquire_threadsafe_function)(
      napi_threadsafe_function func);

  napi_status(NAPI_CDECL* release_threadsafe_function)(
      napi_threadsafe_function func,
      napi_threadsafe_function_release_mode mode);

  napi_status(NAPI_CDECL* unref_threadsafe_function)(
      node_api_basic_env env, napi_threadsafe_function func);

  napi_status(NAPI_CDECL* ref_threadsafe_function)(
      node_api_basic_env env, napi_threadsafe_function func);

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

  napi_status(NAPI_CDECL* add_async_cleanup_hook)(
      node_api_basic_env env,
      napi_async_cleanup_hook hook,
      void* arg,
      napi_async_cleanup_hook_handle* remove_handle);

  napi_status(NAPI_CDECL* remove_async_cleanup_hook)(
      napi_async_cleanup_hook_handle remove_handle);

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

  napi_status(NAPI_CDECL* get_module_file_name)(node_api_basic_env env,
                                                const char** result);

#endif  // NAPI_VERSION >= 9

#if NAPI_VERSION >= 10

  napi_status(NAPI_CDECL* create_buffer_from_arraybuffer)(
      napi_env env,
      napi_value arraybuffer,
      size_t byte_offset,
      size_t byte_length,
      napi_value* result);

#endif  // NAPI_VERSION >= 10
} node_api_module_vtable;

#if NAPI_VERSION >= 4

struct napi_threadsafe_function__ {
  uint64_t sentinel;  // Should be NODE_API_VT_SENTINEL
  const struct node_api_module_vtable* module_vtable;
};

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 8

struct napi_async_cleanup_hook_handle__ {
  uint64_t sentinel;  // Should be NODE_API_VT_SENTINEL
  const struct node_api_module_vtable* module_vtable;
};

#endif  // NAPI_VERSION >= 8

#endif  // defined(NODE_API_MODULE_USE_VTABLE) ||
        // defined(NODE_API_RUNTIME_USE_VTABLE)

#endif  // SRC_NODE_API_TYPES_H_
