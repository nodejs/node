#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <env.h>
#include <ngtcp2/ngtcp2.h>
#include <node_internals.h>
#include <node_realm.h>
#include <v8.h>
#include <string>
#include "defs.h"

namespace node::quic {

// PreferredAddress is a helper class used only when a client Session receives
// an advertised preferred address from a server. The helper provides
// information about the server advertised preferred address and allows
// the preferred address to be selected.
class PreferredAddress final {
 public:
  enum class Policy : uint8_t {
    // Ignore the server-advertised preferred address.
    IGNORE_PREFERRED,
    // Use the server-advertised preferred address.
    USE_PREFERRED,
  };

  static v8::Maybe<Policy> tryGetPolicy(Environment* env,
                                        v8::Local<v8::Value> value);
  static inline v8::Maybe<Policy> tryGetPolicy(Realm* realm,
                                               v8::Local<v8::Value> value) {
    return tryGetPolicy(realm->env(), value);
  }

  static void Initialize(Environment* env, v8::Local<v8::Object> target);
  static inline void Initialize(Realm* realm, v8::Local<v8::Object> target) {
    return Initialize(realm->env(), target);
  }

  struct AddressInfo final {
    char host[NI_MAXHOST];
    int family;
    uint16_t port;
    std::string_view address;
  };

  explicit PreferredAddress(ngtcp2_path* dest,
                            const ngtcp2_preferred_addr* paddr);
  DISALLOW_COPY_AND_MOVE(PreferredAddress)
  void* operator new(size_t) = delete;
  void operator delete(void*) = delete;

  void Use(const AddressInfo& address);

  std::optional<const AddressInfo> ipv4() const;
  std::optional<const AddressInfo> ipv6() const;

  const CID cid() const;

  // Set the preferred address in the transport params.
  // The address family (ipv4 or ipv6) will be automatically
  // detected from the given addr. Any other address family
  // will be ignored.
  static void Set(ngtcp2_transport_params* params, const sockaddr* addr);

 private:
  ngtcp2_path* dest_;
  const ngtcp2_preferred_addr* paddr_;
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
