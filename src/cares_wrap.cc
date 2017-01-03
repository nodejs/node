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

#define CARES_STATICLIB
#include "ares.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "tree.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__) || \
    defined(__MINGW32__) || \
    defined(__OpenBSD__) || \
    defined(_MSC_VER)

# include <nameser.h>
#else
# include <arpa/nameser.h>
#endif

#if defined(__OpenBSD__)
# define AI_V4MAPPED 0
#endif

namespace node {
namespace cares_wrap {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Value;


inline const char* ToErrorCodeString(int status) {
  switch (status) {
#define V(code) case ARES_##code: return #code;
    V(EADDRGETNETWORKPARAMS)
    V(EBADFAMILY)
    V(EBADFLAGS)
    V(EBADHINTS)
    V(EBADNAME)
    V(EBADQUERY)
    V(EBADRESP)
    V(EBADSTR)
    V(ECANCELLED)
    V(ECONNREFUSED)
    V(EDESTRUCTION)
    V(EFILE)
    V(EFORMERR)
    V(ELOADIPHLPAPI)
    V(ENODATA)
    V(ENOMEM)
    V(ENONAME)
    V(ENOTFOUND)
    V(ENOTIMP)
    V(ENOTINITIALIZED)
    V(EOF)
    V(EREFUSED)
    V(ESERVFAIL)
    V(ETIMEOUT)
#undef V
  }
  return "UNKNOWN_ARES_ERROR";
}

class GetAddrInfoReqWrap : public ReqWrap<uv_getaddrinfo_t> {
 public:
  GetAddrInfoReqWrap(Environment* env, Local<Object> req_wrap_obj);

  size_t self_size() const override { return sizeof(*this); }
};

GetAddrInfoReqWrap::GetAddrInfoReqWrap(Environment* env,
                                       Local<Object> req_wrap_obj)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_GETADDRINFOREQWRAP) {
  Wrap(req_wrap_obj, this);
}


static void NewGetAddrInfoReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


class GetNameInfoReqWrap : public ReqWrap<uv_getnameinfo_t> {
 public:
  GetNameInfoReqWrap(Environment* env, Local<Object> req_wrap_obj);

  size_t self_size() const override { return sizeof(*this); }
};

GetNameInfoReqWrap::GetNameInfoReqWrap(Environment* env,
                                       Local<Object> req_wrap_obj)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_GETNAMEINFOREQWRAP) {
  Wrap(req_wrap_obj, this);
}


static void NewGetNameInfoReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


static void NewQueryReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


