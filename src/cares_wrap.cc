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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CARES_STATICLIB
#include "ares.h"
#include "node.h"
#include "req_wrap.h"
#include "tree.h"
#include "uv.h"

#if defined(__OpenBSD__) || defined(__MINGW32__) || defined(_MSC_VER)
# include <nameser.h>
#else
# include <arpa/nameser.h>
#endif


namespace node {

namespace cares_wrap {

using v8::Arguments;
using v8::Array;
using v8::Context;
using v8::Function;
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

struct ares_task_t {
  UV_HANDLE_FIELDS
  ares_socket_t sock;
  uv_poll_t poll_watcher;
  RB_ENTRY(ares_task_t) node;
};


static Persistent<String> oncomplete_sym;
static ares_channel ares_channel;
static uv_timer_t ares_timer;
static RB_HEAD(ares_task_list, ares_task_t) ares_tasks;


static int cmp_ares_tasks(const ares_task_t* a, const ares_task_t* b) {
  if (a->sock < b->sock) return -1;
  if (a->sock > b->sock) return 1;
  return 0;
}


RB_GENERATE_STATIC(ares_task_list, ares_task_t, node, cmp_ares_tasks)



/* This is called once per second by loop->timer. It is used to constantly */
/* call back into c-ares for possibly processing timeouts. */
static void ares_timeout(uv_timer_t* handle, int status) {
  assert(!RB_EMPTY(&ares_tasks));
  ares_process_fd(ares_channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void ares_poll_cb(uv_poll_t* watcher, int status, int events) {
  ares_task_t* task = container_of(watcher, ares_task_t, poll_watcher);

  /* Reset the idle timer */
  uv_timer_again(&ares_timer);

  if (status < 0) {
    /* An error happened. Just pretend that the socket is both readable and */
    /* writable. */
    ares_process_fd(ares_channel, task->sock, task->sock);
    return;
  }

  /* Process DNS responses */
  ares_process_fd(ares_channel,
                  events & UV_READABLE ? task->sock : ARES_SOCKET_BAD,
                  events & UV_WRITABLE ? task->sock : ARES_SOCKET_BAD);
}


static void ares_poll_close_cb(uv_handle_t* watcher) {
  ares_task_t* task = container_of(watcher, ares_task_t, poll_watcher);
  free(task);
}


/* Allocates and returns a new ares_task_t */
static ares_task_t* ares_task_create(uv_loop_t* loop, ares_socket_t sock) {
  ares_task_t* task = (ares_task_t*) malloc(sizeof *task);

  if (task == NULL) {
    /* Out of memory. */
    return NULL;
  }

  task->loop = loop;
  task->sock = sock;

  if (uv_poll_init_socket(loop, &task->poll_watcher, sock) < 0) {
    /* This should never happen. */
    free(task);
    return NULL;
  }

  return task;
}


/* Callback from ares when socket operation is started */
static void ares_sockstate_cb(void* data, ares_socket_t sock,
    int read, int write) {
  uv_loop_t* loop = (uv_loop_t*) data;
  ares_task_t* task;

  ares_task_t lookup_task;
  lookup_task.sock = sock;
  task = RB_FIND(ares_task_list, &ares_tasks, &lookup_task);

  if (read || write) {
    if (!task) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      if (!uv_is_active((uv_handle_t*) &ares_timer)) {
        assert(RB_EMPTY(&ares_tasks));
        uv_timer_start(&ares_timer, ares_timeout, 1000, 1000);
      }

      task = ares_task_create(loop, sock);
      if (task == NULL) {
        /* This should never happen unless we're out of memory or something */
        /* is seriously wrong. The socket won't be polled, but the the query */
        /* will eventually time out. */
        return;
      }

      RB_INSERT(ares_task_list, &ares_tasks, task);
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

    RB_REMOVE(ares_task_list, &ares_tasks, task);
    uv_close((uv_handle_t*) &task->poll_watcher, ares_poll_close_cb);

    if (RB_EMPTY(&ares_tasks)) {
      uv_timer_stop(&ares_timer);
    }
  }
}


static Local<Array> HostentToAddresses(struct hostent* host) {
  HandleScope scope;
  Local<Array> addresses = Array::New();

  char ip[INET6_ADDRSTRLEN];
  for (int i = 0; host->h_addr_list[i]; ++i) {
    uv_inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));

    Local<String> address = String::New(ip);
    addresses->Set(Integer::New(i), address);
  }

  return scope.Close(addresses);
}


static Local<Array> HostentToNames(struct hostent* host) {
  HandleScope scope;
  Local<Array> names = Array::New();

  for (int i = 0; host->h_aliases[i]; ++i) {
    Local<String> address = String::New(host->h_aliases[i]);
    names->Set(Integer::New(i), address);
  }

  return scope.Close(names);
}


static const char* AresErrnoString(int errorno) {
  switch (errorno) {
#define ERRNO_CASE(e) case ARES_##e: return #e;
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
#undef ERRNO_CASE
    default:
      assert(0 && "Unhandled c-ares error");
      return "(UNKNOWN)";
  }
}


static void SetAresErrno(int errorno) {
  HandleScope scope;
  Local<Value> key = String::NewSymbol("_errno");
  Local<Value> value = String::NewSymbol(AresErrnoString(errorno));
  node::process->Set(key, value);
}


class QueryWrap {
 public:
  QueryWrap() {
    HandleScope scope;

    object_ = Persistent<Object>::New(Object::New());
  }

