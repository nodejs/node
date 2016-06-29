#ifndef SRC_NODE_WIN32_PERFCTR_PROVIDER_H_
#define SRC_NODE_WIN32_PERFCTR_PROVIDER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if defined(_MSC_VER)
# define INLINE __forceinline
#else
# define INLINE inline
#endif

namespace node {

extern HANDLE NodeCounterProvider;

INLINE bool NODE_COUNTER_ENABLED() { return NodeCounterProvider != nullptr; }
void NODE_COUNT_HTTP_SERVER_REQUEST();
void NODE_COUNT_HTTP_SERVER_RESPONSE();
void NODE_COUNT_HTTP_CLIENT_REQUEST();
void NODE_COUNT_HTTP_CLIENT_RESPONSE();
void NODE_COUNT_SERVER_CONN_OPEN();
void NODE_COUNT_SERVER_CONN_CLOSE();
void NODE_COUNT_NET_BYTES_SENT(int bytes);
void NODE_COUNT_NET_BYTES_RECV(int bytes);
uint64_t NODE_COUNT_GET_GC_RAWTIME();
void NODE_COUNT_GC_PERCENTTIME(unsigned int percent);
void NODE_COUNT_PIPE_BYTES_SENT(int bytes);
void NODE_COUNT_PIPE_BYTES_RECV(int bytes);

void InitPerfCountersWin32();
void TermPerfCountersWin32();

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WIN32_PERFCTR_PROVIDER_H_