static int cmp_ares_tasks(const node_ares_task* a, const node_ares_task* b) {
  if (a->sock < b->sock)
    return -1;
  if (a->sock > b->sock)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(node_ares_task_list, node_ares_task, node, cmp_ares_tasks)



/* This is called once per second by loop->timer. It is used to constantly */
/* call back into c-ares for possibly processing timeouts. */
static void ares_timeout(uv_timer_t* handle) {
  Environment* env = Environment::from_cares_timer_handle(handle);
  CHECK_EQ(false, RB_EMPTY(env->cares_task_list()));
  ares_process_fd(env->cares_channel(), ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void ares_poll_cb(uv_poll_t* watcher, int status, int events) {
  node_ares_task* task = ContainerOf(&node_ares_task::poll_watcher, watcher);
  Environment* env = task->env;

  /* Reset the idle timer */
  uv_timer_again(env->cares_timer_handle());

  if (status < 0) {
    /* An error happened. Just pretend that the socket is both readable and */
    /* writable. */
    ares_process_fd(env->cares_channel(), task->sock, task->sock);
    return;
  }

  /* Process DNS responses */
  ares_process_fd(env->cares_channel(),
                  events & UV_READABLE ? task->sock : ARES_SOCKET_BAD,
                  events & UV_WRITABLE ? task->sock : ARES_SOCKET_BAD);
}


static void ares_poll_close_cb(uv_handle_t* watcher) {
  node_ares_task* task = ContainerOf(&node_ares_task::poll_watcher,
                                  reinterpret_cast<uv_poll_t*>(watcher));
  free(task);
}


/* Allocates and returns a new node_ares_task */
static node_ares_task* ares_task_create(Environment* env, ares_socket_t sock) {
  auto task = node::UncheckedMalloc<node_ares_task>(1);

  if (task == nullptr) {
    /* Out of memory. */
    return nullptr;
  }

  task->env = env;
  task->sock = sock;

  if (uv_poll_init_socket(env->event_loop(), &task->poll_watcher, sock) < 0) {
    /* This should never happen. */
    free(task);
    return nullptr;
  }

  return task;
}


/* Callback from ares when socket operation is started */
static void ares_sockstate_cb(void* data,
                              ares_socket_t sock,
                              int read,
                              int write) {
  Environment* env = static_cast<Environment*>(data);
  node_ares_task* task;

  node_ares_task lookup_task;
  lookup_task.sock = sock;
  task = RB_FIND(node_ares_task_list, env->cares_task_list(), &lookup_task);

  if (read || write) {
    if (!task) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      uv_timer_t* timer_handle = env->cares_timer_handle();
      if (!uv_is_active(reinterpret_cast<uv_handle_t*>(timer_handle))) {
        CHECK(RB_EMPTY(env->cares_task_list()));
        uv_timer_start(timer_handle, ares_timeout, 1000, 1000);
      }

      task = ares_task_create(env, sock);
      if (task == nullptr) {
        /* This should never happen unless we're out of memory or something */
        /* is seriously wrong. The socket won't be polled, but the query will */
        /* eventually time out. */
        return;
      }

      RB_INSERT(node_ares_task_list, env->cares_task_list(), task);
    }

    /* This should never fail. If it fails anyway, the query will eventually */
    /* time out. */
    uv_poll_start(&task->poll_watcher,
                  (read ? UV_READABLE : 0) | (write ? UV_WRITABLE : 0),
                  ares_poll_cb);

  } else {
    /* read == 0 and write == 0 this is c-ares's way of notifying us that */
    /* the socket is now closed. We must free the data associated with */
    /* socket. */
    CHECK(task &&
          "When an ares socket is closed we should have a handle for it");

    RB_REMOVE(node_ares_task_list, env->cares_task_list(), task);
    uv_close(reinterpret_cast<uv_handle_t*>(&task->poll_watcher),
             ares_poll_close_cb);

    if (RB_EMPTY(env->cares_task_list())) {
      uv_timer_stop(env->cares_timer_handle());
    }
  }
}


static Local<Array> HostentToAddresses(Environment* env, struct hostent* host) {
  EscapableHandleScope scope(env->isolate());
  Local<Array> addresses = Array::New(env->isolate());

  char ip[INET6_ADDRSTRLEN];
  for (uint32_t i = 0; host->h_addr_list[i] != nullptr; ++i) {
    uv_inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));
    Local<String> address = OneByteString(env->isolate(), ip);
    addresses->Set(i, address);
  }

  return scope.Escape(addresses);
}


static Local<Array> HostentToNames(Environment* env, struct hostent* host) {
  EscapableHandleScope scope(env->isolate());
  Local<Array> names = Array::New(env->isolate());

  for (uint32_t i = 0; host->h_aliases[i] != nullptr; ++i) {
    Local<String> address = OneByteString(env->isolate(), host->h_aliases[i]);
    names->Set(i, address);
  }

  return scope.Escape(names);
}


