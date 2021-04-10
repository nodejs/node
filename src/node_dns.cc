#include "node_dns.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_mem-inl.h"
#include "req_wrap-inl.h"
#include "util-inl.h"
#include "uv.h"

namespace node {
namespace dns {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Promise;
using v8::String;
using v8::Value;

#define GETDNS_BASE(ret, name, isolate, ...)                                   \
  do {                                                                         \
    getdns_return_t r = getdns_##name(__VA_ARGS__);                            \
    if (UNLIKELY(r != GETDNS_RETURN_GOOD)) {                                   \
      THROW_ERR_DNS_ERROR(isolate, getdns_get_errorstr_by_id(r));              \
      return ret;                                                              \
    }                                                                          \
  } while (false);

#define GETDNS(name, isolate, ...)                                             \
  GETDNS_BASE(/* empty */, name, isolate, __VA_ARGS__)
#define GETDNSV(name, isolate, ...) GETDNS_BASE({}, name, isolate, __VA_ARGS__)

DNSWrap::DNSWrap(Environment* env, Local<Object> object, int32_t timeout)
    : AsyncWrap(env, object, PROVIDER_DNSCHANNEL), allocator_(MakeAllocator()) {
  MakeWeak();

  Isolate* isolate = env->isolate();
  GETDNS(context_create_with_extended_memory_functions,
         isolate,
         &context_,
         true,
         reinterpret_cast<void*>(&allocator_),
         // Convert from getdns allocator signature to libng signature
         [](void* data, size_t size) -> void* {
           Allocator* a = reinterpret_cast<Allocator*>(data);
           return a->malloc(size, a->data);
         },
         [](void* data, void* ptr, size_t size) -> void* {
           Allocator* a = reinterpret_cast<Allocator*>(data);
           return a->realloc(ptr, size, a->data);
         },
         [](void* data, void* ptr) {
           Allocator* a = reinterpret_cast<Allocator*>(data);
           a->free(ptr, a->data);
         });
  GETDNS(extension_set_libuv_loop, isolate, context_, env->event_loop());
  GETDNS(context_set_tls_authentication,
         isolate,
         context_,
         GETDNS_AUTHENTICATION_REQUIRED);
  if (timeout != -1) {
    GETDNS(
        context_set_timeout, isolate, context_, static_cast<uint64_t>(timeout));
  }
}

DNSWrap::~DNSWrap() {
  getdns_context_destroy(context_);
  CHECK_EQ(current_getdns_memory_, 0);
}

void DNSWrap::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("getdns_memory", current_getdns_memory_);
}

void DNSWrap::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_getdns_memory_, previous_size);
}

void DNSWrap::IncreaseAllocatedSize(size_t size) {
  current_getdns_memory_ += size;
}

void DNSWrap::DecreaseAllocatedSize(size_t size) {
  current_getdns_memory_ -= size;
}

void DNSWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  uint32_t timeout = args[0]->Int32Value(env->context()).ToChecked();
  new DNSWrap(env, args.This(), timeout);
}

