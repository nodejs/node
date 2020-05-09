#include "node_dns.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "util-inl.h"
#include "uv.h"

namespace node {
namespace dns {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Name;
using v8::NewStringType;
using v8::Null;
using v8::Object;
using v8::Promise;
using v8::String;
using v8::Value;

#define GETDNS(name, isolate, ...)                                             \
  do {                                                                         \
    getdns_return_t r = getdns_##name(__VA_ARGS__);                            \
    if (r) {                                                                   \
      THROW_ERR_DNS_ERROR(isolate, getdns_get_errorstr_by_id(r));              \
      return;                                                                  \
    }                                                                          \
  } while (false);

#define GETDNSV(name, isolate, ...)                                            \
  do {                                                                         \
    getdns_return_t r = getdns_##name(__VA_ARGS__);                            \
    if (r) {                                                                   \
      THROW_ERR_DNS_ERROR(isolate, getdns_get_errorstr_by_id(r));              \
      return {};                                                               \
    }                                                                          \
  } while (false);

DNSWrap::DNSWrap(Environment* env, Local<Object> object)
    : BaseObject(env, object) {
  Isolate* isolate = env->isolate();
  GETDNS(context_create, isolate, &context_, true);
  GETDNS(extension_set_libuv_loop, isolate, context_, env->event_loop());
  GETDNS(context_set_tls_authentication,
         isolate,
         context_,
         GETDNS_AUTHENTICATION_REQUIRED);
}

DNSWrap::~DNSWrap() {
  getdns_context_destroy(context_);
}

void DNSWrap::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new DNSWrap(env, args.This());
}

class DNSWrapRequest {
 public:
  explicit DNSWrapRequest(Local<Context> context)
      : isolate_(context->GetIsolate()) {
    Local<Promise::Resolver> resolver =
        Promise::Resolver::New(context).ToLocalChecked();
    context_.Reset(isolate_, context);
    resolver_.Reset(isolate_, resolver);
  }

  ~DNSWrapRequest() { resolver_.Reset(); }

  Local<Promise> promise() { return resolver_.Get(isolate_)->GetPromise(); }

  void resolve(Local<Value> value) {
    USE(resolver_.Get(isolate_)->Resolve(context(), value));
  }

  void reject(Local<Value> value) {
    USE(resolver_.Get(isolate_)->Reject(context(), value));
  }

  Local<Context> context() { return context_.Get(isolate_); }

  Isolate* isolate() const { return isolate_; }

