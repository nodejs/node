#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include "js_native_api_types.h"

using napi_callback_scope = struct napi_callback_scope__*;
using napi_async_context = struct napi_async_context__*;
using napi_async_work = struct napi_async_work__*;
#if NAPI_VERSION >= 4
using napi_threadsafe_function = struct napi_threadsafe_function__*;
#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 4
using napi_threadsafe_function_release_mode = enum {
  napi_tsfn_release,
  napi_tsfn_abort
};

using napi_threadsafe_function_call_mode = enum {
  napi_tsfn_nonblocking,
  napi_tsfn_blocking
};
#endif  // NAPI_VERSION >= 4

using napi_async_execute_callback = void (*)(napi_env env,
                                            void* data);
using napi_async_complete_callback = void (*)(napi_env env,
                                             napi_status status,
                                             void* data);
#if NAPI_VERSION >= 4
using napi_threadsafe_function_call_js = void (*)(napi_env env,
                                                 napi_value js_callback,
                                                 void* context,
                                                 void* data);
#endif  // NAPI_VERSION >= 4

using napi_node_version = struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
};

#if NAPI_VERSION >= 8
using napi_async_cleanup_hook_handle = struct napi_async_cleanup_hook_handle__*;
using napi_async_cleanup_hook = void (*)(napi_async_cleanup_hook_handle handle,
                                        void* data);
#endif  // NAPI_VERSION >= 8

#endif  // SRC_NODE_API_TYPES_H_
