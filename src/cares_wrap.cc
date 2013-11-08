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
#include "req_wrap.h"
#include "tree.h"
#include "uv.h"

#include <assert.h>
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


namespace node {
namespace cares_wrap {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;

typedef class ReqWrap<uv_getaddrinfo_t> GetAddrInfoReqWrap;


static int cmp_ares_tasks(const ares_task_t* a, const ares_task_t* b) {
  if (a->sock < b->sock)
    return -1;
  if (a->sock > b->sock)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(ares_task_list, ares_task_t, node, cmp_ares_tasks)



/* This is called once per second by loop->timer. It is used to constantly */
/* call back into c-ares for possibly processing timeouts. */
static void ares_timeout(uv_timer_t* handle, int status) {
  Environment* env = Environment::from_cares_timer_handle(handle);
  assert(!RB_EMPTY(env->cares_task_list()));
  ares_process_fd(env->cares_channel(), ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void ares_poll_cb(uv_poll_t* watcher, int status, int events) {
  ares_task_t* task = CONTAINER_OF(watcher, ares_task_t, poll_watcher);
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
  ares_task_t* task = CONTAINER_OF(watcher, ares_task_t, poll_watcher);
  free(task);
}


/* Allocates and returns a new ares_task_t */
static ares_task_t* ares_task_create(Environment* env, ares_socket_t sock) {
  ares_task_t* task = static_cast<ares_task_t*>(malloc(sizeof(*task)));

  if (task == NULL) {
    /* Out of memory. */
    return NULL;
  }

  task->env = env;
  task->sock = sock;

  if (uv_poll_init_socket(env->event_loop(), &task->poll_watcher, sock) < 0) {
    /* This should never happen. */
    free(task);
    return NULL;
  }

  return task;
}


/* Callback from ares when socket operation is started */
static void ares_sockstate_cb(void* data,
                              ares_socket_t sock,
                              int read,
                              int write) {
  Environment* env = static_cast<Environment*>(data);
  ares_task_t* task;

  ares_task_t lookup_task;
  lookup_task.sock = sock;
  task = RB_FIND(ares_task_list, env->cares_task_list(), &lookup_task);

  if (read || write) {
    if (!task) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      uv_timer_t* timer_handle = env->cares_timer_handle();
      if (!uv_is_active(reinterpret_cast<uv_handle_t*>(timer_handle))) {
        assert(RB_EMPTY(env->cares_task_list()));
        uv_timer_start(timer_handle, ares_timeout, 1000, 1000);
      }

      task = ares_task_create(env, sock);
      if (task == NULL) {
        /* This should never happen unless we're out of memory or something */
        /* is seriously wrong. The socket won't be polled, but the the query */
        /* will eventually time out. */
        return;
      }

      RB_INSERT(ares_task_list, env->cares_task_list(), task);
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
    assert(task &&
           "When an ares socket is closed we should have a handle for it");

    RB_REMOVE(ares_task_list, env->cares_task_list(), task);
    uv_close(reinterpret_cast<uv_handle_t*>(&task->poll_watcher),
             ares_poll_close_cb);

    if (RB_EMPTY(env->cares_task_list())) {
      uv_timer_stop(env->cares_timer_handle());
    }
  }
}


static Local<Array> HostentToAddresses(struct hostent* host) {
  HandleScope scope(node_isolate);
  Local<Array> addresses = Array::New();

  char ip[INET6_ADDRSTRLEN];
  for (uint32_t i = 0; host->h_addr_list[i] != NULL; ++i) {
    uv_inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));
    Local<String> address = OneByteString(node_isolate, ip);
    addresses->Set(i, address);
  }

  return scope.Close(addresses);
}


static Local<Array> HostentToNames(struct hostent* host) {
  HandleScope scope(node_isolate);
  Local<Array> names = Array::New();

  for (uint32_t i = 0; host->h_aliases[i] != NULL; ++i) {
    Local<String> address = OneByteString(node_isolate, host->h_aliases[i]);
    names->Set(i, address);
  }

  return scope.Close(names);
}


class QueryWrap : public AsyncWrap {
 public:
  QueryWrap(Environment* env, Local<Object> req_wrap_obj)
      : AsyncWrap(env, req_wrap_obj) {
  }

  virtual ~QueryWrap() {
    assert(!persistent().IsEmpty());
    persistent().Dispose();
  }

  // Subclasses should implement the appropriate Send method.
  virtual int Send(const char* name) {
    assert(0);
    return 0;
  }

  virtual int Send(const char* name, int family) {
    assert(0);
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

  void CallOnComplete(Local<Value> answer) {
    Context::Scope context_scope(env()->context());
    HandleScope handle_scope(env()->isolate());
    Local<Value> argv[] = {
      Integer::New(0, env()->isolate()),
      answer
    };
    MakeCallback(env()->oncomplete_string(), ARRAY_SIZE(argv), argv);
  }

  void CallOnComplete(Local<Value> answer, Local<Value> family) {
    Context::Scope context_scope(env()->context());
    HandleScope handle_scope(env()->isolate());
    Local<Value> argv[] = {
      Integer::New(0, env()->isolate()),
      answer,
      family
    };
    MakeCallback(env()->oncomplete_string(), ARRAY_SIZE(argv), argv);
  }

  void ParseError(int status) {
    assert(status != ARES_SUCCESS);
    Context::Scope context_scope(env()->context());
    HandleScope handle_scope(env()->isolate());
    Local<Value> arg;
    switch (status) {
#define V(code)                                                               \
      case ARES_ ## code:                                                     \
        arg = FIXED_ONE_BYTE_STRING(env()->isolate(), #code);                 \
        break;
      V(ENODATA)
      V(EFORMERR)
      V(ESERVFAIL)
      V(ENOTFOUND)
      V(ENOTIMP)
      V(EREFUSED)
      V(EBADQUERY)
      V(EBADNAME)
      V(EBADFAMILY)
      V(EBADRESP)
      V(ECONNREFUSED)
      V(ETIMEOUT)
      V(EOF)
      V(EFILE)
      V(ENOMEM)
      V(EDESTRUCTION)
      V(EBADSTR)
      V(EBADFLAGS)
      V(ENONAME)
      V(EBADHINTS)
      V(ENOTINITIALIZED)
      V(ELOADIPHLPAPI)
      V(EADDRGETNETWORKPARAMS)
      V(ECANCELLED)
#undef V
      default:
        arg = FIXED_ONE_BYTE_STRING(env()->isolate(), "UNKNOWN_ARES_ERROR");
        break;
    }
    MakeCallback(env()->oncomplete_string(), 1, &arg);
  }

  // Subclasses should implement the appropriate Parse method.
  virtual void Parse(unsigned char* buf, int len) {
    assert(0);
  };

  virtual void Parse(struct hostent* host) {
    assert(0);
  };
};


class QueryAWrap: public QueryWrap {
 public:
  QueryAWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_a,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    struct hostent* host;

    int status = ares_parse_a_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(host);
    ares_free_hostent(host);

    this->CallOnComplete(addresses);
  }
};


class QueryAaaaWrap: public QueryWrap {
 public:
  QueryAaaaWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_aaaa,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    struct hostent* host;

    int status = ares_parse_aaaa_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(host);
    ares_free_hostent(host);

    this->CallOnComplete(addresses);
  }
};


class QueryCnameWrap: public QueryWrap {
 public:
  QueryCnameWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_cname,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    struct hostent* host;

    int status = ares_parse_a_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    // A cname lookup always returns a single record but we follow the
    // common API here.
    Local<Array> result = Array::New(1);
    result->Set(0, OneByteString(node_isolate, host->h_name));
    ares_free_hostent(host);

    this->CallOnComplete(result);
  }
};


class QueryMxWrap: public QueryWrap {
 public:
  QueryMxWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_mx,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    struct ares_mx_reply* mx_start;
    int status = ares_parse_mx_reply(buf, len, &mx_start);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> mx_records = Array::New();
    Local<String> exchange_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "exchange");
    Local<String> priority_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "priority");

