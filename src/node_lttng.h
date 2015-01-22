#ifndef SRC_NODE_LTTNG_H_
#define SRC_NODE_LTTNG_H_

#include "node.h"
#include "v8.h"
#include "env.h"

extern "C" {
typedef struct {
  int32_t fd;
  int32_t port;
  char* remote;
  int32_t buffered;
} node_lttng_connection_t;

typedef struct {
  char* url;
  char* method;
} node_lttng_http_client_request_t;

typedef struct {
  union {
    uint32_t version;
    uintptr_t unused;  /* for compat. with old 64-bit struct */
  } _un;
  char* url;
  char* method;
  char* forwardedFor;
  char* _pad[8];
} node_lttng_http_server_request_t;

}  // extern "C"

namespace node {

void InitLTTNG(Environment* env, v8::Handle<v8::Object> target);

}  // namespace node

#endif  // SRC_NODE_LTTNG_H_