 private:
  Global<Context> context_;
  Global<Promise::Resolver> resolver_;
  Isolate* isolate_;
};

Local<Value> RRToValue(Local<Context> context, int type, getdns_dict* rdata) {
  Local<Object> o = Object::New(context->GetIsolate());
#define OPEN_IP(rname, vname)                                                  \
  do {                                                                         \
    getdns_bindata* data;                                                      \
    GETDNSV(dict_get_bindata, context->GetIsolate(), rdata, #rname, &data);    \
    char* ip = getdns_display_ip_address(data);                                \
    o->Set(context,                                                            \
           FIXED_ONE_BYTE_STRING(context->GetIsolate(), #vname),               \
           String::NewFromUtf8(context->GetIsolate(), ip).ToLocalChecked())    \
        .ToChecked();                                                          \
    free(ip);                                                                  \
  } while (false);

#define OPEN_NAME(rname, vname)                                                \
  do {                                                                         \
    getdns_bindata* data;                                                      \
    GETDNSV(dict_get_bindata, context->GetIsolate(), rdata, #rname, &data);    \
    char* dname = nullptr;                                                     \
    GETDNSV(convert_dns_name_to_fqdn, context->GetIsolate(), data, &dname);    \
    o->Set(context,                                                            \
           FIXED_ONE_BYTE_STRING(context->GetIsolate(), #vname),               \
           String::NewFromUtf8(context->GetIsolate(), dname).ToLocalChecked()) \
        .ToChecked();                                                          \
    free(dname);                                                               \
  } while (false);

#define OPEN_STRING(rname, vname)                                              \
  do {                                                                         \
    getdns_bindata* data;                                                      \
    GETDNSV(dict_get_bindata, context->GetIsolate(), rdata, #rname, &data);    \
    o->Set(context,                                                            \
           FIXED_ONE_BYTE_STRING(context->GetIsolate(), #vname),               \
           String::NewFromUtf8(context->GetIsolate(),                          \
                               reinterpret_cast<char*>(data->data),            \
                               NewStringType::kNormal,                         \
                               data->size)                                     \
               .ToLocalChecked())                                              \
        .ToChecked();                                                          \
  } while (false);

#define OPEN_INT(rname, vname)                                                 \
  do {                                                                         \
    uint32_t i;                                                                \
    GETDNSV(dict_get_int, context->GetIsolate(), rdata, #rname, &i);           \
    o->Set(context,                                                            \
           FIXED_ONE_BYTE_STRING(context->GetIsolate(), #vname),               \
           Integer::New(context->GetIsolate(), i))                             \
        .ToChecked();                                                          \
  } while (false);

  switch (type) {
    case GETDNS_RRTYPE_A:
      OPEN_IP(ipv4_address, address);
      break;
    case GETDNS_RRTYPE_NS:
      OPEN_NAME(nsdname, name);
      break;
    case GETDNS_RRTYPE_MD:
      OPEN_NAME(madname, name);
      break;
    case GETDNS_RRTYPE_MF:
      OPEN_NAME(madname, name);
      break;
    case GETDNS_RRTYPE_CNAME:
      OPEN_NAME(cname, name);
      break;
    case GETDNS_RRTYPE_SOA:
      OPEN_NAME(mname, mname);
      OPEN_NAME(rname, rname);
      OPEN_INT(serial, serial);
      OPEN_INT(refresh, refresh);
      OPEN_INT(retry, retry);
      OPEN_INT(expire, expire);
      break;
    case GETDNS_RRTYPE_MB:
      OPEN_NAME(madname, name);
      break;
    case GETDNS_RRTYPE_MG:
      OPEN_NAME(mgmname, name);
      break;
    case GETDNS_RRTYPE_MR:
      OPEN_NAME(newname, name);
      break;
    case GETDNS_RRTYPE_NULL:
    case GETDNS_RRTYPE_WKS:
      break;
    case GETDNS_RRTYPE_PTR:
      OPEN_NAME(ptrdname, name);
      break;
    case GETDNS_RRTYPE_HINFO:
    case GETDNS_RRTYPE_MINFO:
      break;
    case GETDNS_RRTYPE_MX:
      OPEN_NAME(exchange, exchange);
      OPEN_INT(preference, preference);
      break;
    case GETDNS_RRTYPE_TXT: {
      getdns_list* list;
      GETDNSV(
          dict_get_list, context->GetIsolate(), rdata, "txt_strings", &list);
      size_t length;
      GETDNSV(list_get_length, context->GetIsolate(), list, &length);
      Local<Value> elements[length];
      for (size_t i = 0; i < length; i += 1) {
        getdns_bindata* data;
        GETDNSV(list_get_bindata, context->GetIsolate(), list, i, &data);
        elements[i] = String::NewFromUtf8(context->GetIsolate(),
                                          reinterpret_cast<char*>(data->data),
                                          NewStringType::kNormal,
                                          data->size)
                          .ToLocalChecked();
      }
      o->Set(context,
             FIXED_ONE_BYTE_STRING(context->GetIsolate(), "strings"),
             Array::New(context->GetIsolate(), elements, length))
          .ToChecked();
      break;
    }
    case GETDNS_RRTYPE_RP:
    case GETDNS_RRTYPE_AFSDB:
    case GETDNS_RRTYPE_X25:
    case GETDNS_RRTYPE_ISDN:
    case GETDNS_RRTYPE_RT:
    case GETDNS_RRTYPE_NSAP:
    case GETDNS_RRTYPE_NSAP_PTR:
    case GETDNS_RRTYPE_SIG:
    case GETDNS_RRTYPE_KEY:
    case GETDNS_RRTYPE_PX:
    case GETDNS_RRTYPE_GPOS:
      break;
    case GETDNS_RRTYPE_AAAA:
      OPEN_IP(ipv6_address, address);
      break;
    case GETDNS_RRTYPE_LOC:
    case GETDNS_RRTYPE_NXT:
    case GETDNS_RRTYPE_EID:
    case GETDNS_RRTYPE_NIMLOC:
      break;
    case GETDNS_RRTYPE_SRV:
      OPEN_INT(priority, priority);
      OPEN_INT(weight, weight);
      OPEN_INT(port, port);
      OPEN_NAME(target, target);
      break;
    case GETDNS_RRTYPE_ATMA:
    case GETDNS_RRTYPE_NAPTR:
    case GETDNS_RRTYPE_KX:
    case GETDNS_RRTYPE_CERT:
    case GETDNS_RRTYPE_A6:
    case GETDNS_RRTYPE_DNAME:
    case GETDNS_RRTYPE_SINK:
    case GETDNS_RRTYPE_OPT:
    case GETDNS_RRTYPE_APL:
    case GETDNS_RRTYPE_DS:
    case GETDNS_RRTYPE_SSHFP:
    case GETDNS_RRTYPE_IPSECKEY:
    case GETDNS_RRTYPE_RRSIG:
    case GETDNS_RRTYPE_NSEC:
    case GETDNS_RRTYPE_DNSKEY:
    case GETDNS_RRTYPE_DHCID:
    case GETDNS_RRTYPE_NSEC3:
    case GETDNS_RRTYPE_NSEC3PARAM:
    case GETDNS_RRTYPE_TLSA:
    case GETDNS_RRTYPE_SMIMEA:
    case GETDNS_RRTYPE_HIP:
    case GETDNS_RRTYPE_NINFO:
    case GETDNS_RRTYPE_RKEY:
    case GETDNS_RRTYPE_TALINK:
    case GETDNS_RRTYPE_CDS:
    case GETDNS_RRTYPE_CDNSKEY:
    case GETDNS_RRTYPE_OPENPGPKEY:
    case GETDNS_RRTYPE_CSYNC:
    case GETDNS_RRTYPE_ZONEMD:
    case GETDNS_RRTYPE_SPF:
    case GETDNS_RRTYPE_UINFO:
    case GETDNS_RRTYPE_UID:
    case GETDNS_RRTYPE_GID:
    case GETDNS_RRTYPE_UNSPEC:
    case GETDNS_RRTYPE_NID:
    case GETDNS_RRTYPE_L32:
    case GETDNS_RRTYPE_L64:
    case GETDNS_RRTYPE_LP:
    case GETDNS_RRTYPE_EUI48:
    case GETDNS_RRTYPE_EUI64:
    case GETDNS_RRTYPE_TKEY:
    case GETDNS_RRTYPE_TSIG:
    case GETDNS_RRTYPE_IXFR:
    case GETDNS_RRTYPE_AXFR:
    case GETDNS_RRTYPE_MAILB:
    case GETDNS_RRTYPE_MAILA:
    case GETDNS_RRTYPE_ANY:
    case GETDNS_RRTYPE_URI:
      break;
    case GETDNS_RRTYPE_CAA:
      OPEN_INT(flags, flags);
      OPEN_STRING(tag, tag);
      OPEN_STRING(value, value);
      break;
    case GETDNS_RRTYPE_AVC:
    case GETDNS_RRTYPE_DOA:
    case GETDNS_RRTYPE_AMTRELAY:
    case GETDNS_RRTYPE_TA:
    case GETDNS_RRTYPE_DLV:
    default:
      break;
  }
#undef OPEN_IP
#undef OPEN_NAME
#undef OPEN_INT
  return o;
}

// static
void DNSWrap::Callback(getdns_context* getdns_context,
                       getdns_callback_type_t cb_type,
                       getdns_dict* response,
                       void* data,
                       getdns_transaction_t t_id) {
  DNSWrapRequest* req = reinterpret_cast<DNSWrapRequest*>(data);
  Isolate* isolate = req->isolate();
  HandleScope scope(isolate);
  Local<Context> context = req->context();
  Context::Scope context_scope(context);

  switch (cb_type) {
    case GETDNS_CALLBACK_COMPLETE:
      break;
#define V(name)                                                                \
  case GETDNS_CALLBACK_##name:                                                 \
    req->reject(ERR_DNS_##name(isolate, GETDNS_CALLBACK_##name##_TEXT));       \
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
#define V(name)                                                                \
  case GETDNS_RESPSTATUS_##name:                                               \
    req->reject(ERR_DNS_##name(isolate, GETDNS_RESPSTATUS_##name##_TEXT));     \
    return;
        V(NO_NAME)
        V(ALL_TIMEOUT)
        V(NO_SECURE_ANSWERS)
        V(ALL_BOGUS_ANSWERS)
#undef V
      default:
        UNREACHABLE();
    }
  }

  Local<Object> result = Object::New(isolate);

  {
    getdns_list* answers;
    if (getdns_dict_get_list(response, "/just_address_answers", &answers) ==
        GETDNS_RETURN_GOOD) {
      size_t length;
      GETDNS(list_get_length, isolate, answers, &length);
      Local<Value> elements[length];
      for (size_t i = 0; i < length; i += 1) {
        getdns_dict* answer;
        GETDNS(list_get_dict, isolate, answers, i, &answer);
        getdns_bindata* data;
        GETDNS(dict_get_bindata, isolate, answer, "address_type", &data);
        uint32_t family = 0;
        if (data->size == 4) {
          if (strcmp("IPv4", reinterpret_cast<char*>(data->data)) == 0) {
            family = 4;
          } else if (strcmp("IPv6", reinterpret_cast<char*>(data->data)) == 0) {
            family = 6;
          }
        }
        Local<Object> o = Object::New(isolate);
        o->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "family"),
               Integer::NewFromUnsigned(isolate, family))
            .ToChecked();
        GETDNS(dict_get_bindata, isolate, answer, "address_data", &data);
        char* ip = getdns_display_ip_address(data);
        o->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "address"),
               String::NewFromUtf8(isolate, ip).ToLocalChecked())
            .ToChecked();
        free(ip);
        elements[i] = o;
      }
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "addresses"),
                Array::New(isolate, elements, length))
          .ToChecked();
    }
  }

  {
    getdns_list* answer;
    // We never ask multiple questions, so there can only be one answer.
    if (getdns_dict_get_list(response, "/replies_tree/0/answer", &answer) ==
        GETDNS_RETURN_GOOD) {
      size_t length;
      GETDNS(list_get_length, isolate, answer, &length);
      Local<Value> elements[length];
      for (size_t i = 0; i < length; i += 1) {
        getdns_dict* reply;
        GETDNS(list_get_dict, isolate, answer, i, &reply);
        Local<Object> o = Object::New(isolate);
        uint32_t type;
        GETDNS(dict_get_int, isolate, reply, "type", &type);
        o->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "type"),
               Integer::New(isolate, type))
            .ToChecked();
        getdns_dict* rdata;
        GETDNS(dict_get_dict, isolate, reply, "rdata", &rdata);
        o->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "rdata"),
               RRToValue(context, type, rdata))
            .ToChecked();
        uint32_t ttl;
        GETDNS(dict_get_int, isolate, reply, "ttl", &ttl);
        o->Set(context,
               FIXED_ONE_BYTE_STRING(isolate, "ttl"),
               Integer::New(isolate, ttl))
            .ToChecked();
        elements[i] = o;
      }
      result
          ->Set(context,
                FIXED_ONE_BYTE_STRING(isolate, "answers"),
                Array::New(isolate, elements, length))
          .ToChecked();
    }
  }

