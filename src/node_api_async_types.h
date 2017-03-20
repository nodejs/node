#ifndef SRC_NODE_API_ASYNC_TYPES_H_
#define SRC_NODE_API_ASYNC_TYPES_H_

// LIBUV API types are all opaque pointers for ABI stability
// typedef undefined structs instead of void* for compile time type safety
typedef struct napi_uv_work_t__ *napi_work;

#endif  // SRC_NODE_API_ASYNC_TYPES_H_
