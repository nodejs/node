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

#include "node_dtrace.h"

#ifdef HAVE_DTRACE
#include "node_provider.h"
#elif HAVE_ETW
#include "node_win32_etw_provider.h"
#include "node_win32_etw_provider-inl.h"
#else
#define NODE_HTTP_SERVER_REQUEST(arg0, arg1)
#define NODE_HTTP_SERVER_REQUEST_ENABLED() (0)
#define NODE_HTTP_SERVER_RESPONSE(arg0)
#define NODE_HTTP_SERVER_RESPONSE_ENABLED() (0)
#define NODE_HTTP_CLIENT_REQUEST(arg0, arg1)
#define NODE_HTTP_CLIENT_REQUEST_ENABLED() (0)
#define NODE_HTTP_CLIENT_RESPONSE(arg0)
#define NODE_HTTP_CLIENT_RESPONSE_ENABLED() (0)
#define NODE_NET_SERVER_CONNECTION(arg0)
#define NODE_NET_SERVER_CONNECTION_ENABLED() (0)
#define NODE_NET_STREAM_END(arg0)
#define NODE_NET_STREAM_END_ENABLED() (0)
#define NODE_GC_START(arg0, arg1, arg2)
#define NODE_GC_DONE(arg0, arg1, arg2)
#endif

#include "env.h"
#include "env-inl.h"

#include "util.h"

#include <string.h>

