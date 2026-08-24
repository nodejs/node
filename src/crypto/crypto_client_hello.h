#ifndef SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_
#define SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace node::crypto {

// Support for the early ClientHello callback, which the TLS library invokes
// once a complete ClientHello has been received but before version
// negotiation and before any session or ticket resumption. This lets us
// inspect what the client asks for and prepare our configuration (with JS
// involvement potentially) before we actually process the handshake.
//
// OpenSSL and BoringSSL disagree on various details, which are centralized
// here so callers can ignore the differences.

// What the callback wants the TLS library to do next.
enum class ClientHelloResult {
  // Carry on with the handshake.
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
#else
  using Handle = SSL*;
  using Result = int;
#endif

  explicit ClientHelloContext(Handle handle, int* alert = nullptr)
      : handle_(handle), alert_(alert) {}

  inline SSL* ssl() const {
#ifdef OPENSSL_IS_BORINGSSL
    return handle_->ssl;
#else
    return handle_;
#endif
  }

  // The legacy_session_id. Note that a TLS 1.3 ClientHello carries a fake
  // one for middlebox compatibility, so a non-empty value here does not
  // mean the client is attempting session-id resumption.
  inline std::span<const uint8_t> session_id() const {
#ifdef OPENSSL_IS_BORINGSSL
    return {handle_->session_id, handle_->session_id_len};
#else
    const uint8_t* id = nullptr;
    size_t len = SSL_client_hello_get0_session_id(handle_, &id);
    if (id == nullptr) return {};
    return {id, len};
#endif
  }

  inline bool has_session_ticket() const {
    auto ext = extension(TLSEXT_TYPE_session_ticket);
    return ext.has_value() && !ext->empty();
  }

  // The host_name from the server_name extension, or an empty view when the
  // client did not send a usable one. SSL_get_servername() cannot be used
  // this early because the servername callback has not run yet, so this
  // applies the same constraints that callback's parser would: exactly one
  // entry, of type host_name, of a sane length and free of NULs. Anything
  // else reads as absent here and is rejected outright by the TLS stack a
  // moment later, once it parses extensions for itself.
  inline std::string_view servername() const {
    auto ext = extension(TLSEXT_TYPE_server_name);
    if (!ext.has_value()) return {};
    // RFC 6066: a 16-bit list length, then one entry of an 8-bit name type,
    // a 16-bit name length and that many name bytes.
    auto list = ReadVector16(*ext);
    if (!list.has_value() || list->size() < 3) return {};
    if ((*list)[0] != TLSEXT_NAMETYPE_host_name) return {};
    auto name = ReadVector16(list->subspan(1));
    if (!name.has_value()) return {};
    // The entry must be the only one, and must be a plausible host name.
    if (name->size() + 3 != list->size()) return {};
    if (name->empty() || name->size() > TLSEXT_MAXLEN_host_name) return {};
    if (std::find(name->begin(), name->end(), 0) != name->end()) return {};
    return {reinterpret_cast<const char*>(name->data()), name->size()};
  }

  // The protocols the client offered, in ALPN wire format: each entry is a
  // length byte followed by that many name bytes. Empty when the client did
  // not offer ALPN. This is the format SelectNextProtocol() expects.
  inline std::span<const uint8_t> alpn_protocols() const {
    auto ext = extension(TLSEXT_TYPE_application_layer_protocol_negotiation);
    if (!ext.has_value()) return {};
    // RFC 7301: a 16-bit list length, then the protocol names.
    return ReadVector16(*ext).value_or(std::span<const uint8_t>());
  }

  // The raw body of an extension the client sent, or nothing when the
  // client did not send that extension.
  inline std::optional<std::span<const uint8_t>> extension(
      unsigned int type) const {
    const uint8_t* data = nullptr;
    size_t len = 0;
#ifdef OPENSSL_IS_BORINGSSL
    if (!SSL_early_callback_ctx_extension_get(handle_, type, &data, &len)) {
      return std::nullopt;
    }
#else
    if (SSL_client_hello_get0_ext(handle_, type, &data, &len) != 1) {
      return std::nullopt;
    }
#endif
    return std::span<const uint8_t>(data, len);
  }

  // Sets the TLS alert to send if the callback fails. Ignored by TLS
  // libraries that choose the alert themselves.
  inline void set_alert(int alert) const {
    if (alert_ != nullptr) *alert_ = alert;
  }

  static inline Result Encode(ClientHelloResult result) {
#ifdef OPENSSL_IS_BORINGSSL
    switch (result) {
      case ClientHelloResult::kContinue:
        return ssl_select_cert_success;
      case ClientHelloResult::kRetry:
        return ssl_select_cert_retry;
      case ClientHelloResult::kFail:
        break;
    }
    return ssl_select_cert_error;
#else
    switch (result) {
      case ClientHelloResult::kContinue:
        return SSL_CLIENT_HELLO_SUCCESS;
      case ClientHelloResult::kRetry:
        return SSL_CLIENT_HELLO_RETRY;
      case ClientHelloResult::kFail:
        break;
    }
    return SSL_CLIENT_HELLO_ERROR;
#endif
  }

 private:
  // Reads a 16-bit length prefixed vector from the front of data. Returns
  // nothing if the length does not fit within data.
  static inline std::optional<std::span<const uint8_t>> ReadVector16(
      std::span<const uint8_t> data) {
    if (data.size() < 2) return std::nullopt;
    const size_t len = (static_cast<size_t>(data[0]) << 8) | data[1];
    if (data.size() < 2 + len) return std::nullopt;
    return data.subspan(2, len);
  }

  Handle handle_;
  int* alert_;
};

// Adapts a portable callback into the one the linked TLS library expects,
// e.g. SSL_CTX_set_client_hello_cb(ctx, ClientHelloCallback<Fn>::Invoke,
// nullptr).
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

// Whether protocols, a list in ALPN wire format, contains protocol.
inline bool AlpnListContains(std::span<const uint8_t> protocols,
                             std::string_view protocol) {
  for (size_t n = 0; n < protocols.size();) {
    const size_t len = protocols[n];
    if (len == 0 || n + 1 + len > protocols.size()) return false;
    if (std::string_view(
            reinterpret_cast<const char*>(protocols.data() + n + 1), len) ==
        protocol) {
      return true;
    }
    n += 1 + len;
  }
  return false;
}

inline void SetSelectedProtocol(const unsigned char** out,
                                unsigned char* outlen,
                                std::string_view protocol) {
  *out = reinterpret_cast<const unsigned char*>(protocol.data());
  *outlen = static_cast<unsigned char>(protocol.size());
}

// Selects the protocol to use from the offered and supported lists, both in
// ALPN wire format. Returns nothing when the two lists do not overlap.
inline std::optional<std::string_view> SelectNextProtocol(
    std::span<const uint8_t> supported, std::span<const uint8_t> offered) {
  if (supported.empty() || offered.empty()) return std::nullopt;
  uint8_t* selected = nullptr;
  uint8_t selected_len = 0;
  if (SSL_select_next_proto(&selected,
                            &selected_len,
                            supported.data(),
                            static_cast<unsigned int>(supported.size()),
                            offered.data(),
                            static_cast<unsigned int>(offered.size())) !=
      OPENSSL_NPN_NEGOTIATED) {
    return std::nullopt;
  }
  return std::string_view(reinterpret_cast<const char*>(selected),
                          selected_len);
}

}  // namespace node::crypto

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CRYPTO_CRYPTO_CLIENT_HELLO_H_