  getdns_dict_destroy(response);

  req->resolve(result);
}

// static
void DNSWrap::GetAddresses(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  getdns_dict* extension = nullptr;
  DNSWrapRequest* req = new DNSWrapRequest(env->context());
  GETDNS(address,
         env->isolate(),
         dns->context_,
         *name,
         extension,
         req,
         nullptr,
         Callback);
  args.GetReturnValue().Set(req->promise());
}

// static
void DNSWrap::GetServices(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  getdns_dict* extension = nullptr;
  DNSWrapRequest* req = new DNSWrapRequest(env->context());
  GETDNS(service,
         env->isolate(),
         dns->context_,
         *name,
         extension,
         req,
         nullptr,
         Callback);
  args.GetReturnValue().Set(req->promise());
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
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  getdns_dict* address = getdns_dict_create();
  if (!ConvertAddress(env->isolate(), *name, address)) {
    return;
  }

  getdns_dict* extension = nullptr;
  DNSWrapRequest* req = new DNSWrapRequest(env->context());
  GETDNS(hostname,
         env->isolate(),
         dns->context_,
         address,
         extension,
         req,
         nullptr,
         Callback);
  args.GetReturnValue().Set(req->promise());
}

// static
void DNSWrap::GetGeneral(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsString());
  String::Utf8Value name(env->isolate(), args[0]);
  CHECK(args[1]->IsUint32());
  uint16_t type = args[1]->Uint32Value(env->context()).ToChecked();
  getdns_dict* extension = nullptr;
  DNSWrapRequest* req = new DNSWrapRequest(env->context());
  GETDNS(general,
         env->isolate(),
         dns->context_,
         *name,
         type,
         extension,
         req,
         nullptr,
         Callback);
  args.GetReturnValue().Set(req->promise());
}

