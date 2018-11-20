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

#ifndef SRC_NODE_DTRACE_H_
#define SRC_NODE_DTRACE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_internals.h"

extern "C" {
/*
 * The following structures are passed directly to DTrace when probes are fired.
 * Translators in node.d translate these structures into the corresponding D
 * structures, taking care of dealing with the user process data model (32-bit
 * or 64-bit) and structure versions (see node_dtrace_http_server_request_t
 * below).
 */

typedef struct {
  int32_t fd;
  int32_t port;
  char* remote;
  int32_t buffered;
} node_dtrace_connection_t;

typedef struct {
  char* url;
  char* method;
} node_dtrace_http_client_request_t;

/*
 * The original version of this structure contained only a url and method, just
 * like the client request above.  To add the new forwardedFor field, the
 * structure layout was changed to begin with an integer version.  The
 * translator knows whether it's looking at an old- or new-version structure
 * based on whether the version field's value is a reasonable pointer (i.e.
 * address greater than 4K).  No doubt this is filthy, but there's not much else
 * we can do, and it works reliably.
 *
 * This version of the structure also contains padding that should be zeroed out
 * by the consumer so that future versions of the translator can simply check if
 * a field is present by checking it against nullptr.
 */
typedef struct {
  union {
    uint32_t version;
    uintptr_t unused;  /* for compat. with old 64-bit struct */
  } _un;
  char* url;
  char* method;
  char* forwardedFor;
  char* _pad[8];
} node_dtrace_http_server_request_t;

}  // extern "C"

namespace node {

void InitDTrace(Environment* env, v8::Local<v8::Object> target);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DTRACE_H_
