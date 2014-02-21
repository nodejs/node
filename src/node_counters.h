// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_COUNTERS_H_
#define SRC_NODE_COUNTERS_H_

#include "node.h"

#ifdef HAVE_PERFCTR
#include "node_win32_perfctr_provider.h"
#else
#define NODE_COUNTER_ENABLED() (false)
#define NODE_COUNT_HTTP_SERVER_REQUEST()
#define NODE_COUNT_HTTP_SERVER_RESPONSE()
#define NODE_COUNT_HTTP_CLIENT_REQUEST()
#define NODE_COUNT_HTTP_CLIENT_RESPONSE()
#define NODE_COUNT_SERVER_CONN_OPEN()
#define NODE_COUNT_SERVER_CONN_CLOSE()
#define NODE_COUNT_NET_BYTES_SENT(bytes)
#define NODE_COUNT_NET_BYTES_RECV(bytes)
#define NODE_COUNT_GET_GC_RAWTIME()
#define NODE_COUNT_GC_PERCENTTIME()
#define NODE_COUNT_PIPE_BYTES_SENT(bytes)
#define NODE_COUNT_PIPE_BYTES_RECV(bytes)
#endif

#include "v8.h"
#include "env.h"

namespace node {

void InitPerfCounters(Environment* env, v8::Handle<v8::Object> target);
void TermPerfCounters(v8::Handle<v8::Object> target);

}  // namespace node

#endif  // SRC_NODE_COUNTERS_H_
