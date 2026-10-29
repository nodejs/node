#include "crypto/crypto_client_hello.h"

#include <algorithm>

namespace node::crypto {

SSL* ClientHelloContext::ssl() const {
#ifdef OPENSSL_IS_BORINGSSL
  return handle_->ssl;
#else
  return handle_;
#endif
}

std::span<const uint8_t> ClientHelloContext::session_id() const {
#ifdef OPENSSL_IS_BORINGSSL
  return {handle_->session_id, handle_->session_id_len};
#else
  const uint8_t* id = nullptr;
  size_t len = SSL_client_hello_get0_session_id(handle_, &id);
  if (id == nullptr) return {};
  return {id, len};
#endif
}

bool ClientHelloContext::has_session_ticket() const {
  auto ext = extension(TLSEXT_TYPE_session_ticket);
  return ext.has_value() && !ext->empty();
}

std::optional<std::string_view> ClientHelloContext::servername() const {
  auto ext = extension(TLSEXT_TYPE_server_name);
  if (!ext.has_value()) return std::string_view();
  // RFC 6066: a 16-bit list length, then one entry of an 8-bit name type,
  // a 16-bit name length and that many name bytes. The constraints applied
  // here are the ones the TLS stack's own parser applies: exactly one
  // entry, of type host_name, of a sane length and free of NULs.
  auto list = ReadVector16(*ext);
  if (!list.has_value() || list->size() < 3) return std::nullopt;
  if ((*list)[0] != TLSEXT_NAMETYPE_host_name) return std::nullopt;
  auto name = ReadVector16(list->subspan(1));
  if (!name.has_value()) return std::nullopt;
  if (name->size() + 3 != list->size()) return std::nullopt;
  if (name->empty() || name->size() > TLSEXT_MAXLEN_host_name) {
    return std::nullopt;
  }
  if (std::find(name->begin(), name->end(), 0) != name->end()) {
    return std::nullopt;
  }
  return std::string_view(reinterpret_cast<const char*>(name->data()),
                          name->size());
}

std::span<const uint8_t> ClientHelloContext::alpn_protocols() const {
  auto ext = extension(TLSEXT_TYPE_application_layer_protocol_negotiation);
  if (!ext.has_value()) return {};
  // RFC 7301: a 16-bit list length, then the protocol names.
  return ReadVector16(*ext).value_or(std::span<const uint8_t>());
}

std::optional<std::span<const uint8_t>> ClientHelloContext::extension(
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

void ClientHelloContext::set_alert(int alert) const {
  if (alert_ != nullptr) *alert_ = alert;
}

ClientHelloContext::Result ClientHelloContext::Encode(
    ClientHelloResult result) {
  switch (result) {
    case ClientHelloResult::kContinue:
      return kContinueResult;
    case ClientHelloResult::kRetry:
      return kRetryResult;
    case ClientHelloResult::kFail:
      break;
  }
  return kFailResult;
}

std::optional<std::span<const uint8_t>> ClientHelloContext::ReadVector16(
    std::span<const uint8_t> data) {
  if (data.size() < 2) return std::nullopt;
  const size_t len = (static_cast<size_t>(data[0]) << 8) | data[1];
  if (data.size() < 2 + len) return std::nullopt;
  return data.subspan(2, len);
}

bool AlpnListContains(std::span<const uint8_t> protocols,
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

void SetSelectedProtocol(const unsigned char** out,
                         unsigned char* outlen,
                         std::string_view protocol) {
  *out = reinterpret_cast<const unsigned char*>(protocol.data());
  *outlen = static_cast<unsigned char>(protocol.size());
}

std::optional<std::string_view> SelectNextProtocol(
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