class QueryWrap : public AsyncWrap {
 public:
  QueryWrap(Environment* env, Local<Object> req_wrap_obj)
      : AsyncWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_QUERYWRAP) {
    if (env->in_domain())
      req_wrap_obj->Set(env->domain_string(), env->domain_array()->Get(0));
  }

  ~QueryWrap() override {
    CHECK_EQ(false, persistent().IsEmpty());
    ClearWrap(object());
    persistent().Reset();
  }

  // Subclasses should implement the appropriate Send method.
  virtual int Send(const char* name) {
    UNREACHABLE();
    return 0;
  }

  virtual int Send(const char* name, int family) {
    UNREACHABLE();
    return 0;
  }

 protected:
  void* GetQueryArg() {
    return static_cast<void*>(this);
  }

  static void Callback(void *arg, int status, int timeouts,
      unsigned char* answer_buf, int answer_len) {
    QueryWrap* wrap = static_cast<QueryWrap*>(arg);

    if (status != ARES_SUCCESS) {
      wrap->ParseError(status);
    } else {
      wrap->Parse(answer_buf, answer_len);
    }

    delete wrap;
  }

  static void Callback(void *arg, int status, int timeouts,
      struct hostent* host) {
    QueryWrap* wrap = static_cast<QueryWrap*>(arg);

    if (status != ARES_SUCCESS) {
      wrap->ParseError(status);
    } else {
      wrap->Parse(host);
    }

    delete wrap;
  }

  void CallOnComplete(Local<Value> answer,
                      Local<Value> extra = Local<Value>()) {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    Local<Value> argv[] = {
      Integer::New(env()->isolate(), 0),
      answer,
      extra
    };
    const int argc = arraysize(argv) - extra.IsEmpty();
    MakeCallback(env()->oncomplete_string(), argc, argv);
  }

  void ParseError(int status) {
    CHECK_NE(status, ARES_SUCCESS);
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    const char* code = ToErrorCodeString(status);
    Local<Value> arg = OneByteString(env()->isolate(), code);
    MakeCallback(env()->oncomplete_string(), 1, &arg);
  }

  // Subclasses should implement the appropriate Parse method.
  virtual void Parse(unsigned char* buf, int len) {
    UNREACHABLE();
  }

  virtual void Parse(struct hostent* host) {
    UNREACHABLE();
  }
};


class QueryAWrap: public QueryWrap {
 public:
  QueryAWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_a,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    hostent* host;
    ares_addrttl addrttls[256];
    int naddrttls = arraysize(addrttls);

    int status = ares_parse_a_reply(buf, len, &host, addrttls, &naddrttls);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(env(), host);
    Local<Array> ttls = Array::New(env()->isolate(), naddrttls);

    auto context = env()->context();
    for (int i = 0; i < naddrttls; i += 1) {
      auto value = Integer::New(env()->isolate(), addrttls[i].ttl);
      ttls->Set(context, i, value).FromJust();
    }
    ares_free_hostent(host);

    CallOnComplete(addresses, ttls);
  }
};


class QueryAaaaWrap: public QueryWrap {
 public:
  QueryAaaaWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_aaaa,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    hostent* host;
    ares_addr6ttl addrttls[256];
    int naddrttls = arraysize(addrttls);

    int status = ares_parse_aaaa_reply(buf, len, &host, addrttls, &naddrttls);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(env(), host);
    Local<Array> ttls = Array::New(env()->isolate(), naddrttls);

    auto context = env()->context();
    for (int i = 0; i < naddrttls; i += 1) {
      auto value = Integer::New(env()->isolate(), addrttls[i].ttl);
      ttls->Set(context, i, value).FromJust();
    }
    ares_free_hostent(host);

    CallOnComplete(addresses, ttls);
  }
};


class QueryCnameWrap: public QueryWrap {
 public:
  QueryCnameWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_cname,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    struct hostent* host;

    int status = ares_parse_a_reply(buf, len, &host, nullptr, nullptr);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    // A cname lookup always returns a single record but we follow the
    // common API here.
    Local<Array> result = Array::New(env()->isolate(), 1);
    result->Set(0, OneByteString(env()->isolate(), host->h_name));
    ares_free_hostent(host);

    this->CallOnComplete(result);
  }
};


class QueryMxWrap: public QueryWrap {
 public:
  QueryMxWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_mx,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    struct ares_mx_reply* mx_start;
    int status = ares_parse_mx_reply(buf, len, &mx_start);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> mx_records = Array::New(env()->isolate());
    Local<String> exchange_symbol = env()->exchange_string();
    Local<String> priority_symbol = env()->priority_string();

    ares_mx_reply* current = mx_start;
    for (uint32_t i = 0; current != nullptr; ++i, current = current->next) {
      Local<Object> mx_record = Object::New(env()->isolate());
      mx_record->Set(exchange_symbol,
                     OneByteString(env()->isolate(), current->host));
      mx_record->Set(priority_symbol,
                     Integer::New(env()->isolate(), current->priority));
      mx_records->Set(i, mx_record);
    }

    ares_free_data(mx_start);

