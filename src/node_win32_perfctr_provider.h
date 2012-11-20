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

#ifndef SRC_WINPERFCTRS_H_
#define SRC_WINPERFCTRS_H_

#if defined(_MSC_VER)
# define INLINE __forceinline
#else
# define INLINE inline
#endif

namespace node {

extern HANDLE NodeCounterProvider;

INLINE bool NODE_COUNTER_ENABLED() { return NodeCounterProvider != NULL; }
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

}

#endif

