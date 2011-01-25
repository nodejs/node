#include <node_dtrace.h>

#ifdef HAVE_DTRACE
#include "node_provider.h"
#else
#define NODE_HTTP_SERVER_REQUEST(arg0, arg1)
#define NODE_HTTP_SERVER_REQUEST_ENABLED() (0)
#define NODE_HTTP_SERVER_RESPONSE(arg0)
#define NODE_HTTP_SERVER_RESPONSE_ENABLED() (0)
#define NODE_NET_SERVER_CONNECTION(arg0)
#define NODE_NET_SERVER_CONNECTION_ENABLED() (0)
#define NODE_NET_STREAM_END(arg0)
#define NODE_NET_STREAM_END_ENABLED() (0)
#endif

namespace node {

using namespace v8;

#define SLURP_STRING(obj, member, valp) \
  String::Utf8Value _##member(obj->Get(String::New(#member))->ToString()); \
  if ((*(const char **)valp = *_##member) == NULL) \
    *(const char **)valp = "<unknown>";

#define SLURP_INT(obj, member, valp) \
  *valp = obj->Get(String::New(#member))->ToInteger()->Value();

#define SLURP_CONNECTION(arg, conn) \
  node_dtrace_connection_t conn; \
  Local<Object> _##conn = Local<Object>::Cast(arg); \
  SLURP_INT(_##conn, fd, &conn.fd); \
  SLURP_STRING(_##conn, remoteAddress, &conn.remote); \
  SLURP_INT(_##conn, remotePort, &conn.port);

Handle<Value> DTRACE_NET_SERVER_CONNECTION(const Arguments& args) {
  if (!NODE_NET_SERVER_CONNECTION_ENABLED())
    return Undefined();

  HandleScope scope;

  SLURP_CONNECTION(args[0], conn);
  NODE_NET_SERVER_CONNECTION(&conn);

  return Undefined();
}

Handle<Value> DTRACE_NET_STREAM_END(const Arguments& args) {
  if (!NODE_NET_STREAM_END_ENABLED())
    return Undefined();

  HandleScope scope;

  SLURP_CONNECTION(args[0], conn);
  NODE_NET_STREAM_END(&conn);

  return Undefined();
}

Handle<Value> DTRACE_HTTP_SERVER_REQUEST(const Arguments& args) {
  node_dtrace_http_request_t req;

  if (!NODE_HTTP_SERVER_REQUEST_ENABLED())
    return Undefined();

  HandleScope scope;

  Local<Object> arg0 = Local<Object>::Cast(args[0]);
  Local<Object> arg1 = Local<Object>::Cast(args[1]);

  SLURP_STRING(arg0, url, &req.url);
  SLURP_STRING(arg0, method, &req.method);

  SLURP_CONNECTION(args[1], conn);

  NODE_HTTP_SERVER_REQUEST(&req, &conn);
  return Undefined();
}

Handle<Value> DTRACE_HTTP_SERVER_RESPONSE(const Arguments& args) {
  if (!NODE_HTTP_SERVER_RESPONSE_ENABLED())
    return Undefined();

  HandleScope scope;

  SLURP_CONNECTION(args[0], conn);
  NODE_HTTP_SERVER_RESPONSE(&conn);

  return Undefined();
}

#define NODE_PROBE(name) #name, name

void InitDTrace(Handle<Object> target) {
  static struct {
    const char *name;
    Handle<Value> (*func)(const Arguments&);
    Persistent<FunctionTemplate> templ;
  } tab[] = {
    { NODE_PROBE(DTRACE_NET_SERVER_CONNECTION) },
    { NODE_PROBE(DTRACE_NET_STREAM_END) },
    { NODE_PROBE(DTRACE_HTTP_SERVER_REQUEST) },
    { NODE_PROBE(DTRACE_HTTP_SERVER_RESPONSE) },
    { NULL }
  };

  for (int i = 0; tab[i].name != NULL; i++) {
    tab[i].templ = Persistent<FunctionTemplate>::New(
        FunctionTemplate::New(tab[i].func));
    target->Set(String::NewSymbol(tab[i].name), tab[i].templ->GetFunction());
  }
}

}
