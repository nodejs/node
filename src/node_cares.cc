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

#include <node_cares.h>

#include <node.h>
#include <v8.h>
#include <ares.h>

#include <sys/types.h>

#ifdef __POSIX__
# include <sys/socket.h>
# include <netdb.h>

# include <arpa/nameser.h>
# include <arpa/inet.h>
#endif

#ifdef __MINGW32__
# include <nameser.h>
#endif

#ifdef __OpenBSD__
# ifndef ns_t_a
#  include <nameser.h>
# endif
#endif  // __OpenBSD__

/*
 * HACK to use inet_pton/inet_ntop from c-ares because mingw32 doesn't have it
 * This trick is used in node_net.cc as well
 * TODO fixme
 */
#ifdef __MINGW32__
  extern "C" {
#   include <inet_net_pton.h>
#   include <inet_ntop.h>
  }

# define inet_pton ares_inet_pton
# define inet_ntop ares_inet_ntop
#endif

namespace node {

using namespace v8;


class Channel : public ObjectWrap {
 public:
  static void Initialize(Handle<Object> target);

 private:
  static Persistent<FunctionTemplate> constructor_template;

  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Query(const Arguments& args);
  static Handle<Value> GetHostByName(const Arguments& args);
  static Handle<Value> GetHostByAddr(const Arguments& args);
  static Handle<Value> Timeout(const Arguments& args);
  static Handle<Value> ProcessFD(const Arguments& args);

  ares_channel channel;

  static void SockStateCb(void *data, ares_socket_t sock, int read, int write);
  static void QueryCb(void *arg, int status, int timeouts, unsigned char* abuf, int alen);
};


// To be passed to the QueryCb callback when a Query is finished.
// Holds a C callback to parse the response and the final JS callback
struct QueryArg {
  typedef void (*ParseAnswerCb)(QueryArg*, unsigned char*, int);
  Persistent<Function> js_cb;
  ParseAnswerCb parse_cb;

  QueryArg(const Local<Value> &js_cb, ParseAnswerCb parse_cb)
    : js_cb(Persistent<Function>::New(Local<Function>::Cast(js_cb)))
    , parse_cb(parse_cb) {}