    ares_mx_reply* current = mx_start;
    for (uint32_t i = 0; current != NULL; ++i, current = current->next) {
      Local<Object> mx_record = Object::New();
      mx_record->Set(exchange_symbol,
                     OneByteString(node_isolate, current->host));
      mx_record->Set(priority_symbol,
                     Integer::New(current->priority, node_isolate));
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

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_ns,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    struct hostent* host;

    int status = ares_parse_ns_reply(buf, len, &host);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> names = HostentToNames(host);
    ares_free_hostent(host);

    this->CallOnComplete(names);
  }
};


class QueryTxtWrap: public QueryWrap {
 public:
  QueryTxtWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_txt,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    struct ares_txt_reply* txt_out;

    int status = ares_parse_txt_reply(buf, len, &txt_out);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> txt_records = Array::New();

    ares_txt_reply* current = txt_out;
    for (uint32_t i = 0; current != NULL; ++i, current = current->next) {
      Local<String> txt = OneByteString(node_isolate, current->txt);
      txt_records->Set(i, txt);
    }

    ares_free_data(txt_out);

    this->CallOnComplete(txt_records);
  }
};


class QuerySrvWrap: public QueryWrap {
 public:
  explicit QuerySrvWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_srv,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    struct ares_srv_reply* srv_start;
    int status = ares_parse_srv_reply(buf, len, &srv_start);
    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> srv_records = Array::New();
    Local<String> name_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "name");
    Local<String> port_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "port");
    Local<String> priority_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "priority");
    Local<String> weight_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "weight");

    ares_srv_reply* current = srv_start;
    for (uint32_t i = 0; current != NULL; ++i, current = current->next) {
      Local<Object> srv_record = Object::New();
      srv_record->Set(name_symbol,
                      OneByteString(node_isolate, current->host));
      srv_record->Set(port_symbol,
                      Integer::New(current->port, node_isolate));
      srv_record->Set(priority_symbol,
                      Integer::New(current->priority, node_isolate));
      srv_record->Set(weight_symbol,
                      Integer::New(current->weight, node_isolate));
      srv_records->Set(i, srv_record);
    }

    ares_free_data(srv_start);

    this->CallOnComplete(srv_records);
  }
};

