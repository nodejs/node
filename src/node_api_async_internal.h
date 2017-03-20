#ifndef SRC_NODE_API_ASYNC_INTERNAL_H_
#define SRC_NODE_API_ASYNC_INTERNAL_H_

#include "node_api_async.h"
#include "uv.h"

typedef struct napi_work_impl__ {
  uv_work_t* work;
  void* data;
  void (*execute)(void* data);
  void (*complete)(void* data);
  void (*destroy)(void* data);
} napi_work_impl;


void napi_async_execute(uv_work_t* req);
void napi_async_complete(uv_work_t* req);


#endif  // SRC_NODE_API_ASYNC_INTERNAL_H_
