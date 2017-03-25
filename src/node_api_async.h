#ifndef SRC_NODE_API_ASYNC_H_
#define SRC_NODE_API_ASYNC_H_

#include <stdlib.h>
#include "node_api.h"
#include "node_api_async_types.h"

EXTERN_C_START

NAPI_EXTERN napi_work napi_create_async_work();
NAPI_EXTERN void napi_delete_async_work(napi_work w);
NAPI_EXTERN void napi_async_set_data(napi_work w, void* data);
NAPI_EXTERN void napi_async_set_execute(napi_work w, void (*execute)(void*));
NAPI_EXTERN void napi_async_set_complete(napi_work w, void (*complete)(void*));
NAPI_EXTERN void napi_async_set_destroy(napi_work w, void (*destroy)(void*));
NAPI_EXTERN void napi_async_queue_worker(napi_work w);

EXTERN_C_END

#endif  // SRC_NODE_API_ASYNC_H_