  ~QueryArg() {
    js_cb.Dispose();
  }
};


Persistent<FunctionTemplate> Channel::constructor_template;
static Persistent<String> priority_symbol;
static Persistent<String> weight_symbol;
static Persistent<String> port_symbol;
static Persistent<String> name_symbol;
static Persistent<String> callback_symbol;
static Persistent<String> exchange_symbol;


void Cares::Initialize(Handle<Object> target) {
  HandleScope scope;

  int r = ares_library_init(ARES_LIB_INIT_ALL);
  if (0 != r) {
    // TODO
    // ThrowException(Exception::Error(String::New(ares_strerror(r))));
    assert(r == 0);
  }


  target->Set(String::NewSymbol("SOCKET_BAD"), Integer::New(ARES_SOCKET_BAD));

  priority_symbol = NODE_PSYMBOL("priority");
  weight_symbol = NODE_PSYMBOL("weight");
  port_symbol = NODE_PSYMBOL("port");
  name_symbol = NODE_PSYMBOL("name");
  exchange_symbol = NODE_PSYMBOL("exchange");

  target->Set(String::NewSymbol("AF_INET"), Integer::New(AF_INET));
  target->Set(String::NewSymbol("AF_INET6"), Integer::New(AF_INET6));

  target->Set(String::NewSymbol("A"), Integer::New(ns_t_a));
  target->Set(String::NewSymbol("AAAA"), Integer::New(ns_t_aaaa));
  target->Set(String::NewSymbol("MX"), Integer::New(ns_t_mx));
  target->Set(String::NewSymbol("NS"), Integer::New(ns_t_ns));
  target->Set(String::NewSymbol("PTR"), Integer::New(ns_t_ptr));
  target->Set(String::NewSymbol("TXT"), Integer::New(ns_t_txt));
  target->Set(String::NewSymbol("SRV"), Integer::New(ns_t_srv));
  target->Set(String::NewSymbol("CNAME"), Integer::New(ns_t_cname));

  target->Set(String::NewSymbol("NODATA"), Integer::New(ARES_ENODATA));
  target->Set(String::NewSymbol("FORMERR"), Integer::New(ARES_EFORMERR));
  target->Set(String::NewSymbol("BADRESP"), Integer::New(ARES_EBADRESP));
  target->Set(String::NewSymbol("NOTFOUND"), Integer::New(ARES_ENOTFOUND));
  target->Set(String::NewSymbol("BADNAME"), Integer::New(ARES_EBADNAME));
  target->Set(String::NewSymbol("TIMEOUT"), Integer::New(ARES_ETIMEOUT));
  target->Set(String::NewSymbol("CONNREFUSED"), Integer::New(ARES_ECONNREFUSED));
  target->Set(String::NewSymbol("NOMEM"), Integer::New(ARES_ENOMEM));
  target->Set(String::NewSymbol("DESTRUCTION"), Integer::New(ARES_EDESTRUCTION));

  // Only occur if the ARES_FLAG_NOCHECKRESP flag was specified
  target->Set(String::NewSymbol("NOTIMP"), Integer::New(ARES_ENOTIMP));
  target->Set(String::NewSymbol("EREFUSED"), Integer::New(ARES_EREFUSED));
  target->Set(String::NewSymbol("SERVFAIL"), Integer::New(ARES_ESERVFAIL));

  Channel::Initialize(target);
}


static Local<Array> HostEntToAddresses(struct hostent* hostent) {
  Local<Array> addresses = Array::New();


  char ip[INET6_ADDRSTRLEN];
  for (int i = 0; hostent->h_addr_list[i]; ++i) {
    inet_ntop(hostent->h_addrtype, hostent->h_addr_list[i], ip, sizeof(ip));

    Local<String> address = String::New(ip);
    addresses->Set(Integer::New(i), address);
  }

  return addresses;
}


static Local<Array> HostEntToNames(struct hostent* hostent) {
  Local<Array> names = Array::New();

  for (int i = 0; hostent->h_aliases[i]; ++i) {
    Local<String> address = String::New(hostent->h_aliases[i]);
    names->Set(Integer::New(i), address);
  }

  return names;
}

static inline const char *ares_errno_string(int errorno) {
#define ERRNO_CASE(e)  case ARES_##e: return #e;
  switch (errorno) {
    ERRNO_CASE(SUCCESS)
    ERRNO_CASE(ENODATA)
    ERRNO_CASE(EFORMERR)
    ERRNO_CASE(ESERVFAIL)
    ERRNO_CASE(ENOTFOUND)
    ERRNO_CASE(ENOTIMP)
    ERRNO_CASE(EREFUSED)
    ERRNO_CASE(EBADQUERY)
    ERRNO_CASE(EBADNAME)
    ERRNO_CASE(EBADFAMILY)
    ERRNO_CASE(EBADRESP)
    ERRNO_CASE(ECONNREFUSED)
    ERRNO_CASE(ETIMEOUT)
    ERRNO_CASE(EOF)
    ERRNO_CASE(EFILE)
    ERRNO_CASE(ENOMEM)
    ERRNO_CASE(EDESTRUCTION)
    ERRNO_CASE(EBADSTR)
    ERRNO_CASE(EBADFLAGS)
    ERRNO_CASE(ENONAME)
    ERRNO_CASE(EBADHINTS)
    ERRNO_CASE(ENOTINITIALIZED)
    ERRNO_CASE(ELOADIPHLPAPI)
    ERRNO_CASE(EADDRGETNETWORKPARAMS)
    ERRNO_CASE(ECANCELLED)
    default:
      assert(0 && "Unhandled c-ares errno");
      return "(UNKNOWN)";
  }
}


static void ResolveError(Persistent<Function> &cb, int status) {
  HandleScope scope;

  Local<String> code = String::NewSymbol(ares_errno_string(status));
  Local<String> message = String::NewSymbol(ares_strerror(status));

  Local<String> cons1 = String::Concat(code, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e = Exception::Error(cons2);

  Local<Object> obj = e->ToObject();
  obj->Set(String::NewSymbol("errno"), Integer::New(status));
  obj->Set(String::NewSymbol("code"), code);

  TryCatch try_catch;

  cb->Call(v8::Context::GetCurrent()->Global(), 1, &e);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


static void HostByNameCb(void *data,
                         int status,
                         int timeouts,
                         struct hostent *hostent) {
  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if  (status != ARES_SUCCESS) {
    ResolveError(*cb, status);
    cb_destroy(cb);
    return;
  }

  TryCatch try_catch;

  Local<Array> addresses = HostEntToAddresses(hostent);

  Local<Value> argv[2] = { Local<Value>::New(Null()), addresses};

  (*cb)->Call(v8::Context::GetCurrent()->Global(), 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}


static void HostByAddrCb(void *data,
                         int status,
                         int timeouts,
                         struct hostent *hostent) {
  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if  (status != ARES_SUCCESS) {
    ResolveError(*cb, status);
    cb_destroy(cb);
    return;
  }

  TryCatch try_catch;

  Local<Array> names = HostEntToNames(hostent);

  Local<Value> argv[2] = { Local<Value>::New(Null()), names };

  (*cb)->Call(v8::Context::GetCurrent()->Global(), 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}


static void cb_call(Persistent<Function> &cb, int argc, Local<Value> *argv) {
  TryCatch try_catch;

  cb->Call(v8::Context::GetCurrent()->Global(), argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


static void ParseAnswerA(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  hostent* host;

  int status = ares_parse_a_reply(abuf, alen, &host, NULL, NULL);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> addresses = HostEntToAddresses(host);
  ares_free_hostent(host);

  Local<Value> argv[2] = { Local<Value>::New(Null()), addresses };
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerAAAA(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  hostent* host;

  int status = ares_parse_aaaa_reply(abuf, alen, &host, NULL, NULL);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> addresses = HostEntToAddresses(host);
  ares_free_hostent(host);

  Local<Value> argv[2] = { Local<Value>::New(Null()), addresses};
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerCNAME(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  hostent* host;

  int status = ares_parse_a_reply(abuf, alen, &host, NULL, NULL);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  // a CNAME lookup always returns a single record but
  // it's probably best to follow the common API here
  Local<Array> addresses = Array::New(1);
  addresses->Set(0, String::New(host->h_name));
  ares_free_hostent(host);

  Local<Value> argv[2] = { Local<Value>::New(Null()), addresses };
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerMX(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  struct ares_mx_reply *mx_out;

  int status = ares_parse_mx_reply(abuf, alen, &mx_out);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> mx_records = Array::New();

  struct ares_mx_reply *current = mx_out;
  for (int i = 0; current; ++i, current = current->next) {
    Local<Object> mx = Object::New();

    mx->Set(priority_symbol, Integer::New(current->priority));
    mx->Set(exchange_symbol, String::New(current->host));

    mx_records->Set(Integer::New(i), mx);
  }
  ares_free_data(mx_out);

  Local<Value> argv[2] = { Local<Value>::New(Null()), mx_records };
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerNS(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  hostent* host;

  int status = ares_parse_ns_reply(abuf, alen, &host);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> names = HostEntToNames(host);
  ares_free_hostent(host);

  Local<Value> argv[2] = { Local<Value>::New(Null()), names };
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerSRV(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  struct ares_srv_reply *srv_out;

  int status = ares_parse_srv_reply(abuf, alen, &srv_out);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> srv_records = Array::New();

  struct ares_srv_reply *current = srv_out;
  for (int i = 0; current; ++i, current = current->next) {
    Local<Object> srv = Object::New();

    srv->Set(priority_symbol, Integer::New(current->priority));
    srv->Set(weight_symbol, Integer::New(current->weight));
    srv->Set(port_symbol, Integer::New(current->port));
    srv->Set(name_symbol, String::New(current->host));

    srv_records->Set(Integer::New(i), srv);
  }
  ares_free_data(srv_out);

  Local<Value> argv[2] = { Local<Value>::New(Null()), srv_records };
  cb_call(arg->js_cb, 2, argv);
}


static void ParseAnswerTXT(QueryArg *arg, unsigned char* abuf, int alen) {
  HandleScope scope;

  struct ares_txt_reply *txt_out;

  int status = ares_parse_txt_reply(abuf, alen, &txt_out);
  if (status != ARES_SUCCESS) {
    ResolveError(arg->js_cb, status);
    return;
  }

  Local<Array> txt_records = Array::New();

  struct ares_txt_reply *current = txt_out;
  for (int i = 0; current; ++i, current = current->next) {
    Local<String> txt = String::New(reinterpret_cast<char*>(current->txt));
    txt_records->Set(Integer::New(i), txt);
  }
  ares_free_data(txt_out);


  Local<Value> argv[2] = { Local<Value>::New(Null()), txt_records };
  cb_call(arg->js_cb, 2, argv);
}


void Channel::QueryCb(void *arg,
                      int status,
                      int timeouts,
                      unsigned char* abuf,
                      int alen) {
  QueryArg *query_arg = static_cast<QueryArg*>(arg);

  HandleScope scope;

  if (status != ARES_SUCCESS) {
    ResolveError(query_arg->js_cb, status);
    delete query_arg;
    return;
  }

  query_arg->parse_cb(query_arg, abuf, alen);

  delete query_arg;
}


void Channel::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Channel::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Channel"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "getHostByName", Channel::GetHostByName);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "getHostByAddr", Channel::GetHostByAddr);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "query", Channel::Query);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "timeout", Channel::Timeout);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "processFD", Channel::ProcessFD);

  target->Set(String::NewSymbol("Channel"), constructor_template->GetFunction());

  callback_symbol = NODE_PSYMBOL("callback");
}


Handle<Value> Channel::New(const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;

  struct ares_options options;
  int optmask = 0;

  Channel *c = new Channel();
  c->Wrap(args.This());

  if (args.Length() > 0) {
    if(!args[0]->IsObject()) {
      return ThrowException(Exception::TypeError(
            String::New("Bad Options Argument")));
    }

    Local<Object> options_o = Local<Object>::Cast(args[0]);

    Local<Value> cb = options_o->Get(String::NewSymbol("SOCK_STATE_CB"));
    if (!cb.IsEmpty()) {
      c->handle_->Set(callback_symbol, cb);
      options.sock_state_cb_data = c;
      options.sock_state_cb = Channel::SockStateCb;
      optmask |= ARES_OPT_SOCK_STATE_CB;
    }
  }

  ares_init_options(&c->channel, &options, optmask);

  return args.This();
}


Handle<Value> Channel::Query(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("First argument must be a name")));
  }

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument must be a query type")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::TypeError(
          String::New("Third argument must be a callback")));
  }

  String::Utf8Value name(args[0]->ToString());
  int type = args[1]->Int32Value();
  QueryArg::ParseAnswerCb parse_cb;

  switch(type) {
    case ns_t_a:
      parse_cb = ParseAnswerA;
      break;

    case ns_t_aaaa:
      parse_cb = ParseAnswerAAAA;
      break;

    case ns_t_mx:
      parse_cb = ParseAnswerMX;
      break;

    case ns_t_ns:
      parse_cb = ParseAnswerNS;
      break;

    case ns_t_txt:
      parse_cb = ParseAnswerTXT;
      break;

    case ns_t_srv:
      parse_cb = ParseAnswerSRV;
      break;

    case ns_t_cname:
      parse_cb = ParseAnswerCNAME;
      break;

    case ns_t_ptr:

      int length, family;
      char address_b[sizeof(struct in6_addr)];

      if (inet_pton(AF_INET, *name, &address_b) == 1) {
        length = sizeof(struct in_addr);
        family = AF_INET;
      } else if (inet_pton(AF_INET6, *name, &address_b) == 1) {
        length = sizeof(struct in6_addr);
        family = AF_INET6;
      } else {
        return ThrowException(Exception::Error(String::New("Invalid IP address")));
      }

      ares_gethostbyaddr(c->channel, address_b, length, family, HostByAddrCb, cb_persist(args[2]));

      return Undefined();

    default:
      return ThrowException(Exception::Error(
            String::New("Unsupported query type")));
  }

  QueryArg *query_arg = new QueryArg(args[2], parse_cb);

  ares_query(c->channel, *name, ns_c_in, type, QueryCb, query_arg);

  return Undefined();
}


Handle<Value> Channel::GetHostByAddr(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be a address")));
  }

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("Second argument must be an address family")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::Error(
          String::New("Third argument must be a callback")));
  }

  int family = args[1]->Int32Value();
  if (family != AF_INET6 && family != AF_INET) {
    return ThrowException(Exception::Error(
          String::New("Unsupported address family")));
  }

  String::Utf8Value address_s(args[0]->ToString());

  char address_b[sizeof(struct in6_addr)];
  int r = inet_pton(family, *address_s, address_b);
  if (r != 1) {
    return ThrowException(Exception::Error(
          String::New("Invalid network address")));
  }

  int length;
  if (family == AF_INET6)
    length = sizeof(struct in6_addr);
  else
    length = sizeof(struct in_addr);

  ares_gethostbyaddr(c->channel, address_b, length, family, HostByAddrCb, cb_persist(args[2]));

  return Undefined();
}



Handle<Value> Channel::GetHostByName(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be a name")));
  }

  if (!args[1]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("Second argument must be a family")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::Error(
          String::New("Third argument must be a callback")));
  }

