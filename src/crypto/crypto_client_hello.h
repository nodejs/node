#ifndef SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_
#define SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace node::crypto {

// Support for the early ClientHello callback, which the TLS library invokes
// once a complete ClientHello has been received but before version
// negotiation and before any session or ticket resumption.

enum class ClientHelloResult {
  kContinue,
  // Unwind out of the TLS library, leaving the handshake suspended. The
  // callback runs again from the start when the handshake is resumed.
  kRetry,
  // Abort the handshake.
  kFail,
};

// The ClientHello being processed, and the inputs from it that are usable
// this early. Instances are only valid for the duration of the callback.
class ClientHelloContext final {
 public:
#ifdef OPENSSL_IS_BORINGSSL
  using Handle = const SSL_CLIENT_HELLO*;
  using Result = ssl_select_cert_result_t;
  static constexpr Result kContinueResult = ssl_select_cert_success;
  static constexpr Result kRetryResult = ssl_select_cert_retry;
  static constexpr Result kFailResult = ssl_select_cert_error;
#else
  using Handle = SSL*;
  using Result = int;
  static constexpr Result kContinueResult = SSL_CLIENT_HELLO_SUCCESS;
  static constexpr Result kRetryResult = SSL_CLIENT_HELLO_RETRY;
  static constexpr Result kFailResult = SSL_CLIENT_HELLO_ERROR;
#endif

  explicit ClientHelloContext(Handle handle, int* alert = nullptr)
      : handle_(handle), alert_(alert) {}

  SSL* ssl() const;

  // The legacy_session_id. Note that a TLS 1.3 ClientHello carries a fake
  // one for middlebox compatibility, so a non-empty value here does not
  // mean the client is attempting session-id resumption.
  std::span<const uint8_t> session_id() const;

  bool has_session_ticket() const;

  // The host name to select an identity for, empty if the client sent no
  // server_name. Nothing at all if it sent one that cannot be used, in which
  // case the handshake should fail. SSL_get_servername() does not work this
  // early as the servername callback has not run yet.
  std::optional<std::string_view> servername() const;

  // ALPN protocols in wire format: each entry is a length byte followed by
  // that many name bytes.
  std::span<const uint8_t> alpn_protocols() const;

  std::optional<std::span<const uint8_t>> extension(unsigned int type) const;

  // Ignored by TLS libraries that choose the alert themselves.
  void set_alert(int alert) const;

  static Result Encode(ClientHelloResult result);

 private:
  // Reads a 16-bit length prefixed vector from the front of data. Returns
  // nothing if the length does not fit within data.
  static std::optional<std::span<const uint8_t>> ReadVector16(
      std::span<const uint8_t> data);

  Handle handle_;
  int* alert_;
};

// Adapts a portable callback into the one the linked TLS library expects.
template <ClientHelloResult (*Fn)(const ClientHelloContext&)>
struct ClientHelloCallback final {
#ifdef OPENSSL_IS_BORINGSSL
  static ssl_select_cert_result_t Invoke(const SSL_CLIENT_HELLO* hello) {
    return ClientHelloContext::Encode(Fn(ClientHelloContext(hello)));
  }
#else
  static int Invoke(SSL* ssl, int* alert, void* arg) {
    return ClientHelloContext::Encode(Fn(ClientHelloContext(ssl, alert)));
  }
#endif
};

// Registers Fn as the early ClientHello callback on ctx.
template <ClientHelloResult (*Fn)(const ClientHelloContext&)>
inline void SetClientHelloCallback(SSL_CTX* ctx) {
#ifdef OPENSSL_IS_BORINGSSL
  SSL_CTX_set_select_certificate_cb(ctx, ClientHelloCallback<Fn>::Invoke);
#else
  SSL_CTX_set_client_hello_cb(ctx, ClientHelloCallback<Fn>::Invoke, nullptr);
#endif
}

bool AlpnListContains(std::span<const uint8_t> protocols,
                      std::string_view protocol);

void SetSelectedProtocol(const unsigned char** out,
                         unsigned char* outlen,
                         std::string_view protocol);

// Selects the protocol to use from the offered and supported lists, both in
// ALPN wire format. Returns nothing when the two lists do not overlap.
std::optional<std::string_view> SelectNextProtocol(
    std::span<const uint8_t> supported, std::span<const uint8_t> offered);

}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_