// static
void DNSWrap::Callback(getdns_context* getdns_context,
                       getdns_callback_type_t cb_type,
                       getdns_dict* response,
                       void* data,
                       getdns_transaction_t tid) {
  DNSWrap* dns = static_cast<DNSWrap*>(data);
  Isolate* isolate = dns->env()->isolate();
  HandleScope scope(isolate);
  Local<Context> context = dns->env()->context();
  Context::Scope context_scope(context);

  auto cleanup = OnScopeLeave([&]{
    getdns_dict_destroy(response);
  });

  switch (cb_type) {
    case GETDNS_CALLBACK_COMPLETE:
      break;
#define V(name)                                                                \
  case GETDNS_CALLBACK_##name:                                                 \
    dns->RejectTransaction(                                                    \
        tid, ERR_DNS_##name(isolate, GETDNS_CALLBACK_##name##_TEXT));          \
    return;
      V(CANCEL)
      V(TIMEOUT)
      V(ERROR)
#undef V
    default:
      UNREACHABLE();
  }

  {
    uint32_t status;
    GETDNS(dict_get_int, isolate, response, "/status", &status);
    switch (status) {
      case GETDNS_RESPSTATUS_GOOD:
        break;
      case GETDNS_RESPSTATUS_NO_NAME: {
        // Sometimes getdns says NO_NAME but there are actually still answers,
        // so manually check before forwarding the error.
        size_t len;
        getdns_list* list;
        GETDNS(dict_get_list, isolate, response, "/replies_tree", &list);
        GETDNS(list_get_length, isolate, list, &len);
        if (len > 0) {
          GETDNS(dict_get_list,
                 isolate,
                 response,
                 "/replies_tree/0/answer",
                 &list);
          GETDNS(list_get_length, isolate, list, &len);
          if (len > 0) {
            break;
          }
        }
        dns->RejectTransaction(
            tid, ERR_DNS_NO_NAME(isolate, GETDNS_RESPSTATUS_NO_NAME_TEXT));
        return;
      }
#define V(name)                                                                \
  case GETDNS_RESPSTATUS_##name:                                               \
    dns->RejectTransaction(                                                    \
        tid, ERR_DNS_##name(isolate, GETDNS_RESPSTATUS_##name##_TEXT));        \
    return;
        V(ALL_TIMEOUT)
        V(NO_SECURE_ANSWERS)
        V(ALL_BOGUS_ANSWERS)
#undef V
      default:
        UNREACHABLE();
    }
  }

  char* json = getdns_print_json_dict(response, 0);
  Local<String> result = String::NewFromUtf8(isolate, json).ToLocalChecked();
  free(json);

  dns->ResolveTransaction(tid, result);
}

Local<Value> DNSWrap::RegisterTransaction(getdns_transaction_t tid) {
  Local<Promise::Resolver> p =
      Promise::Resolver::New(env()->context()).ToLocalChecked();
  transactions_[tid].wrap.Reset(env()->isolate(), object());
  transactions_[tid].resolver.Reset(env()->isolate(), p);
  return p->GetPromise();
}

void DNSWrap::ResolveTransaction(getdns_transaction_t tid, Local<Value> v) {
  auto it = transactions_.find(tid);
  it->second.resolver.Get(env()->isolate())
      ->Resolve(env()->context(), v)
      .ToChecked();
  transactions_.erase(it);
}

void DNSWrap::RejectTransaction(getdns_transaction_t tid, Local<Value> v) {
  auto it = transactions_.find(tid);
  it->second.resolver.Get(env()->isolate())
      ->Reject(env()->context(), v)
      .ToChecked();
  transactions_.erase(it);
}

// static
void DNSWrap::GetAddresses(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();
  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  getdns_dict* extension = nullptr;
  getdns_transaction_t tid;
  GETDNS(address,
         env->isolate(),
         dns->context_,
         *name,
         extension,
         dns,
         &tid,
         Callback);
  args.GetReturnValue().Set(dns->RegisterTransaction(tid));
}


static bool ConvertAddress(Isolate* isolate,
                           const char* address,
                           getdns_dict* dict) {
  char address_buffer[sizeof(struct in6_addr)];
  size_t length;
  int family;
  if (uv_inet_pton(AF_INET, address, &address_buffer) == 0) {
    length = sizeof(struct in_addr);
    family = AF_INET;
  } else if (uv_inet_pton(AF_INET6, address, &address_buffer) == 0) {
    length = sizeof(struct in6_addr);
    family = AF_INET6;
  } else {
    THROW_ERR_DNS_ERROR(isolate, "Invalid address");
    return false;
  }

  getdns_bindata address_data{length,
                              reinterpret_cast<uint8_t*>(address_buffer)};
  GETDNSV(dict_util_set_string,
          isolate,
          dict,
          "address_type",
          family == AF_INET ? "IPv4" : "IPv6");
  GETDNSV(dict_set_bindata, isolate, dict, "address_data", &address_data);

  return true;
}

// static
void DNSWrap::GetHostnames(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();

  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  getdns_dict* address = getdns_dict_create_with_context(dns->context_);
  auto cleanup = OnScopeLeave([&]() {
    getdns_dict_destroy(address);
  });
  if (!ConvertAddress(env->isolate(), *name, address)) {
    return;
  }

  getdns_dict* extension = nullptr;
  getdns_transaction_t tid;
  GETDNS(hostname,
         env->isolate(),
         dns->context_,
         address,
         extension,
         dns,
         &tid,
         Callback);
  args.GetReturnValue().Set(dns->RegisterTransaction(tid));
}

// static
void DNSWrap::GetGeneral(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();
  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  CHECK(args[1]->IsUint32());
  uint16_t type = args[1]->Uint32Value(env->context()).ToChecked();
  getdns_dict* extension = nullptr;
  getdns_transaction_t tid;
  GETDNS(general,
         env->isolate(),
         dns->context_,
         *name,
         type,
         extension,
         dns,
         &tid,
         Callback);
  args.GetReturnValue().Set(dns->RegisterTransaction(tid));
}

// static
void DNSWrap::SetUpstreamRecursiveServers(
    const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();

  CHECK(args[0]->IsArray());
  Local<Array> a = args[0].As<Array>();
  uint32_t length = a->Length();

  getdns_list* list = getdns_list_create_with_context(dns->context_);
  auto cleanup = OnScopeLeave([&]() {
    getdns_list_destroy(list);
  });

  for (uint32_t i = 0; i < length; i += 1) {
    getdns_dict* dict = getdns_dict_create_with_context(dns->context_);
    auto cleanup = OnScopeLeave([&]() {
      getdns_dict_destroy(dict);
    });
    Local<Array> server =
        a->Get(env->context(), i).ToLocalChecked().As<Array>();
    String::Utf8Value address(env->isolate(),
                              server->Get(env->context(), 0).ToLocalChecked());
    if (!ConvertAddress(env->isolate(), *address, dict)) {
      return;
    }
    uint32_t port = server->Get(env->context(), 1)
                        .ToLocalChecked()
                        ->Uint32Value(env->context())
                        .ToChecked();
    Local<Value> tls_hostname = server->Get(env->context(), 2).ToLocalChecked();
    if (!tls_hostname->IsUndefined()) {
      GETDNS(dict_set_int, env->isolate(), dict, "tls_port", port);
      String::Utf8Value hostname(env->isolate(), tls_hostname);
      GETDNS(dict_util_set_string,
             env->isolate(),
             dict,
             "tls_auth_name",
             *hostname);
    } else {
      GETDNS(dict_set_int, env->isolate(), dict, "port", port);
    }
    GETDNS(list_set_dict, env->isolate(), list, i, dict);
  }

  GETDNS(context_set_upstream_recursive_servers,
         env->isolate(),
         dns->context_,
         list);
}

// static
void DNSWrap::GetUpstreamRecursiveServers(
    const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();

  getdns_list* list;
  auto cleanup = OnScopeLeave([&]() {
    getdns_list_destroy(list);
  });
  GETDNS(context_get_upstream_recursive_servers,
         env->isolate(),
         dns->context_,
         &list);

  char* json = getdns_print_json_list(list, 0);
  Local<String> result =
      String::NewFromUtf8(env->isolate(), json).ToLocalChecked();
  free(json);

  args.GetReturnValue().Set(result);
}

// static
void DNSWrap::SetDNSTransportList(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = dns->env();

  CHECK(args[0]->IsArray());
  Local<Array> transports = args[0].As<Array>();
  uint32_t length = transports->Length();

  std::vector<getdns_transport_list_t> transport_list(length);
  for (uint32_t i = 0; i < length; i += 1) {
    getdns_transport_list_t transport =
        static_cast<getdns_transport_list_t>(transports->Get(env->context(), i)
                                                 .ToLocalChecked()
                                                 ->Uint32Value(env->context())
                                                 .ToChecked());
    transport_list[i] = transport;
  }

  GETDNS(context_set_dns_transport_list,
         env->isolate(),
         dns->context_,
         length,
         &transport_list[0]);
}

// static
void DNSWrap::CancelAllTransactions(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  std::vector<getdns_transaction_t> transactions;
  for (auto& t : dns->transactions_) {
    transactions.push_back(t.first);
  }
  for (auto& t : transactions) {
    GETDNS(cancel_callback, dns->env()->isolate(), dns->context_, t);
  }
  dns->transactions_.clear();
}

class GetNameInfoReqWrap : public ReqWrap<uv_getnameinfo_t> {
 public:
  GetNameInfoReqWrap(Environment* env, Local<Object> req_wrap_obj);

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(GetNameInfoReqWrap)
  SET_SELF_SIZE(GetNameInfoReqWrap)
};

GetNameInfoReqWrap::GetNameInfoReqWrap(Environment* env,
                                       Local<Object> req_wrap_obj)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_GETNAMEINFOREQWRAP) {}

void AfterGetNameInfo(uv_getnameinfo_t* req,
                      int status,
                      const char* hostname,
                      const char* service) {
  std::unique_ptr<GetNameInfoReqWrap> req_wrap{
      static_cast<GetNameInfoReqWrap*>(req->data)};
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[] = {Integer::New(env->isolate(), status),
                         Null(env->isolate()),
                         Null(env->isolate())};

  if (status == 0) {
    // Success
    Local<String> js_hostname = OneByteString(env->isolate(), hostname);
    Local<String> js_service = OneByteString(env->isolate(), service);
    argv[1] = js_hostname;
    argv[2] = js_service;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END2(TRACING_CATEGORY_NODE2(dns, native),
                                  "lookupService",
                                  req_wrap.get(),
                                  "hostname",
                                  TRACE_STR_COPY(hostname),
                                  "service",
                                  TRACE_STR_COPY(service));

  // Make the callback into JavaScript
  req_wrap->MakeCallback(env->oncomplete_string(), arraysize(argv), argv);
}

void GetNameInfo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsString());
  CHECK(args[2]->IsUint32());
  Local<Object> req_wrap_obj = args[0].As<Object>();
  node::Utf8Value ip(env->isolate(), args[1]);
  const unsigned port = args[2]->Uint32Value(env->context()).FromJust();
  struct sockaddr_storage addr;

  CHECK(uv_ip4_addr(*ip, port, reinterpret_cast<sockaddr_in*>(&addr)) == 0 ||
        uv_ip6_addr(*ip, port, reinterpret_cast<sockaddr_in6*>(&addr)) == 0);

  auto req_wrap = std::make_unique<GetNameInfoReqWrap>(env, req_wrap_obj);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(TRACING_CATEGORY_NODE2(dns, native),
                                    "lookupService",
                                    req_wrap.get(),
                                    "ip",
                                    TRACE_STR_COPY(*ip),
                                    "port",
                                    port);

  int err = req_wrap->Dispatch(uv_getnameinfo,
                               AfterGetNameInfo,
                               reinterpret_cast<struct sockaddr*>(&addr),
                               NI_NAMEREQD);
  if (err == 0)
    // Release ownership of the pointer allowing the ownership to be transferred
    USE(req_wrap.release());

  args.GetReturnValue().Set(err);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // DNSWrap
  Local<FunctionTemplate> tmpl = env->NewFunctionTemplate(DNSWrap::New);
  auto dns_wrap_string = FIXED_ONE_BYTE_STRING(env->isolate(), "DNSWrap");
  tmpl->InstanceTemplate()->SetInternalFieldCount(DNSWrap::kInternalFieldCount);
  tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
  tmpl->SetClassName(dns_wrap_string);

  env->SetProtoMethod(tmpl, "getAddresses", DNSWrap::GetAddresses);
  env->SetProtoMethod(tmpl, "getHostnames", DNSWrap::GetHostnames);
  env->SetProtoMethod(tmpl, "getGeneral", DNSWrap::GetGeneral);

  env->SetProtoMethod(tmpl,
                      "setUpstreamRecursiveServers",
                      DNSWrap::SetUpstreamRecursiveServers);
  env->SetProtoMethod(tmpl,
                      "getUpstreamRecursiveServers",
                      DNSWrap::GetUpstreamRecursiveServers);
  env->SetProtoMethod(
      tmpl, "setDNSTransportList", DNSWrap::SetDNSTransportList);

  env->SetProtoMethod(
      tmpl, "cancelAllTransactions", DNSWrap::CancelAllTransactions);

  env->SetConstructorFunction(target, "DNSWrap", tmpl);

  // getnameinfo
  env->SetMethod(target, "getnameinfo", GetNameInfo);
  Local<FunctionTemplate> niw =
      BaseObject::MakeLazilyInitializedJSTemplate(env);
  niw->Inherit(AsyncWrap::GetConstructorTemplate(env));
  env->SetConstructorFunction(target, "GetNameInfoReqWrap", niw);

#define V(name)                                                                \
  target                                                                       \
      ->Set(env->context(),                                                    \
            FIXED_ONE_BYTE_STRING(env->isolate(), #name),                      \
            Integer::New(env->isolate(), name))                                \
      .ToChecked();

  V(GETDNS_TRANSPORT_UDP)
  V(GETDNS_TRANSPORT_TCP)
  V(GETDNS_TRANSPORT_TLS)
#undef V
}

}  // namespace dns
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(dns, node::dns::Initialize)
