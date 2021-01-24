#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include "js_native_api_types.h"

typedef struct napi_callback_scope__* napi_callback_scope;
typedef struct napi_async_context__* napi_async_context;
typedef struct napi_async_work__* napi_async_work;
#define node_api_callback_scope napi_callback_scope
#define node_api_async_context napi_async_context
#define node_api_async_work napi_async_work
#if NAPI_VERSION >= 4
typedef struct napi_threadsafe_function__* napi_threadsafe_function;
#define node_api_threadsafe_function napi_threadsafe_function
#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 4
typedef enum {
  napi_tsfn_release,
  napi_tsfn_abort
} napi_threadsafe_function_release_mode;
#define node_api_tsfn_release napi_tsfn_release
#define node_api_tsfn_abort napi_tsfn_abort
#define node_api_threadsafe_function_release_mode \
  napi_threadsafe_function_release_mode 
typedef enum {
  napi_tsfn_nonblocking,
  napi_tsfn_blocking
} napi_threadsafe_function_call_mode;
#define node_api_tsfn_nonblocking napi_tsfn_nonblocking
#define node_api_tsfn_blocking napi_tsfn_blocking
#define node_api_threadsafe_function_call_mode \
  napi_threadsafe_function_call_mode
#endif  // NAPI_VERSION >= 4

typedef void (*napi_async_execute_callback)(node_api_env env,
                                            void* data);
#define node_api_async_execute_callback napi_async_execute_callback
typedef void (*napi_async_complete_callback)(node_api_env env,
                                             node_api_status status,
                                             void* data);
#define node_api_async_complete_callback napi_async_complete_callback
#if NAPI_VERSION >= 4
typedef void (*napi_threadsafe_function_call_js)(node_api_env env,
                                                 node_api_value js_callback,
                                                 void* context,
                                                 void* data);
#define node_api_threadsafe_function_call_js napi_threadsafe_function_call_js
#endif  // NAPI_VERSION >= 4

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;
#define node_api_node_version napi_node_version

#ifdef NAPI_EXPERIMENTAL
typedef struct napi_async_cleanup_hook_handle__* napi_async_cleanup_hook_handle;
#define node_api_async_cleanup_hook_handle napi_async_cleanup_hook_handle
typedef void (*napi_async_cleanup_hook)(
    node_api_async_cleanup_hook_handle handle,
    void* data);
#define node_api_async_cleanup_hook napi_async_cleanup_hook
#endif  // NAPI_EXPERIMENTAL

#endif  // SRC_NODE_API_TYPES_H_
