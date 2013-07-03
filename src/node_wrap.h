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

#ifndef NODE_WRAP_H
#define NODE_WRAP_H

#include "v8.h"
#include "uv.h"

#include "pipe_wrap.h"
#include "tty_wrap.h"
#include "tcp_wrap.h"
#include "udp_wrap.h"

namespace node {

extern v8::Persistent<v8::FunctionTemplate> pipeConstructorTmpl;
extern v8::Persistent<v8::FunctionTemplate> ttyConstructorTmpl;
extern v8::Persistent<v8::FunctionTemplate> tcpConstructorTmpl;

#define WITH_GENERIC_STREAM(obj, BODY)                    \
    do {                                                  \
      if (!tcpConstructorTmpl.IsEmpty() &&                \
          HasInstance(tcpConstructorTmpl, obj)) {         \
        PipeWrap* wrap = PipeWrap::Unwrap(obj);           \
        BODY                                              \
      } else if (!ttyConstructorTmpl.IsEmpty() &&         \
                 HasInstance(ttyConstructorTmpl, obj)) {  \
        TTYWrap* wrap = TTYWrap::Unwrap(obj);             \
        BODY                                              \
      } else if (!pipeConstructorTmpl.IsEmpty() &&        \
                 HasInstance(pipeConstructorTmpl, obj)) { \
        TCPWrap* wrap = TCPWrap::Unwrap(obj);             \
        BODY                                              \
      }                                                   \
    } while (0)

inline uv_stream_t* HandleToStream(v8::Local<v8::Object> obj) {
  v8::HandleScope scope(node_isolate);

  WITH_GENERIC_STREAM(obj, {
    return reinterpret_cast<uv_stream_t*>(wrap->UVHandle());
  });

  return NULL;
}

}

#endif
