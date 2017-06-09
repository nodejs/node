#ifndef SRC_NODE_LTTNG_H_
#define SRC_NODE_LTTNG_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "v8.h"
#include "env.h"

extern "C" {
typedef struct {
  int32_t fd;
  int32_t port;
  const char* remote;
  int32_t buffered;
} node_lttng_connection_t;

typedef struct {
  const char* url;
  const char* method;
} node_lttng_http_client_request_t;

typedef struct {
  union {
    uint32_t version;
    uintptr_t unused;  /* for compat. with old 64-bit struct */
  } _un;
  const char* url;
  const char* method;
  const char* forwarded_for;
  const char* _pad[8];
} node_lttng_http_server_request_t;

}  // extern "C"

namespace node {

void InitLTTNG(Environment* env, v8::Local<v8::Object> target);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_LTTNG_H_