    this->CallOnComplete(mx_records);
  }
};


class QueryNsWrap: public QueryWrap {
 public:
  QueryNsWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_ns,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    struct hostent* host;

    int status = ares_parse_ns_reply(buf, len, &host);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> names = HostentToNames(env(), host);
    ares_free_hostent(host);

    this->CallOnComplete(names);
  }
};


class QueryTxtWrap: public QueryWrap {
 public:
  QueryTxtWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_txt,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    struct ares_txt_ext* txt_out;

    int status = ares_parse_txt_reply_ext(buf, len, &txt_out);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> txt_records = Array::New(env()->isolate());
    Local<Array> txt_chunk;

    struct ares_txt_ext* current = txt_out;
    uint32_t i = 0;
    for (uint32_t j = 0; current != nullptr; current = current->next) {
      Local<String> txt = OneByteString(env()->isolate(), current->txt);
      // New record found - write out the current chunk
      if (current->record_start) {
        if (!txt_chunk.IsEmpty())
          txt_records->Set(i++, txt_chunk);
        txt_chunk = Array::New(env()->isolate());
        j = 0;
      }
      txt_chunk->Set(j++, txt);
    }
    // Push last chunk if it isn't empty
    if (!txt_chunk.IsEmpty())
      txt_records->Set(i, txt_chunk);

    ares_free_data(txt_out);

    this->CallOnComplete(txt_records);
  }
};


class QuerySrvWrap: public QueryWrap {
 public:
  explicit QuerySrvWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_srv,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    struct ares_srv_reply* srv_start;
    int status = ares_parse_srv_reply(buf, len, &srv_start);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> srv_records = Array::New(env()->isolate());
    Local<String> name_symbol = env()->name_string();
    Local<String> port_symbol = env()->port_string();
    Local<String> priority_symbol = env()->priority_string();
    Local<String> weight_symbol = env()->weight_string();

    ares_srv_reply* current = srv_start;
    for (uint32_t i = 0; current != nullptr; ++i, current = current->next) {
      Local<Object> srv_record = Object::New(env()->isolate());
      srv_record->Set(name_symbol,
                      OneByteString(env()->isolate(), current->host));
      srv_record->Set(port_symbol,
                      Integer::New(env()->isolate(), current->port));
      srv_record->Set(priority_symbol,
                      Integer::New(env()->isolate(), current->priority));
      srv_record->Set(weight_symbol,
                      Integer::New(env()->isolate(), current->weight));
      srv_records->Set(i, srv_record);
    }

    ares_free_data(srv_start);

    this->CallOnComplete(srv_records);
  }
};

class QueryPtrWrap: public QueryWrap {
 public:
  explicit QueryPtrWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_ptr,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    struct hostent* host;

    int status = ares_parse_ptr_reply(buf, len, NULL, 0, AF_INET, &host);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> aliases = Array::New(env()->isolate());

    for (uint32_t i = 0; host->h_aliases[i] != NULL; i++) {
      aliases->Set(i, OneByteString(env()->isolate(), host->h_aliases[i]));
    }

    ares_free_hostent(host);

    this->CallOnComplete(aliases);
  }
};

class QueryNaptrWrap: public QueryWrap {
 public:
  explicit QueryNaptrWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_naptr,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    ares_naptr_reply* naptr_start;
    int status = ares_parse_naptr_reply(buf, len, &naptr_start);

    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> naptr_records = Array::New(env()->isolate());
    Local<String> flags_symbol = env()->flags_string();
    Local<String> service_symbol = env()->service_string();
    Local<String> regexp_symbol = env()->regexp_string();
    Local<String> replacement_symbol = env()->replacement_string();
    Local<String> order_symbol = env()->order_string();
    Local<String> preference_symbol = env()->preference_string();

    ares_naptr_reply* current = naptr_start;
    for (uint32_t i = 0; current != nullptr; ++i, current = current->next) {
      Local<Object> naptr_record = Object::New(env()->isolate());
      naptr_record->Set(flags_symbol,
                        OneByteString(env()->isolate(), current->flags));
      naptr_record->Set(service_symbol,
                        OneByteString(env()->isolate(), current->service));
      naptr_record->Set(regexp_symbol,
                        OneByteString(env()->isolate(), current->regexp));
      naptr_record->Set(replacement_symbol,
                        OneByteString(env()->isolate(), current->replacement));
      naptr_record->Set(order_symbol,
                        Integer::New(env()->isolate(), current->order));
      naptr_record->Set(preference_symbol,
                        Integer::New(env()->isolate(), current->preference));
      naptr_records->Set(i, naptr_record);
    }