class QueryNaptrWrap: public QueryWrap {
 public:
  explicit QueryNaptrWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
    ares_query(env()->cares_channel(),
               name,
               ns_c_in,
               ns_t_naptr,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope(node_isolate);

    ares_naptr_reply* naptr_start;
    int status = ares_parse_naptr_reply(buf, len, &naptr_start);

    if (status != ARES_SUCCESS) {
      ParseError(status);
      return;
    }

    Local<Array> naptr_records = Array::New();
    Local<String> flags_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "flags");
    Local<String> service_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "service");
    Local<String> regexp_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "regexp");
    Local<String> replacement_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "replacement");
    Local<String> order_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "order");
    Local<String> preference_symbol =
        FIXED_ONE_BYTE_STRING(node_isolate, "preference");

    ares_naptr_reply* current = naptr_start;
    for (uint32_t i = 0; current != NULL; ++i, current = current->next) {
      Local<Object> naptr_record = Object::New();
      naptr_record->Set(flags_symbol,
                        OneByteString(node_isolate, current->flags));
      naptr_record->Set(service_symbol,
                        OneByteString(node_isolate, current->service));
      naptr_record->Set(regexp_symbol,
                        OneByteString(node_isolate, current->regexp));
      naptr_record->Set(replacement_symbol,
                        OneByteString(node_isolate, current->replacement));
      naptr_record->Set(order_symbol,
                        Integer::New(current->order, node_isolate));
      naptr_record->Set(preference_symbol,
                        Integer::New(current->preference, node_isolate));
      naptr_records->Set(i, naptr_record);
    }

    ares_free_data(naptr_start);

    this->CallOnComplete(naptr_records);
  }
};


class GetHostByAddrWrap: public QueryWrap {
 public:
  explicit GetHostByAddrWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name) {
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

 protected:
  void Parse(struct hostent* host) {
    HandleScope scope(node_isolate);

    this->CallOnComplete(HostentToNames(host));
  }
};


