#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <env.h>
#include <ngtcp2/ngtcp2.h>
#include <node_internals.h>
#include <v8.h>
#include <string>

namespace node {
namespace quic {

// PreferredAddress is a helper class used only when a client Session receives
// an advertised preferred address from a server. The helper provides
// information about the server advertised preferred address and allows
// the preferred address to be selected.
class PreferredAddress final {
 public:
  enum class Policy {
    // Ignore the server-advertised preferred address.
    IGNORE_PREFERRED_ADDRESS,
    // Use the server-advertised preferred address.
    USE_PREFERRED_ADDRESS,
  };

  // The QUIC_* constants are expected to be exported out to be used on
  // the JavaScript side of the API.
  static constexpr uint32_t QUIC_PREFERRED_ADDRESS_USE =
      static_cast<uint32_t>(Policy::USE_PREFERRED_ADDRESS);
  static constexpr uint32_t QUIC_PREFERRED_ADDRESS_IGNORE =
      static_cast<uint32_t>(Policy::IGNORE_PREFERRED_ADDRESS);

  static v8::Maybe<Policy> GetPolicy(Environment* env,
                                     v8::Local<v8::Value> value);

  struct AddressInfo final {
    char host[NI_MAXHOST];
    int family;
    uint16_t port;
    std::string_view address;
  };

  explicit PreferredAddress(ngtcp2_path* dest,
                            const ngtcp2_preferred_addr* paddr);
  PreferredAddress(const PreferredAddress&) = delete;
  PreferredAddress(PreferredAddress&&) = delete;
  PreferredAddress& operator=(const PreferredAddress&) = delete;
  PreferredAddress& operator=(PreferredAddress&&) = delete;

  void Use(const AddressInfo& address);

  std::optional<const AddressInfo> ipv4() const;
  std::optional<const AddressInfo> ipv6() const;

  // Set the preferred address in the transport params.
  // The address family (ipv4 or ipv6) will be automatically
  // detected from the given addr. Any other address family
  // will be ignored.
  static void Set(ngtcp2_transport_params* params, const sockaddr* addr);

 private:
  ngtcp2_path* dest_;
  const ngtcp2_preferred_addr* paddr_;
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