    ares_free_data(naptr_start);

    this->CallOnComplete(naptr_records);
  }
};


class QuerySoaWrap: public QueryWrap {
 public:
  QuerySoaWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_soa,
               Callback,
               GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(unsigned char* buf, int len) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    ares_soa_reply* soa_out;
    int status = ares_parse_soa_reply(buf, len, &soa_out);

    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Object> soa_record = Object::New(env()->isolate());

    soa_record->Set(env()->nsname_string(),
                    OneByteString(env()->isolate(), soa_out->nsname));
    soa_record->Set(env()->hostmaster_string(),
                    OneByteString(env()->isolate(), soa_out->hostmaster));
    soa_record->Set(env()->serial_string(),
                    Integer::New(env()->isolate(), soa_out->serial));
    soa_record->Set(env()->refresh_string(),
                    Integer::New(env()->isolate(), soa_out->refresh));
    soa_record->Set(env()->retry_string(),
                    Integer::New(env()->isolate(), soa_out->retry));
    soa_record->Set(env()->expire_string(),
                    Integer::New(env()->isolate(), soa_out->expire));
    soa_record->Set(env()->minttl_string(),
                    Integer::New(env()->isolate(), soa_out->minttl));

    ares_free_data(soa_out);

    this->CallOnComplete(soa_record);
  }
};


class GetHostByAddrWrap: public QueryWrap {
 public:
  explicit GetHostByAddrWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) override {
    int length, family;
    char address_buffer[sizeof(struct in6_addr)];

    if (uv_inet_pton(AF_INET, name, &address_buffer) == 0) {
      length = sizeof(struct in_addr);
      family = AF_INET;
    } else if (uv_inet_pton(AF_INET6, name, &address_buffer) == 0) {
      length = sizeof(struct in6_addr);
      family = AF_INET6;
    } else {
      return UV_EINVAL;  // So errnoException() reports a proper error.
    }

    ares_gethostbyaddr(env()->cares_channel(),
                       address_buffer,
                       length,
                       family,
                       Callback,
                       GetQueryArg());
    return 0;
  }

  size_t self_size() const override { return sizeof(*this); }

 protected:
  void Parse(struct hostent* host) override {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());
    this->CallOnComplete(HostentToNames(env(), host));
  }
};


class GetHostByNameWrap: public QueryWrap {
 public:
  explicit GetHostByNameWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name, int family) override {
    ares_gethostbyname(env()->cares_channel(),
                       name,
                       family,
                       Callback,
                       GetQueryArg());
    return 0;
  }

 protected:
  void Parse(struct hostent* host) override {
    HandleScope scope(env()->isolate());

    Local<Array> addresses = HostentToAddresses(env(), host);
    Local<Integer> family = Integer::New(env()->isolate(), host->h_addrtype);

    this->CallOnComplete(addresses, family);
  }
};


template <class Wrap>
static void Query(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_EQ(false, args.IsConstructCall());
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<String> string = args[1].As<String>();
  Wrap* wrap = new Wrap(env, req_wrap_obj);

  node::Utf8Value name(env->isolate(), string);
  int err = wrap->Send(*name);
  if (err)
    delete wrap;

  args.GetReturnValue().Set(err);
}