class GetHostByNameWrap: public QueryWrap {
 public:
  explicit GetHostByNameWrap(Environment* env, Local<Object> req_wrap_obj)
      : QueryWrap(env, req_wrap_obj) {
  }

  int Send(const char* name, int family) {
    ares_gethostbyname(env()->cares_channel(),
                       name,
                       family,
                       Callback,
                       GetQueryArg());
    return 0;
  }

 protected:
  void Parse(struct hostent* host) {
    HandleScope scope(node_isolate);

    Local<Array> addresses = HostentToAddresses(host);
    Local<Integer> family = Integer::New(host->h_addrtype, node_isolate);

    this->CallOnComplete(addresses, family);
  }
};


template <class Wrap>
static void Query(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  assert(!args.IsConstructCall());
  assert(args[0]->IsObject());
  assert(args[1]->IsString());

  Local<Object> req_wrap_obj = args[0].As<Object>();
  Local<String> string = args[1].As<String>();
  Wrap* wrap = new Wrap(env, req_wrap_obj);

  String::Utf8Value name(string);
  int err = wrap->Send(*name);
  if (err)
    delete wrap;

  args.GetReturnValue().Set(err);
}


void AfterGetAddrInfo(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  GetAddrInfoReqWrap* req_wrap = static_cast<GetAddrInfoReqWrap*>(req->data);
  Environment* env = req_wrap->env();

  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  Local<Value> argv[] = {
    Integer::New(status, node_isolate),
    Null(node_isolate)
  };

  if (status == 0) {
    // Success
    struct addrinfo *address;
    int n = 0;

    // Count the number of responses.
    for (address = res; address; address = address->ai_next) {
      n++;
    }

    // Create the response array.
    Local<Array> results = Array::New(n);

    char ip[INET6_ADDRSTRLEN];
    const char *addr;

    n = 0;

    // Iterate over the IPv4 responses again this time creating javascript
    // strings for each IP and filling the results array.
    address = res;
    while (address) {
      assert(address->ai_socktype == SOCK_STREAM);

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
        Local<String> s = OneByteString(node_isolate, ip);
        results->Set(n, s);
        n++;
      }

      // Increment
      address = address->ai_next;
    }

    // Iterate over the IPv6 responses putting them in the array.
    address = res;
    while (address) {
      assert(address->ai_socktype == SOCK_STREAM);

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
        Local<String> s = OneByteString(node_isolate, ip);
        results->Set(n, s);
        n++;
      }

      // Increment
      address = address->ai_next;
    }


    argv[1] = results;
  }

  uv_freeaddrinfo(res);

  // Make the callback into JavaScript
  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


static void IsIP(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  String::AsciiValue ip(args[0]);
  char address_buffer[sizeof(struct in6_addr)];

  int rc = 0;
  if (uv_inet_pton(AF_INET, *ip, &address_buffer) == 0)
    rc = 4;
  else if (uv_inet_pton(AF_INET6, *ip, &address_buffer) == 0)
    rc = 6;

  args.GetReturnValue().Set(rc);
}


static void GetAddrInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  assert(args[0]->IsObject());
  assert(args[1]->IsString());
  assert(args[2]->IsInt32());
  Local<Object> req_wrap_obj = args[0].As<Object>();
  String::Utf8Value hostname(args[1]);

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
    assert(0 && "bad address family");
    abort();
  }

  GetAddrInfoReqWrap* req_wrap = new GetAddrInfoReqWrap(env, req_wrap_obj);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;

  int err = uv_getaddrinfo(env->event_loop(),
                           &req_wrap->req_,
                           AfterGetAddrInfo,
                           *hostname,
                           NULL,
                           &hints);
  req_wrap->Dispatched();
  if (err)
    delete req_wrap;

  args.GetReturnValue().Set(err);
}