  int family = args[1]->Int32Value();
  if (family != AF_INET6 && family != AF_INET) {
    return ThrowException(Exception::Error(
          String::New("Unsupported address family")));
  }

  String::Utf8Value name(args[0]->ToString());

  ares_gethostbyname(c->channel, *name, family, HostByNameCb, cb_persist(args[2]));

  return Undefined();
}


Handle<Value> Channel::Timeout(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be an integer number of milliseconds")));
  }

  struct timeval tvbuf, maxtv, *ret;

  int64_t time = args[0]->IntegerValue();
  maxtv.tv_sec = time/1000;
  maxtv.tv_usec = (time % 1000) * 1000;

  ret = ares_timeout(c->channel, &maxtv, &tvbuf);

  return scope.Close(Integer::New(ret->tv_sec * 1000 + ret->tv_usec / 1000));
}


Handle<Value> Channel::ProcessFD(const Arguments& args) {
  HandleScope scope;
  Channel *c = ObjectWrap::Unwrap<Channel>(args.Holder());
  assert(c);

  int read_fd, write_fd;

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::Error(
          String::New("First argument must be a file descriptor or SOCKET_BAD")));
  }

  read_fd = args[0]->Int32Value();

  if (args.Length() > 1) {

    if (!args[1]->IsInt32()) {
      return ThrowException(Exception::Error(
            String::New("Second argument must be a file descriptor or SOCKET_BAD")));
    }
    write_fd = args[1]->Int32Value();

  } else {
    write_fd = ARES_SOCKET_BAD;
  }

  ares_process_fd(c->channel, read_fd, write_fd);

  return Undefined();
}


void Channel::SockStateCb(void *data, ares_socket_t sock, int read, int write) {
  Channel *c = static_cast<Channel*>(data);
  HandleScope scope;

  Local<Value> callback_v = c->handle_->Get(callback_symbol);
  if (!callback_v->IsFunction()) return;
  Local<Function> callback = Local<Function>::Cast(callback_v);

  Local<Value> argv[3];

  argv[0] = Integer::New(sock);
  argv[1] = Integer::New(read);
  argv[2] = Integer::New(write);

  TryCatch try_catch;

  callback->Call(c->handle_, 3, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}




}  // namespace node

NODE_MODULE(node_cares, node::Cares::Initialize);