void AfterGetAddrInfo(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  GetAddrInfoReqWrap* req_wrap = static_cast<GetAddrInfoReqWrap*>(req->data);
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    Null(env->isolate())
  };

  if (status == 0) {
    // Success
    struct addrinfo *address;
    int n = 0;

    // Create the response array.
    Local<Array> results = Array::New(env->isolate());

    char ip[INET6_ADDRSTRLEN];
    const char *addr;

    // Iterate over the IPv4 responses again this time creating javascript
    // strings for each IP and filling the results array.
    address = res;
    while (address) {
      CHECK_EQ(address->ai_socktype, SOCK_STREAM);

      // Ignore random ai_family types.
      if (address->ai_family == AF_INET) {
        // Juggle pointers
        addr = reinterpret_cast<char*>(&(reinterpret_cast<struct sockaddr_in*>(
            address->ai_addr)->sin_addr));
        int err = uv_inet_ntop(address->ai_family,
                               addr,
                               ip,
                               INET6_ADDRSTRLEN);
        if (err)
          continue;

        // Create JavaScript string
        Local<String> s = OneByteString(env->isolate(), ip);
        results->Set(n, s);
        n++;
      }

      // Increment
      address = address->ai_next;
    }

    // Iterate over the IPv6 responses putting them in the array.
    address = res;
    while (address) {
      CHECK_EQ(address->ai_socktype, SOCK_STREAM);

      // Ignore random ai_family types.
      if (address->ai_family == AF_INET6) {
        // Juggle pointers
        addr = reinterpret_cast<char*>(&(reinterpret_cast<struct sockaddr_in6*>(
            address->ai_addr)->sin6_addr));
        int err = uv_inet_ntop(address->ai_family,
                               addr,
                               ip,
                               INET6_ADDRSTRLEN);
        if (err)
          continue;

        // Create JavaScript string
        Local<String> s = OneByteString(env->isolate(), ip);
        results->Set(n, s);
        n++;
      }

      // Increment
      address = address->ai_next;
    }

    // No responses were found to return
    if (n == 0) {
      argv[0] = Integer::New(env->isolate(), UV_EAI_NODATA);
    }

    argv[1] = results;
  }

  uv_freeaddrinfo(res);

  // Make the callback into JavaScript
  req_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);

  delete req_wrap;
}


void AfterGetNameInfo(uv_getnameinfo_t* req,
                      int status,
                      const char* hostname,
                      const char* service) {
  GetNameInfoReqWrap* req_wrap = static_cast<GetNameInfoReqWrap*>(req->data);
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {
    Integer::New(env->isolate(), status),
    Null(env->isolate()),
    Null(env->isolate())
  };

  if (status == 0) {
    // Success
    Local<String> js_hostname = OneByteString(env->isolate(), hostname);
    Local<String> js_service = OneByteString(env->isolate(), service);
    argv[1] = js_hostname;
    argv[2] = js_service;
  }

  // Make the callback into JavaScript
  req_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);

  delete req_wrap;
}


static void IsIP(const FunctionCallbackInfo<Value>& args) {
  node::Utf8Value ip(args.GetIsolate(), args[0]);
  char address_buffer[sizeof(struct in6_addr)];

  int rc = 0;
  if (uv_inet_pton(AF_INET, *ip, &address_buffer) == 0)
    rc = 4;
  else if (uv_inet_pton(AF_INET6, *ip, &address_buffer) == 0)
    rc = 6;

  args.GetReturnValue().Set(rc);
}

static void IsIPv4(const FunctionCallbackInfo<Value>& args) {
  node::Utf8Value ip(args.GetIsolate(), args[0]);
  char address_buffer[sizeof(struct in_addr)];

  if (uv_inet_pton(AF_INET, *ip, &address_buffer) == 0) {
    args.GetReturnValue().Set(true);
  } else {
    args.GetReturnValue().Set(false);
  }
}

static void IsIPv6(const FunctionCallbackInfo<Value>& args) {
  node::Utf8Value ip(args.GetIsolate(), args[0]);
  char address_buffer[sizeof(struct in6_addr)];

  if (uv_inet_pton(AF_INET6, *ip, &address_buffer) == 0) {
    args.GetReturnValue().Set(true);
  } else {
    args.GetReturnValue().Set(false);
  }
}

static void GetAddrInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsInt32());
  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value hostname(env->isolate(), args[1]);

  int32_t flags = (args[3]->IsInt32()) ? args[3]->Int32Value() : 0;
  int family;

  switch (args[2]->Int32Value()) {
  case 0:
    family = AF_UNSPEC;
    break;
  case 4:
    family = AF_INET;
    break;
  case 6:
    family = AF_INET6;
    break;
  default:
    CHECK(0 && "bad address family");
  }

  GetAddrInfoReqWrap* req_wrap = new GetAddrInfoReqWrap(env, req_wrap_obj);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = flags;

  int err = uv_getaddrinfo(env->event_loop(),
                           req_wrap->req(),
                           AfterGetAddrInfo,
                           *hostname,
                           nullptr,
                           &hints);
  req_wrap->Dispatched();
  if (err)
    delete req_wrap;

  args.GetReturnValue().Set(err);
}


