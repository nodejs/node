#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include "js_native_api_types.h"

typedef struct napi_callback_scope__* napi_callback_scope;
typedef struct napi_async_context__* napi_async_context;
typedef struct napi_async_work__* napi_async_work;
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

typedef void (*napi_async_execute_callback)(napi_env env,
                                            void* data);
typedef void (*napi_async_complete_callback)(napi_env env,
                                             napi_status status,
                                             void* data);
#if NAPI_VERSION >= 4
typedef void (*napi_threadsafe_function_call_js)(napi_env env,
                                                 napi_value js_callback,
                                                 void* context,
                                                 void* data);
#endif  // NAPI_VERSION >= 4

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;

#ifdef NAPI_EXPERIMENTAL
typedef struct napi_async_cleanup_hook_handle__* napi_async_cleanup_hook_handle;
#endif  // NAPI_EXPERIMENTAL

#endif  // SRC_NODE_API_TYPES_H_
