#ifndef SRC_NODE_COUNTERS_H_
#define SRC_NODE_COUNTERS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"

#ifdef HAVE_PERFCTR
#include "node_win32_perfctr_provider.h"
#else
#define NODE_COUNTER_ENABLED() (false)
#define NODE_COUNT_GC_PERCENTTIME(percent) do { } while (false)
#define NODE_COUNT_GET_GC_RAWTIME() do { } while (false)
#define NODE_COUNT_HTTP_CLIENT_REQUEST() do { } while (false)
#define NODE_COUNT_HTTP_CLIENT_RESPONSE() do { } while (false)
#define NODE_COUNT_HTTP_SERVER_REQUEST() do { } while (false)
#define NODE_COUNT_HTTP_SERVER_RESPONSE() do { } while (false)
#define NODE_COUNT_NET_BYTES_RECV(bytes) do { } while (false)
#define NODE_COUNT_NET_BYTES_SENT(bytes) do { } while (false)
#define NODE_COUNT_PIPE_BYTES_RECV(bytes) do { } while (false)
#define NODE_COUNT_PIPE_BYTES_SENT(bytes) do { } while (false)
#define NODE_COUNT_SERVER_CONN_CLOSE() do { } while (false)
#define NODE_COUNT_SERVER_CONN_OPEN() do { } while (false)
#endif

#include "v8.h"
#include "env.h"

namespace node {

void InitPerfCounters(Environment* env, v8::Local<v8::Object> target);
void TermPerfCounters(v8::Local<v8::Object> target);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_COUNTERS_H_