// static
void DNSWrap::SetUpstreamRecursiveServers(
    const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsArray());
  Local<Array> a = args[0].As<Array>();
  uint32_t length = a->Length();

  getdns_list* list = getdns_list_create();

  for (uint32_t i = 0; i < length; i += 1) {
    getdns_dict* dict = getdns_dict_create();
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
    if (server->Length() == 3) {
      GETDNS(dict_set_int, env->isolate(), dict, "tls_port", port);
      String::Utf8Value hostname(
          env->isolate(), server->Get(env->context(), 2).ToLocalChecked());
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

  getdns_list_destroy(list);
}

// static
void DNSWrap::GetUpstreamRecursiveServers(
    const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);

  getdns_list* list;
  GETDNS(context_get_upstream_recursive_servers,
         env->isolate(),
         dns->context_,
         &list);
  // args.GetReturnValue().Set(ListToValue(env->isolate(), list));
}

// static
void DNSWrap::SetDNSTransportList(const FunctionCallbackInfo<Value>& args) {
  DNSWrap* dns;
  ASSIGN_OR_RETURN_UNWRAP(&dns, args.This());
  Environment* env = Environment::GetCurrent(args);

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

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> tmpl = env->NewFunctionTemplate(DNSWrap::New);
  auto dns_wrap_string = FIXED_ONE_BYTE_STRING(env->isolate(), "DNSWrap");
  tmpl->InstanceTemplate()->SetInternalFieldCount(DNSWrap::kInternalFieldCount);
  tmpl->SetClassName(dns_wrap_string);

  env->SetProtoMethod(tmpl, "getAddresses", DNSWrap::GetAddresses);
  env->SetProtoMethod(tmpl, "getServices", DNSWrap::GetServices);
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

  target
      ->Set(env->context(),
            dns_wrap_string,
            tmpl->GetFunction(context).ToLocalChecked())
      .ToChecked();

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
