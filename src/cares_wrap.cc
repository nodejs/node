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
#include <node.h>
#include <req_wrap.h>
#include <uv.h>

#include <string.h>

#if defined(__OpenBSD__) || defined(__MINGW32__) || defined(_MSC_VER)
# include <nameser.h>
#else
# include <arpa/nameser.h>
#endif

// Temporary hack: libuv should provide uv_inet_pton and uv_inet_ntop.
#if defined(__MINGW32__) || defined(_MSC_VER)
  extern "C" {
#   include <inet_net_pton.h>
#   include <inet_ntop.h>
  }
# define uv_inet_pton ares_inet_pton
# define uv_inet_ntop ares_inet_ntop

#else // __POSIX__
# include <arpa/inet.h>
# define uv_inet_pton inet_pton
# define uv_inet_ntop inet_ntop
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

static Persistent<String> oncomplete_sym;

static ares_channel ares_channel;


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
  Handle<Value> key = String::NewSymbol("errno");
  Handle<Value> value = String::NewSymbol(AresErrnoString(errorno));
  Context::GetCurrent()->Global()->Set(key, value);
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
    QueryWrap* wrap = reinterpret_cast<QueryWrap*>(arg);

    if (status != ARES_SUCCESS) {
      wrap->ParseError(status);
    } else {
      wrap->Parse(answer_buf, answer_len);
    }

    delete wrap;
  }

  static void Callback(void *arg, int status, int timeouts,
      struct hostent* host) {
    QueryWrap* wrap = reinterpret_cast<QueryWrap*>(arg);

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
    MakeCallback(object_, "oncomplete", 2, argv);
  }

  void CallOnComplete(Local<Value> answer, Local<Value> family) {
    HandleScope scope;
    Local<Value> argv[3] = { Integer::New(0), answer, family };
    MakeCallback(object_, "oncomplete", 3, argv);
  }

  void ParseError(int status) {
    assert(status != ARES_SUCCESS);
    SetAresErrno(status);

    HandleScope scope;
    Local<Value> argv[1] = { Integer::New(-1) };
    MakeCallback(object_, "oncomplete", 1, argv);
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
      mx_record->Set(priority_symbol, Integer::New(mx_current->priority));
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
      srv_record->Set(port_symbol, Integer::New(srv_current->port));
      srv_record->Set(priority_symbol, Integer::New(srv_current->priority));
      srv_record->Set(weight_symbol, Integer::New(srv_current->weight));
      srv_records->Set(Integer::New(i++), srv_record);
    }

    ares_free_data(srv_start);

    this->CallOnComplete(srv_records);
  }
};


class GetHostByAddrWrap: public QueryWrap {
 public:
  int Send(const char* name) {
    int length, family;
    char address_buffer[sizeof(struct in6_addr)];

    if (uv_inet_pton(AF_INET, name, &address_buffer) == 1) {
      length = sizeof(struct in_addr);
      family = AF_INET;
    } else if (uv_inet_pton(AF_INET6, name, &address_buffer) == 1) {
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

  String::Utf8Value name(args[0]->ToString());

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

  String::Utf8Value name(args[0]->ToString());
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
        const char* c = uv_inet_ntop(address->ai_family, addr, ip,
            INET6_ADDRSTRLEN);

        // Create JavaScript string
        Local<String> s = String::New(c);
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
        const char* c = uv_inet_ntop(address->ai_family, addr, ip,
            INET6_ADDRSTRLEN);

        // Create JavaScript string
        Local<String> s = String::New(c);
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
  MakeCallback(req_wrap->object_, "oncomplete", 1, argv);

  delete req_wrap;
}


static Handle<Value> GetAddrInfo(const Arguments& args) {
  HandleScope scope;

  String::Utf8Value hostname(args[0]->ToString());

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
  uv_ares_init_options(uv_default_loop(), &ares_channel, &options, 0);
  assert(r == 0);

  NODE_SET_METHOD(target, "queryA", Query<QueryAWrap>);
  NODE_SET_METHOD(target, "queryAaaa", Query<QueryAaaaWrap>);
  NODE_SET_METHOD(target, "queryCname", Query<QueryCnameWrap>);
  NODE_SET_METHOD(target, "queryMx", Query<QueryMxWrap>);
  NODE_SET_METHOD(target, "queryNs", Query<QueryNsWrap>);
  NODE_SET_METHOD(target, "queryTxt", Query<QueryTxtWrap>);
  NODE_SET_METHOD(target, "querySrv", Query<QuerySrvWrap>);
  NODE_SET_METHOD(target, "getHostByAddr", Query<GetHostByAddrWrap>);
  NODE_SET_METHOD(target, "getHostByName", QueryWithFamily<GetHostByNameWrap>);

  NODE_SET_METHOD(target, "getaddrinfo", GetAddrInfo);

  target->Set(String::NewSymbol("AF_INET"), Integer::New(AF_INET));
  target->Set(String::NewSymbol("AF_INET6"), Integer::New(AF_INET6));
  target->Set(String::NewSymbol("AF_UNSPEC"), Integer::New(AF_UNSPEC));

  oncomplete_sym = Persistent<String>::New(String::NewSymbol("oncomplete"));
}


} // namespace cares_wrap

}  // namespace node

NODE_MODULE(node_cares_wrap, node::cares_wrap::Initialize)
