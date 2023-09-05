#ifndef SRC_NODE_API_EMBEDDING_H_
#define SRC_NODE_API_EMBEDDING_H_

#include "js_native_api.h"
#include "js_native_api_types.h"

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

EXTERN_C_START

typedef void (*napi_error_message_handler)(const char* msg);

NAPI_EXTERN napi_status NAPI_CDECL
napi_create_platform(int argc,
                     char** argv,
                     napi_error_message_handler err_handler,
                     napi_platform* result);

NAPI_EXTERN napi_status NAPI_CDECL
napi_destroy_platform(napi_platform platform);

NAPI_EXTERN napi_status NAPI_CDECL
napi_create_environment(napi_platform platform,
                        napi_error_message_handler err_handler,
                        const char* main_script,
                        int32_t api_version,
                        napi_env* result);

NAPI_EXTERN napi_status NAPI_CDECL napi_run_environment(napi_env env);

NAPI_EXTERN napi_status NAPI_CDECL napi_await_promise(napi_env env,
                                                      napi_value promise,
                                                      napi_value* result);

NAPI_EXTERN napi_status NAPI_CDECL napi_destroy_environment(napi_env env,
                                                            int* exit_code);

EXTERN_C_END

#endif  // SRC_NODE_API_EMBEDDING_H_