static void GetNameInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());
  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip(env->isolate(), args[1]);
  const unsigned port = args[2]->Uint32Value();
  struct sockaddr_storage addr;

  CHECK(uv_ip4_addr(*ip, port, reinterpret_cast<sockaddr_in*>(&addr)) == 0 ||
        uv_ip6_addr(*ip, port, reinterpret_cast<sockaddr_in6*>(&addr)) == 0);

  GetNameInfoReqWrap* req_wrap = new GetNameInfoReqWrap(env, req_wrap_obj);

  int err = uv_getnameinfo(env->event_loop(),
                           req_wrap->req(),
                           AfterGetNameInfo,
                           (struct sockaddr*)&addr,
                           NI_NAMEREQD);
  req_wrap->Dispatched();
  if (err)
    delete req_wrap;

  args.GetReturnValue().Set(err);
}


static void GetServers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Array> server_array = Array::New(env->isolate());

  ares_addr_node* servers;

  int r = ares_get_servers(env->cares_channel(), &servers);
  CHECK_EQ(r, ARES_SUCCESS);

  ares_addr_node* cur = servers;

  for (uint32_t i = 0; cur != nullptr; ++i, cur = cur->next) {
    char ip[INET6_ADDRSTRLEN];

    const void* caddr = static_cast<const void*>(&cur->addr);
    int err = uv_inet_ntop(cur->family, caddr, ip, sizeof(ip));
    CHECK_EQ(err, 0);

    Local<String> addr = OneByteString(env->isolate(), ip);
    server_array->Set(i, addr);
  }

  ares_free_data(servers);

  args.GetReturnValue().Set(server_array);
}


static void SetServers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsArray());

  Local<Array> arr = Local<Array>::Cast(args[0]);

  uint32_t len = arr->Length();

  if (len == 0) {
    int rv = ares_set_servers(env->cares_channel(), nullptr);
    return args.GetReturnValue().Set(rv);
  }

  ares_addr_node* servers = new ares_addr_node[len];
  ares_addr_node* last = nullptr;

  int err;

  for (uint32_t i = 0; i < len; i++) {
    CHECK(arr->Get(i)->IsArray());

    Local<Array> elm = Local<Array>::Cast(arr->Get(i));

    CHECK(elm->Get(0)->Int32Value());
    CHECK(elm->Get(1)->IsString());

    int fam = elm->Get(0)->Int32Value();
    node::Utf8Value ip(env->isolate(), elm->Get(1));

    ares_addr_node* cur = &servers[i];

    switch (fam) {
      case 4:
        cur->family = AF_INET;
        err = uv_inet_pton(AF_INET, *ip, &cur->addr);
        break;
      case 6:
        cur->family = AF_INET6;
        err = uv_inet_pton(AF_INET6, *ip, &cur->addr);
        break;
      default:
        CHECK(0 && "Bad address family.");
    }

    if (err)
      break;

    cur->next = nullptr;

    if (last != nullptr)
      last->next = cur;

    last = cur;
  }

  if (err == 0)
    err = ares_set_servers(env->cares_channel(), &servers[0]);
  else
    err = ARES_EBADSTR;

  delete[] servers;

  args.GetReturnValue().Set(err);
}


static void StrError(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  const char* errmsg = ares_strerror(args[0]->Int32Value());
  args.GetReturnValue().Set(OneByteString(env->isolate(), errmsg));
}


static void CaresTimerCloseCb(uv_handle_t* handle) {
  Environment* env = Environment::from_cares_timer_handle(
      reinterpret_cast<uv_timer_t*>(handle));
  env->FinishHandleCleanup(handle);
}


static void CaresTimerClose(Environment* env,
                            uv_handle_t* handle,
                            void* arg) {
  uv_close(handle, CaresTimerCloseCb);
}


