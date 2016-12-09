#ifndef SRC_NODE_LTTNG_TP_H_
#define SRC_NODE_LTTNG_TP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER node

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./node_lttng_tp.h"

#if !defined(__NODE_LTTNG_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define __NODE_LTTNG_TP_H

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
  node,
  http_server_request,
  TP_ARGS(
    const char*, url,
    const char*, method,
    const char*, forwardedFor),
  TP_FIELDS(
    ctf_string(url, url)
    ctf_string(method, method)
    ctf_string(forwardedFor, forwardedFor))

TRACEPOINT_EVENT(
  node,
  http_server_response,
  TP_ARGS(
    int, port,
    const char*, remote,
    int, fd),
  TP_FIELDS(
    ctf_integer(int, port, port)
    ctf_string(remote, remote)
    ctf_integer(int, fd, fd))

TRACEPOINT_EVENT(
  node,
  http_client_request,
  TP_ARGS(
    const char*, url,
    const char*, method),
  TP_FIELDS(
    ctf_string(url, url)
    ctf_string(method, method))

TRACEPOINT_EVENT(
  node,
  http_client_response,
  TP_ARGS(
    int, port,
    const char*, remote,
    int, fd),
  TP_FIELDS(
    ctf_integer(int, port, port)
    ctf_string(remote, remote)
    ctf_integer(int, fd, fd))

TRACEPOINT_EVENT(
  node,
  net_server_connection,
  TP_ARGS(
    const char*, remote,
    int, port,
    int, fd,
    int, buffered),
  TP_FIELDS(
    ctf_string(remote, remote)
    ctf_integer(int, port, port)
    ctf_integer(int, fd, fd)
    ctf_integer(int, buffered, buffered))

TRACEPOINT_EVENT(
  node,
  net_stream_end,
  TP_ARGS(
    const char*, remote,
    int, port,
    int, fd),
  TP_FIELDS(
    ctf_string(remote, remote)
    ctf_integer(int, port, port)
    ctf_integer(int, fd, fd))

TRACEPOINT_EVENT(
  node,
  gc_start,
  TP_ARGS(
    const char*, gctype,
    const char*, gcflags),
  TP_FIELDS(
    ctf_string(gctype, gctype)
    ctf_string(gcflags, gcflags))
)

TRACEPOINT_EVENT(
  node,
  gc_done,
  TP_ARGS(
    const char*, gctype,
    const char*, gcflags),
  TP_FIELDS(
    ctf_string(gctype, gctype)
    ctf_string(gcflags, gcflags))

#endif /* __NODE_LTTNG_TP_H */

#include <lttng/tracepoint-event.h>

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_LTTNG_TP_H_