static void GetServers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  Local<Array> server_array = Array::New();

  ares_addr_node* servers;

  int r = ares_get_servers(env->cares_channel(), &servers);
  assert(r == ARES_SUCCESS);

  ares_addr_node* cur = servers;

  for (uint32_t i = 0; cur != NULL; ++i, cur = cur->next) {
    char ip[INET6_ADDRSTRLEN];

    const void* caddr = static_cast<const void*>(&cur->addr);
    int err = uv_inet_ntop(cur->family, caddr, ip, sizeof(ip));
    assert(err == 0);

    Local<String> addr = OneByteString(node_isolate, ip);
    server_array->Set(i, addr);
  }

  ares_free_data(servers);

  args.GetReturnValue().Set(server_array);
}


static void SetServers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  assert(args[0]->IsArray());

  Local<Array> arr = Local<Array>::Cast(args[0]);

  uint32_t len = arr->Length();

  if (len == 0) {
    int rv = ares_set_servers(env->cares_channel(), NULL);
    return args.GetReturnValue().Set(rv);
  }

  ares_addr_node* servers = new ares_addr_node[len];
  ares_addr_node* last = NULL;

  int err;

  for (uint32_t i = 0; i < len; i++) {
    assert(arr->Get(i)->IsArray());

    Local<Array> elm = Local<Array>::Cast(arr->Get(i));

    assert(elm->Get(0)->Int32Value());
    assert(elm->Get(1)->IsString());

    int fam = elm->Get(0)->Int32Value();
    String::Utf8Value ip(elm->Get(1));

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
        assert(0 && "Bad address family.");
        abort();
    }

    if (err)
      break;

    cur->next = NULL;

    if (last != NULL)
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
  HandleScope scope(node_isolate);
  const char* errmsg = ares_strerror(args[0]->Int32Value());
  args.GetReturnValue().Set(OneByteString(node_isolate, errmsg));
}


static void Initialize(Handle<Object> target,
                       Handle<Value> unused,
                       Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  int r = ares_library_init(ARES_LIB_INIT_ALL);
  assert(r == ARES_SUCCESS);

  struct ares_options options;
  memset(&options, 0, sizeof(options));
  options.flags = ARES_FLAG_NOCHECKRESP;
  options.sock_state_cb = ares_sockstate_cb;
  options.sock_state_cb_data = env;

  /* We do the call to ares_init_option for caller. */
  r = ares_init_options(env->cares_channel_ptr(),
                        &options,
                        ARES_OPT_FLAGS | ARES_OPT_SOCK_STATE_CB);
  assert(r == ARES_SUCCESS);

  /* Initialize the timeout timer. The timer won't be started until the */
  /* first socket is opened. */
  uv_timer_init(env->event_loop(), env->cares_timer_handle());

  NODE_SET_METHOD(target, "queryA", Query<QueryAWrap>);
  NODE_SET_METHOD(target, "queryAaaa", Query<QueryAaaaWrap>);
  NODE_SET_METHOD(target, "queryCname", Query<QueryCnameWrap>);
  NODE_SET_METHOD(target, "queryMx", Query<QueryMxWrap>);
  NODE_SET_METHOD(target, "queryNs", Query<QueryNsWrap>);
  NODE_SET_METHOD(target, "queryTxt", Query<QueryTxtWrap>);
  NODE_SET_METHOD(target, "querySrv", Query<QuerySrvWrap>);
  NODE_SET_METHOD(target, "queryNaptr", Query<QueryNaptrWrap>);
  NODE_SET_METHOD(target, "getHostByAddr", Query<GetHostByAddrWrap>);

  NODE_SET_METHOD(target, "getaddrinfo", GetAddrInfo);
  NODE_SET_METHOD(target, "isIP", IsIP);

  NODE_SET_METHOD(target, "strerror", StrError);
  NODE_SET_METHOD(target, "getServers", GetServers);
  NODE_SET_METHOD(target, "setServers", SetServers);

  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "AF_INET"),
              Integer::New(AF_INET, node_isolate));
  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "AF_INET6"),
              Integer::New(AF_INET6, node_isolate));
  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "AF_UNSPEC"),
              Integer::New(AF_UNSPEC, node_isolate));
}

}  // namespace cares_wrap
}  // namespace node

NODE_MODULE_CONTEXT_AWARE(node_cares_wrap, node::cares_wrap::Initialize)
