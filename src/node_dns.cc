// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_dns.h>

#include <stdlib.h> /* exit() */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <v8.h>
#include <ev.h>
#include <udns.h>

namespace node {

using namespace v8;

static ev_io io_watcher;
static ev_timer timer_watcher;

static Persistent<String> errno_symbol;

static inline Persistent<Function>* cb_persist(const Local<Value> &v) {
  Persistent<Function> *fn = new Persistent<Function>();
  *fn = Persistent<Function>::New(Local<Function>::Cast(v));
  return fn;
}

static inline Persistent<Function>* cb_unwrap(void *data) {
  Persistent<Function> *cb =
    reinterpret_cast<Persistent<Function>*>(data);
  assert((*cb)->IsFunction());
  return cb;
}

static inline void cb_destroy(Persistent<Function> * cb) {
  cb->Dispose();
  delete cb;
}

static inline void set_timeout() {
  int maxwait = 20;
  int wait = dns_timeouts(NULL, maxwait, ev_now(EV_DEFAULT_UC));

  ev_timer_stop(EV_DEFAULT_UC_ &timer_watcher);

  if (!dns_active(NULL)) return;

  if (wait >= 0) {
    ev_timer_set(&timer_watcher, static_cast<double>(wait), 0.0);
    ev_timer_start(EV_DEFAULT_UC_ &timer_watcher);
  }
}

static inline void maybe_start() {
  ev_io_start(EV_DEFAULT_UC_ &io_watcher);
  set_timeout();
}

static void ioevent(EV_P_ ev_io *_watcher, int revents) {
  assert(revents == EV_READ);
  assert(_watcher == &io_watcher);
  dns_ioevent(NULL, ev_now(EV_DEFAULT_UC));
  if (!dns_active(NULL)) ev_io_stop(EV_DEFAULT_UC_ &io_watcher);
  set_timeout();
}

static void timeout(EV_P_ ev_timer *_watcher, int revents) {
  assert(revents == EV_TIMEOUT);
  assert(_watcher == &timer_watcher);
  set_timeout();
}

static void ResolveError(Handle<Function> *cb) {
  HandleScope scope;
  int status = dns_status(NULL);
  assert(status < 0);

  Local<Value> e = Exception::Error(String::NewSymbol(dns_strerror(status)));
  Local<Object> obj = e->ToObject();
  obj->Set(errno_symbol, Integer::New(status));

  (*cb)->Call(Context::GetCurrent()->Global(), 1, &e);
}

static void AfterResolveA4(struct dns_ctx *ctx,
                           struct dns_rr_a4 *result,
                           void *data) {
  assert(ctx == &dns_defctx);

  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if (result == NULL) {
    ResolveError(cb);
    cb_destroy(cb);
    return;
  }

  /* canonical name */
  Local<String> cname = String::New(result->dnsa4_cname);

  /* Time-To-Live (TTL) value */
  Local<Integer> ttl = Integer::New(result->dnsa4_ttl);

  Local<Array> addresses = Array::New(result->dnsa4_nrr);
  for (int i = 0; i < result->dnsa4_nrr; i++) {
    HandleScope loop_scope;

    char ip[INET_ADDRSTRLEN];
    dns_ntop(AF_INET, &(result->dnsa4_addr[i]), ip, INET_ADDRSTRLEN);
    Local<String> address = String::New(ip);

    addresses->Set(Integer::New(i), address);
  }

  Local<Value> argv[3] = { addresses, ttl, cname };

  TryCatch try_catch;

  (*cb)->Call(Context::GetCurrent()->Global(), 3, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}

static void AfterResolveA6(struct dns_ctx *ctx,
                           struct dns_rr_a6 *result,
                           void *data) {
  assert(ctx == &dns_defctx);

  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if (result == NULL) {
    ResolveError(cb);
    cb_destroy(cb);
    return;
  }

  /* canonical name */
  Local<String> cname = String::New(result->dnsa6_cname);

  /* Time-To-Live (TTL) value */
  Local<Integer> ttl = Integer::New(result->dnsa6_ttl);

  Local<Array> addresses = Array::New(result->dnsa6_nrr);
  for (int i = 0; i < result->dnsa6_nrr; i++) {
    HandleScope loop_scope;

    char ip[INET6_ADDRSTRLEN];
    dns_ntop(AF_INET6, &(result->dnsa6_addr[i]), ip, INET6_ADDRSTRLEN);
    Local<String> address = String::New(ip);

    addresses->Set(Integer::New(i), address);
  }

  Local<Value> argv[3] = { addresses, ttl, cname };

  TryCatch try_catch;

  (*cb)->Call(Context::GetCurrent()->Global(), 3, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}

static Handle<Value> ResolveA(int type, const Arguments& args) {
  HandleScope scope;

  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("Argument must be a string.")));
  }

