#ifndef SRC_NODE_DNS_H_
#define SRC_NODE_DNS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include "async_wrap-inl.h"
#include "getdns/getdns_ext_libuv.h"
#include "node_mem.h"

namespace node {
namespace dns {

struct Allocator {
  void* data;
  void* (*malloc)(size_t size, void* data);
  void (*free)(void* ptr, void* data);
  void* (*calloc)(size_t num, size_t size, void* data);
  void* (*realloc)(void* ptr, size_t size, void* data);
};

class DNSWrap : public AsyncWrap,
                public mem::NgLibMemoryManager<DNSWrap, Allocator> {
 public:
  static void New(const v8::FunctionCallbackInfo<v8::Value>&);

  static void GetAddresses(const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetServices(const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetHostnames(const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetGeneral(const v8::FunctionCallbackInfo<v8::Value>&);

  static void SetUpstreamRecursiveServers(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void GetUpstreamRecursiveServers(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void SetDNSTransportList(const v8::FunctionCallbackInfo<v8::Value>&);

  static void CancelAllTransactions(const v8::FunctionCallbackInfo<v8::Value>&);

  void MemoryInfo(MemoryTracker*) const override;
  SET_MEMORY_INFO_NAME(DNSWrap)
  SET_SELF_SIZE(DNSWrap)

  void CheckAllocatedSize(size_t) const;
  void IncreaseAllocatedSize(size_t);
  void DecreaseAllocatedSize(size_t);

 private:
  v8::Local<v8::Value> RegisterTransaction(getdns_transaction_t);
  void ResolveTransaction(getdns_transaction_t, v8::Local<v8::Value>);
  void RejectTransaction(getdns_transaction_t, v8::Local<v8::Value>);

  static void Callback(getdns_context*,
                       getdns_callback_type_t,
                       getdns_dict*,
                       void* data,
                       getdns_transaction_t);

  struct Transaction {
    // Prevent gc while transactions are live
    v8::Global<v8::Object> wrap;
    v8::Global<v8::Promise::Resolver> resolver;
  };

  DNSWrap(Environment* env, v8::Local<v8::Object> object, int32_t timeout);
  ~DNSWrap() override;
  Allocator allocator_;
  size_t current_getdns_memory_ = 0;
  getdns_context* context_ = nullptr;
  std::unordered_map<getdns_transaction_t, Transaction> transactions_;
};

}  // namespace dns
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DNS_H_