namespace node {

using v8::FunctionCallbackInfo;
using v8::GCCallbackFlags;
using v8::GCType;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

#define SLURP_STRING(obj, member, valp) \
  if (!(obj)->IsObject()) { \
    return env->ThrowError( \
        "expected object for " #obj " to contain string member " #member); \
  } \
  node::Utf8Value _##member(env->isolate(), \
      obj->Get(OneByteString(env->isolate(), #member))); \
  if ((*(const char **)valp = *_##member) == nullptr) \
    *(const char **)valp = "<unknown>";

#define SLURP_INT(obj, member, valp) \
  if (!(obj)->IsObject()) { \
    return env->ThrowError( \
      "expected object for " #obj " to contain integer member " #member); \
  } \
  *valp = obj->Get(OneByteString(env->isolate(), #member)) \
      ->Int32Value();

#define SLURP_OBJECT(obj, member, valp) \
  if (!(obj)->IsObject()) { \
    return env->ThrowError( \
      "expected object for " #obj " to contain object member " #member); \
  } \
  *valp = Local<Object>::Cast(obj->Get(OneByteString(env->isolate(), #member)));

#define SLURP_CONNECTION(arg, conn) \
  if (!(arg)->IsObject()) { \
    return env->ThrowError( \
      "expected argument " #arg " to be a connection object"); \
  } \
  node_dtrace_connection_t conn; \
  Local<Object> _##conn = Local<Object>::Cast(arg); \
  Local<Value> _handle = \
      (_##conn)->Get(FIXED_ONE_BYTE_STRING(env->isolate(), "_handle")); \
  if (_handle->IsObject()) { \
    SLURP_INT(_handle.As<Object>(), fd, &conn.fd); \
  } else { \
    conn.fd = -1; \
  } \
  SLURP_STRING(_##conn, remoteAddress, &conn.remote); \
  SLURP_INT(_##conn, remotePort, &conn.port); \
  SLURP_INT(_##conn, bufferSize, &conn.buffered);

#define SLURP_CONNECTION_HTTP_CLIENT(arg, conn) \
  if (!(arg)->IsObject()) { \
    return env->ThrowError( \
      "expected argument " #arg " to be a connection object"); \
  } \
  node_dtrace_connection_t conn; \
  Local<Object> _##conn = Local<Object>::Cast(arg); \
  SLURP_INT(_##conn, fd, &conn.fd); \
  SLURP_STRING(_##conn, host, &conn.remote); \
  SLURP_INT(_##conn, port, &conn.port); \
  SLURP_INT(_##conn, bufferSize, &conn.buffered);

#define SLURP_CONNECTION_HTTP_CLIENT_RESPONSE(arg0, arg1, conn) \
  if (!(arg0)->IsObject()) { \
    return env->ThrowError( \
      "expected argument " #arg0 " to be a connection object"); \
  } \
  if (!(arg1)->IsObject()) { \
    return env->ThrowError( \
      "expected argument " #arg1 " to be a connection object"); \
  } \
  node_dtrace_connection_t conn; \
  Local<Object> _##conn = Local<Object>::Cast(arg0); \
  SLURP_INT(_##conn, fd, &conn.fd); \
  SLURP_INT(_##conn, bufferSize, &conn.buffered); \
  _##conn = Local<Object>::Cast(arg1); \
  SLURP_STRING(_##conn, host, &conn.remote); \
  SLURP_INT(_##conn, port, &conn.port);


void DTRACE_NET_SERVER_CONNECTION(const FunctionCallbackInfo<Value>& args) {
  if (!NODE_NET_SERVER_CONNECTION_ENABLED())
    return;
  Environment* env = Environment::GetCurrent(args);
  SLURP_CONNECTION(args[0], conn);
  NODE_NET_SERVER_CONNECTION(&conn, conn.remote, conn.port, conn.fd);
}


void DTRACE_NET_STREAM_END(const FunctionCallbackInfo<Value>& args) {
  if (!NODE_NET_STREAM_END_ENABLED())
    return;
  Environment* env = Environment::GetCurrent(args);
  SLURP_CONNECTION(args[0], conn);
  NODE_NET_STREAM_END(&conn, conn.remote, conn.port, conn.fd);
}

void DTRACE_HTTP_SERVER_REQUEST(const FunctionCallbackInfo<Value>& args) {
  node_dtrace_http_server_request_t req;

  if (!NODE_HTTP_SERVER_REQUEST_ENABLED())
    return;

  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());
  Local<Object> arg0 = Local<Object>::Cast(args[0]);
  Local<Object> headers;

  memset(&req, 0, sizeof(req));
  req._un.version = 1;
  SLURP_STRING(arg0, url, &req.url);
  SLURP_STRING(arg0, method, &req.method);
  SLURP_OBJECT(arg0, headers, &headers);

  if (!(headers)->IsObject()) {
    return env->ThrowError(
      "expected object for request to contain string member headers");
  }

  Local<Value> strfwdfor = headers->Get(env->x_forwarded_string());
  node::Utf8Value fwdfor(env->isolate(), strfwdfor);

  if (!strfwdfor->IsString() || (req.forwardedFor = *fwdfor) == nullptr)
    req.forwardedFor = const_cast<char*>("");

  SLURP_CONNECTION(args[1], conn);
  NODE_HTTP_SERVER_REQUEST(&req, &conn, conn.remote, conn.port, req.method, \
                           req.url, conn.fd);
}


void DTRACE_HTTP_SERVER_RESPONSE(const FunctionCallbackInfo<Value>& args) {
  if (!NODE_HTTP_SERVER_RESPONSE_ENABLED())
    return;
  Environment* env = Environment::GetCurrent(args);
  SLURP_CONNECTION(args[0], conn);
  NODE_HTTP_SERVER_RESPONSE(&conn, conn.remote, conn.port, conn.fd);
}


void DTRACE_HTTP_CLIENT_REQUEST(const FunctionCallbackInfo<Value>& args) {
  node_dtrace_http_client_request_t req;
  char *header;

  if (!NODE_HTTP_CLIENT_REQUEST_ENABLED())
    return;

  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());

  /*
   * For the method and URL, we're going to dig them out of the header.  This
   * is not as efficient as it could be, but we would rather not force the
   * caller here to retain their method and URL until the time at which
   * DTRACE_HTTP_CLIENT_REQUEST can be called.
   */
  Local<Object> arg0 = Local<Object>::Cast(args[0]);
  SLURP_STRING(arg0, _header, &header);

  req.method = header;

  while (*header != '\0' && *header != ' ')
    header++;

  if (*header != '\0')
    *header++ = '\0';

  req.url = header;

  while (*header != '\0' && *header != ' ')
    header++;

  *header = '\0';

  SLURP_CONNECTION_HTTP_CLIENT(args[1], conn);
  NODE_HTTP_CLIENT_REQUEST(&req, &conn, conn.remote, conn.port, req.method, \
                           req.url, conn.fd);
}


void DTRACE_HTTP_CLIENT_RESPONSE(const FunctionCallbackInfo<Value>& args) {
  if (!NODE_HTTP_CLIENT_RESPONSE_ENABLED())
    return;
  Environment* env = Environment::GetCurrent(args);
  SLURP_CONNECTION_HTTP_CLIENT_RESPONSE(args[0], args[1], conn);
  NODE_HTTP_CLIENT_RESPONSE(&conn, conn.remote, conn.port, conn.fd);
}


void dtrace_gc_start(Isolate* isolate, GCType type, GCCallbackFlags flags) {
  // Previous versions of this probe point only logged type and flags.
  // That's why for reasons of backwards compatibility the isolate goes last.
  NODE_GC_START(type, flags, isolate);
}


void dtrace_gc_done(Isolate* isolate, GCType type, GCCallbackFlags flags) {
  // Previous versions of this probe point only logged type and flags.
  // That's why for reasons of backwards compatibility the isolate goes last.
  NODE_GC_DONE(type, flags, isolate);
}


void InitDTrace(Environment* env, Local<Object> target) {
  HandleScope scope(env->isolate());

  static struct {
    const char *name;
    void (*func)(const FunctionCallbackInfo<Value>&);
  } tab[] = {
#define NODE_PROBE(name) #name, name
    { NODE_PROBE(DTRACE_NET_SERVER_CONNECTION) },
    { NODE_PROBE(DTRACE_NET_STREAM_END) },
    { NODE_PROBE(DTRACE_HTTP_SERVER_REQUEST) },
    { NODE_PROBE(DTRACE_HTTP_SERVER_RESPONSE) },
    { NODE_PROBE(DTRACE_HTTP_CLIENT_REQUEST) },
    { NODE_PROBE(DTRACE_HTTP_CLIENT_RESPONSE) }
#undef NODE_PROBE
  };

  for (size_t i = 0; i < arraysize(tab); i++) {
    Local<String> key = OneByteString(env->isolate(), tab[i].name);
    Local<Value> val = env->NewFunctionTemplate(tab[i].func)->GetFunction();
    target->Set(key, val);
  }

#ifdef HAVE_ETW
  init_etw();
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
  env->isolate()->AddGCPrologueCallback(dtrace_gc_start);
  env->isolate()->AddGCEpilogueCallback(dtrace_gc_done);
#endif
}

}  // namespace node