  String::Utf8Value name(args[0]->ToString());

  struct dns_query *query;
  switch (type) {
    case DNS_T_A:
      query = dns_submit_a4(NULL, *name, 0, AfterResolveA4, cb_persist(args[1]));
      break;

    case DNS_T_AAAA:
      query = dns_submit_a6(NULL, *name, 0, AfterResolveA6, cb_persist(args[1]));
      break;

    default:
      return ThrowException(Exception::Error(String::New("Unsupported type")));
  }

  assert(query);  // TODO(ry) better error handling.

  maybe_start();

  return Undefined();
}

static Handle<Value> ResolveA4(const Arguments& args) {
  return ResolveA(DNS_T_A, args);
}

static Handle<Value> ResolveA6(const Arguments& args) {
  return ResolveA(DNS_T_AAAA, args);
}

static void AfterReverse(struct dns_ctx *ctx,
                         struct dns_rr_ptr *result,
                         void *data) {
  assert(ctx == &dns_defctx);

  HandleScope scope;

  Persistent<Function> *cb = cb_unwrap(data);

  if (result == NULL) {
    ResolveError(cb);
    cb_destroy(cb);
    return;
  }

  /* canonical name */
  Local<String> cname = String::New(result->dnsptr_cname);

  /* Time-To-Live (TTL) value */
  Local<Integer> ttl = Integer::New(result->dnsptr_ttl);

  Local<Array> domains = Array::New();

  for (int i = 0; i < result->dnsptr_nrr; i++) {
    HandleScope loop_scope;

    Local<String> domain = String::New(result->dnsptr_ptr[i]);
    domains->Set(Integer::New(i), domain);
  }

  Local<Value> argv[3] = { domains, ttl, cname };

  TryCatch try_catch;

  (*cb)->Call(Context::GetCurrent()->Global(), 3, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  cb_destroy(cb);
}

static Handle<Value> Reverse(const Arguments& args) {
  HandleScope scope;

  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(Exception::Error(
          String::New("Argument must be a string.")));
  }

  String::Utf8Value ip_address(args[0]->ToString());

  union {
    struct in_addr addr;
    struct in6_addr addr6;
  } a;

  bool v4;

  if (dns_pton(AF_INET, *ip_address, &a.addr) > 0) {
    v4 = true;
  } else if (dns_pton(AF_INET6, *ip_address, &a.addr6) > 0) {
    v4 = false;
  } else {
    return ThrowException(Exception::Error(String::New("Invalid IP address")));
  }


  struct dns_query *query;

  if (v4) {
    query = dns_submit_a4ptr(NULL, &a.addr, AfterReverse, cb_persist(args[1]));
  } else {
    query = dns_submit_a6ptr(NULL, &a.addr6, AfterReverse, cb_persist(args[1]));
  }

  assert(query);  // TODO(ry) better error handling.

  maybe_start();

  return Undefined();
}

void DNS::Initialize(Handle<Object> target) {
  if (dns_init(NULL, 0) < 0) {
    fprintf(stderr, "Error initializing UDNS context\n");
    exit(-2);
  }

  int fd = dns_open(NULL);

  ev_io_init(&io_watcher, ioevent, fd, EV_READ);
  ev_init(&timer_watcher, timeout);

  HandleScope scope;

  errno_symbol = NODE_PSYMBOL("errno");

  target->Set(String::NewSymbol("TEMPFAIL"), Integer::New(DNS_E_TEMPFAIL));
  target->Set(String::NewSymbol("PROTOCOL"), Integer::New(DNS_E_PROTOCOL));
  target->Set(String::NewSymbol("NXDOMAIN"), Integer::New(DNS_E_NXDOMAIN));
  target->Set(String::NewSymbol("NODATA"), Integer::New(DNS_E_NODATA));
  target->Set(String::NewSymbol("NOMEM"), Integer::New(DNS_E_NOMEM));
  target->Set(String::NewSymbol("BADQUERY"), Integer::New(DNS_E_BADQUERY));

  Local<FunctionTemplate> resolve4 = FunctionTemplate::New(ResolveA4);
  target->Set(String::NewSymbol("resolve4"), resolve4->GetFunction());

  Local<FunctionTemplate> resolve6 = FunctionTemplate::New(ResolveA6);
  target->Set(String::NewSymbol("resolve6"), resolve6->GetFunction());

  Local<FunctionTemplate> reverse = FunctionTemplate::New(Reverse);
  target->Set(String::NewSymbol("reverse"), reverse->GetFunction());
}

}  // namespace node
