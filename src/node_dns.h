#ifndef SRC_NODE_DNS_H_
#define SRC_NODE_DNS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "../deps/getdns/build/getdns/getdns_ext_libuv.h"
#include "base_object.h"
#include "node_mem.h"

namespace node {
namespace dns {

class DNSWrap : public BaseObject {
 public:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Callback(getdns_context*,
                       getdns_callback_type_t,
                       getdns_dict*,
                       void* data,
                       getdns_transaction_t);

  static void GetAddresses(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetServices(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetHostnames(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetGeneral(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetUpstreamRecursiveServers(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetUpstreamRecursiveServers(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetDNSTransportList(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override {}

  SET_MEMORY_INFO_NAME(DNSWrap)
  SET_SELF_SIZE(DNSWrap)

 private:
  DNSWrap(Environment* env, v8::Local<v8::Object> object);
  ~DNSWrap() override;
  getdns_context* context_ = nullptr;
};

}  // namespace dns
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DNS_H_