  virtual ~QueryWrap() {
    assert(!object_.IsEmpty());

    object_->Delete(oncomplete_sym);

    object_.Dispose();
    object_.Clear();
  }

  Handle<Object> GetObject() {
    return object_;
  }

  void SetOnComplete(Handle<Value> oncomplete) {
    assert(oncomplete->IsFunction());
    object_->Set(oncomplete_sym, oncomplete);
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
    HandleScope scope;
    Local<Value> argv[2] = { Integer::New(0), answer };
    MakeCallback(object_, oncomplete_sym, ARRAY_SIZE(argv), argv);
  }

  void CallOnComplete(Local<Value> answer, Local<Value> family) {
    HandleScope scope;
    Local<Value> argv[3] = { Integer::New(0), answer, family };
    MakeCallback(object_, oncomplete_sym, ARRAY_SIZE(argv), argv);
  }

  void ParseError(int status) {
    assert(status != ARES_SUCCESS);
    SetAresErrno(status);

    HandleScope scope;
    Local<Value> argv[1] = { Integer::New(-1) };
    MakeCallback(object_, oncomplete_sym, ARRAY_SIZE(argv), argv);
  }

  // Subclasses should implement the appropriate Parse method.
  virtual void Parse(unsigned char* buf, int len) {
    assert(0);
  };

  virtual void Parse(struct hostent* host) {
    assert(0);
  };

 private:
  Persistent<Object> object_;
};


class QueryAWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel, name, ns_c_in, ns_t_a, Callback, GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    struct hostent* host;

    int status = ares_parse_a_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(host);
    ares_free_hostent(host);

    this->CallOnComplete(addresses);
  }
};


class QueryAaaaWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel,
               name,
               ns_c_in,
               ns_t_aaaa,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    struct hostent* host;

    int status = ares_parse_aaaa_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> addresses = HostentToAddresses(host);
    ares_free_hostent(host);

    this->CallOnComplete(addresses);
  }
};


class QueryCnameWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel,
               name,
               ns_c_in,
               ns_t_cname,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    struct hostent* host;

    int status = ares_parse_a_reply(buf, len, &host, NULL, NULL);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    // A cname lookup always returns a single record but we follow the
    // common API here.
    Local<Array> result = Array::New(1);
    result->Set(0, String::New(host->h_name));
    ares_free_hostent(host);

    this->CallOnComplete(result);
  }
};


class QueryMxWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel, name, ns_c_in, ns_t_mx, Callback, GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    struct ares_mx_reply* mx_start;
    int status = ares_parse_mx_reply(buf, len, &mx_start);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> mx_records = Array::New();
    Local<String> exchange_symbol = String::NewSymbol("exchange");
    Local<String> priority_symbol = String::NewSymbol("priority");
    int i = 0;
    for (struct ares_mx_reply* mx_current = mx_start;
         mx_current;
         mx_current = mx_current->next) {
      Local<Object> mx_record = Object::New();
      mx_record->Set(exchange_symbol, String::New(mx_current->host));
      mx_record->Set(priority_symbol,
                     Integer::New(mx_current->priority));
      mx_records->Set(Integer::New(i++), mx_record);
    }

    ares_free_data(mx_start);

    this->CallOnComplete(mx_records);
  }
};


class QueryNsWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel, name, ns_c_in, ns_t_ns, Callback, GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    struct hostent* host;

    int status = ares_parse_ns_reply(buf, len, &host);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> names = HostentToNames(host);
    ares_free_hostent(host);

    this->CallOnComplete(names);
  }
};


class QueryTxtWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel, name, ns_c_in, ns_t_txt, Callback, GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    struct ares_txt_reply* txt_out;

    int status = ares_parse_txt_reply(buf, len, &txt_out);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> txt_records = Array::New();

    struct ares_txt_reply *current = txt_out;
    for (int i = 0; current; ++i, current = current->next) {
      Local<String> txt = String::New(reinterpret_cast<char*>(current->txt));
      txt_records->Set(Integer::New(i), txt);
    }

    ares_free_data(txt_out);

    this->CallOnComplete(txt_records);
  }
};


class QuerySrvWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel,
               name,
               ns_c_in,
               ns_t_srv,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    struct ares_srv_reply* srv_start;
    int status = ares_parse_srv_reply(buf, len, &srv_start);
    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> srv_records = Array::New();
    Local<String> name_symbol = String::NewSymbol("name");
    Local<String> port_symbol = String::NewSymbol("port");
    Local<String> priority_symbol = String::NewSymbol("priority");
    Local<String> weight_symbol = String::NewSymbol("weight");
    int i = 0;
    for (struct ares_srv_reply* srv_current = srv_start;
         srv_current;
         srv_current = srv_current->next) {
      Local<Object> srv_record = Object::New();
      srv_record->Set(name_symbol, String::New(srv_current->host));
      srv_record->Set(port_symbol,
                      Integer::New(srv_current->port));
      srv_record->Set(priority_symbol,
                      Integer::New(srv_current->priority));
      srv_record->Set(weight_symbol,
                      Integer::New(srv_current->weight));
      srv_records->Set(Integer::New(i++), srv_record);
    }

    ares_free_data(srv_start);

    this->CallOnComplete(srv_records);
  }
};

class QueryNaptrWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    ares_query(ares_channel,
               name,
               ns_c_in,
               ns_t_naptr,
               Callback,
               GetQueryArg());
    return 0;
  }

 protected:
  void Parse(unsigned char* buf, int len) {
    HandleScope scope;

    ares_naptr_reply* naptr_start;
    int status = ares_parse_naptr_reply(buf, len, &naptr_start);

    if (status != ARES_SUCCESS) {
      this->ParseError(status);
      return;
    }

    Local<Array> naptr_records = Array::New();
    Local<String> flags_symbol = String::NewSymbol("flags");
    Local<String> service_symbol = String::NewSymbol("service");
    Local<String> regexp_symbol = String::NewSymbol("regexp");
    Local<String> replacement_symbol = String::NewSymbol("replacement");
    Local<String> order_symbol = String::NewSymbol("order");
    Local<String> preference_symbol = String::NewSymbol("preference");

    int i = 0;
    for (ares_naptr_reply* naptr_current = naptr_start;
         naptr_current;
         naptr_current = naptr_current->next) {

      Local<Object> naptr_record = Object::New();

      naptr_record->Set(flags_symbol,
          String::New(reinterpret_cast<char*>(naptr_current->flags)));
      naptr_record->Set(service_symbol,
          String::New(reinterpret_cast<char*>(naptr_current->service)));
      naptr_record->Set(regexp_symbol,
          String::New(reinterpret_cast<char*>(naptr_current->regexp)));
      naptr_record->Set(replacement_symbol,
          String::New(naptr_current->replacement));
      naptr_record->Set(order_symbol, Integer::New(naptr_current->order));
      naptr_record->Set(preference_symbol,
          Integer::New(naptr_current->preference));

      naptr_records->Set(Integer::New(i++), naptr_record);
    }

    ares_free_data(naptr_start);

    this->CallOnComplete(naptr_records);
  }
};


class GetHostByAddrWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    int length, family;
    char address_buffer[sizeof(struct in6_addr)];

    if (uv_inet_pton(AF_INET, name, &address_buffer).code == UV_OK) {
      length = sizeof(struct in_addr);
      family = AF_INET;
    } else if (uv_inet_pton(AF_INET6, name, &address_buffer).code == UV_OK) {
      length = sizeof(struct in6_addr);
      family = AF_INET6;
    } else {
      return ARES_ENOTIMP;
    }

    ares_gethostbyaddr(ares_channel,
                       address_buffer,
                       length,
                       family,
                       Callback,
                       GetQueryArg());
    return 0;
  }

 protected:
  void Parse(struct hostent* host) {
    HandleScope scope;

    this->CallOnComplete(HostentToNames(host));
  }
};


class GetHostByNameWrap: public QueryWrap {
 public:
  int Send(const char* name, int family) {
    ares_gethostbyname(ares_channel, name, family, Callback, GetQueryArg());
    return 0;
  }

 protected:
  void Parse(struct hostent* host) {
    HandleScope scope;

    Local<Array> addresses = HostentToAddresses(host);
    Local<Integer> family = Integer::New(host->h_addrtype);

    this->CallOnComplete(addresses, family);
  }
};


template <class Wrap>
static Handle<Value> Query(const Arguments& args) {
  HandleScope scope;

  assert(!args.IsConstructCall());
  assert(args.Length() >= 2);
  assert(args[1]->IsFunction());

  Wrap* wrap = new Wrap();
  wrap->SetOnComplete(args[1]);

  // We must cache the wrap's js object here, because cares might make the
  // callback from the wrap->Send stack. This will destroy the wrap's internal
  // object reference, causing wrap->GetObject() to return undefined.
  Local<Object> object = Local<Object>::New(wrap->GetObject());

  String::Utf8Value name(args[0]);

  int r = wrap->Send(*name);
  if (r) {
    SetAresErrno(r);
    delete wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(object);
  }
}


template <class Wrap>
static Handle<Value> QueryWithFamily(const Arguments& args) {
  HandleScope scope;

  assert(!args.IsConstructCall());
  assert(args.Length() >= 3);
  assert(args[2]->IsFunction());

  Wrap* wrap = new Wrap();
  wrap->SetOnComplete(args[2]);

  // We must cache the wrap's js object here, because cares might make the
  // callback from the wrap->Send stack. This will destroy the wrap's internal
  // object reference, causing wrap->GetObject() to return undefined.
  Local<Object> object = Local<Object>::New(wrap->GetObject());

  String::Utf8Value name(args[0]);
  int family = args[1]->Int32Value();

  int r = wrap->Send(*name, family);
  if (r) {
    SetAresErrno(r);
    delete wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(object);
  }
}


void AfterGetAddrInfo(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  HandleScope scope;

  GetAddrInfoReqWrap* req_wrap = (GetAddrInfoReqWrap*) req->data;

  Local<Value> argv[1];

  if (status) {
    // Error
    SetErrno(uv_last_error(uv_default_loop()));
    argv[0] = Local<Value>::New(Null());
  } else {
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
        addr = (char*) &((struct sockaddr_in*) address->ai_addr)->sin_addr;
        uv_err_t err = uv_inet_ntop(address->ai_family,
                                    addr,
                                    ip,
                                    INET6_ADDRSTRLEN);
        if (err.code != UV_OK)
          continue;

        // Create JavaScript string
        Local<String> s = String::New(ip);
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
        addr = (char*) &((struct sockaddr_in6*) address->ai_addr)->sin6_addr;
        uv_err_t err = uv_inet_ntop(address->ai_family,
                                    addr,
                                    ip,
                                    INET6_ADDRSTRLEN);
        if (err.code != UV_OK)
          continue;

        // Create JavaScript string
        Local<String> s = String::New(ip);
        results->Set(n, s);
        n++;
      }

      // Increment
      address = address->ai_next;
    }


    argv[0] = results;
  }

  uv_freeaddrinfo(res);

  // Make the callback into JavaScript
  MakeCallback(req_wrap->object_, oncomplete_sym, ARRAY_SIZE(argv), argv);

  delete req_wrap;
}