static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  int r = ares_library_init(ARES_LIB_INIT_ALL);
  if (r != ARES_SUCCESS)
    return env->ThrowError(ToErrorCodeString(r));

  struct ares_options options;
  memset(&options, 0, sizeof(options));
  options.flags = ARES_FLAG_NOCHECKRESP;
  options.sock_state_cb = ares_sockstate_cb;
  options.sock_state_cb_data = env;

  /* We do the call to ares_init_option for caller. */
  r = ares_init_options(env->cares_channel_ptr(),
                        &options,
                        ARES_OPT_FLAGS | ARES_OPT_SOCK_STATE_CB);
  if (r != ARES_SUCCESS) {
    ares_library_cleanup();
    return env->ThrowError(ToErrorCodeString(r));
  }

  /* Initialize the timeout timer. The timer won't be started until the */
  /* first socket is opened. */
  uv_timer_init(env->event_loop(), env->cares_timer_handle());
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->cares_timer_handle()),
      CaresTimerClose,
      nullptr);

  env->SetMethod(target, "queryA", Query<QueryAWrap>);
  env->SetMethod(target, "queryAaaa", Query<QueryAaaaWrap>);
  env->SetMethod(target, "queryCname", Query<QueryCnameWrap>);
  env->SetMethod(target, "queryMx", Query<QueryMxWrap>);
  env->SetMethod(target, "queryNs", Query<QueryNsWrap>);
  env->SetMethod(target, "queryTxt", Query<QueryTxtWrap>);
  env->SetMethod(target, "querySrv", Query<QuerySrvWrap>);
  env->SetMethod(target, "queryPtr", Query<QueryPtrWrap>);
  env->SetMethod(target, "queryNaptr", Query<QueryNaptrWrap>);
  env->SetMethod(target, "querySoa", Query<QuerySoaWrap>);
  env->SetMethod(target, "getHostByAddr", Query<GetHostByAddrWrap>);

  env->SetMethod(target, "getaddrinfo", GetAddrInfo);
  env->SetMethod(target, "getnameinfo", GetNameInfo);
  env->SetMethod(target, "isIP", IsIP);
  env->SetMethod(target, "isIPv4", IsIPv4);
  env->SetMethod(target, "isIPv6", IsIPv6);

  env->SetMethod(target, "strerror", StrError);
  env->SetMethod(target, "getServers", GetServers);
  env->SetMethod(target, "setServers", SetServers);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "AF_INET"),
              Integer::New(env->isolate(), AF_INET));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "AF_INET6"),
              Integer::New(env->isolate(), AF_INET6));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "AF_UNSPEC"),
              Integer::New(env->isolate(), AF_UNSPEC));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "AI_ADDRCONFIG"),
              Integer::New(env->isolate(), AI_ADDRCONFIG));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "AI_V4MAPPED"),
              Integer::New(env->isolate(), AI_V4MAPPED));

  Local<FunctionTemplate> aiw =
      FunctionTemplate::New(env->isolate(), NewGetAddrInfoReqWrap);
  aiw->InstanceTemplate()->SetInternalFieldCount(1);
  aiw->SetClassName(
      FIXED_ONE_BYTE_STRING(env->isolate(), "GetAddrInfoReqWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "GetAddrInfoReqWrap"),
              aiw->GetFunction());

  Local<FunctionTemplate> niw =
      FunctionTemplate::New(env->isolate(), NewGetNameInfoReqWrap);
  niw->InstanceTemplate()->SetInternalFieldCount(1);
  niw->SetClassName(
      FIXED_ONE_BYTE_STRING(env->isolate(), "GetNameInfoReqWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "GetNameInfoReqWrap"),
              niw->GetFunction());

  Local<FunctionTemplate> qrw =
      FunctionTemplate::New(env->isolate(), NewQueryReqWrap);
  qrw->InstanceTemplate()->SetInternalFieldCount(1);
  qrw->SetClassName(
      FIXED_ONE_BYTE_STRING(env->isolate(), "QueryReqWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "QueryReqWrap"),
              qrw->GetFunction());
}

}  // namespace cares_wrap
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(cares_wrap, node::cares_wrap::Initialize)