static Handle<Value> IsIP(const Arguments& args) {
  HandleScope scope;

  String::AsciiValue ip(args[0]);
  char address_buffer[sizeof(struct in6_addr)];

  if (uv_inet_pton(AF_INET, *ip, &address_buffer).code == UV_OK) {
    return scope.Close(v8::Integer::New(4));
  }

  if (uv_inet_pton(AF_INET6, *ip, &address_buffer).code == UV_OK) {
    return scope.Close(v8::Integer::New(6));
  }

  return scope.Close(v8::Integer::New(0));
}


static Handle<Value> GetAddrInfo(const Arguments& args) {
  HandleScope scope;

  String::Utf8Value hostname(args[0]);

  int fam = AF_UNSPEC;
  if (args[1]->IsInt32()) {
    switch (args[1]->Int32Value()) {
      case 6:
        fam = AF_INET6;
        break;

      case 4:
        fam = AF_INET;
        break;
    }
  }

  GetAddrInfoReqWrap* req_wrap = new GetAddrInfoReqWrap();

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = fam;
  hints.ai_socktype = SOCK_STREAM;

  int r = uv_getaddrinfo(uv_default_loop(),
                         &req_wrap->req_,
                         AfterGetAddrInfo,
                         *hostname,
                         NULL,
                         &hints);
  req_wrap->Dispatched();

  if (r) {
    SetErrno(uv_last_error(uv_default_loop()));
    delete req_wrap;
    return scope.Close(v8::Null());
  } else {
    return scope.Close(req_wrap->object_);
  }
}


static void Initialize(Handle<Object> target) {
  HandleScope scope;
  int r;

  r = ares_library_init(ARES_LIB_INIT_ALL);
  assert(r == ARES_SUCCESS);

  struct ares_options options;
  memset(&options, 0, sizeof(options));
  options.flags = ARES_FLAG_NOCHECKRESP;
  options.sock_state_cb = ares_sockstate_cb;
  options.sock_state_cb_data = uv_default_loop();

  /* We do the call to ares_init_option for caller. */
  r = ares_init_options(&ares_channel,
                        &options,
                        ARES_OPT_FLAGS | ARES_OPT_SOCK_STATE_CB);
  assert(r == ARES_SUCCESS);

  /* Initialize the timeout timer. The timer won't be started until the */
  /* first socket is opened. */
  uv_timer_init(uv_default_loop(), &ares_timer);

  NODE_SET_METHOD(target, "queryA", Query<QueryAWrap>);
  NODE_SET_METHOD(target, "queryAaaa", Query<QueryAaaaWrap>);
  NODE_SET_METHOD(target, "queryCname", Query<QueryCnameWrap>);
  NODE_SET_METHOD(target, "queryMx", Query<QueryMxWrap>);
  NODE_SET_METHOD(target, "queryNs", Query<QueryNsWrap>);
  NODE_SET_METHOD(target, "queryTxt", Query<QueryTxtWrap>);
  NODE_SET_METHOD(target, "querySrv", Query<QuerySrvWrap>);
  NODE_SET_METHOD(target, "queryNaptr", Query<QueryNaptrWrap>);
  NODE_SET_METHOD(target, "getHostByAddr", Query<GetHostByAddrWrap>);
  NODE_SET_METHOD(target, "getHostByName", QueryWithFamily<GetHostByNameWrap>);

  NODE_SET_METHOD(target, "getaddrinfo", GetAddrInfo);
  NODE_SET_METHOD(target, "isIP", IsIP);

  target->Set(String::NewSymbol("AF_INET"),
              Integer::New(AF_INET));
  target->Set(String::NewSymbol("AF_INET6"),
              Integer::New(AF_INET6));
  target->Set(String::NewSymbol("AF_UNSPEC"),
              Integer::New(AF_UNSPEC));

  oncomplete_sym = NODE_PSYMBOL("oncomplete");
}


} // namespace cares_wrap

}  // namespace node

NODE_MODULE(node_cares_wrap, node::cares_wrap::Initialize)
