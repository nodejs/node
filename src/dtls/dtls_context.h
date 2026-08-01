#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if HAVE_OPENSSL && HAVE_DTLS

#include <base_object.h>
#include <env.h>
#include <ncrypto.h>
#include <node_sockaddr.h>
#include <v8.h>

#include <openssl/dtls1.h>
#include <openssl/ssl.h>

#include <vector>

namespace node::dtls {

// DTLSContext wraps an SSL_CTX configured for DTLS.
// It manages certificate/key configuration, cipher selection,
// ALPN, and automatic cookie generation/verification for servers.
class DTLSContext final : public BaseObject {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void InitPerContext(v8::Local<v8::Object> target,
                             v8::Local<v8::Context> context,
                             Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  DTLSContext(Environment* env,
              v8::Local<v8::Object> wrap,
              SSL_CTX* ctx,
              bool is_server);

  SSL_CTX* ssl_ctx() const { return ctx_.get(); }

  // Set the peer address for cookie generation during DTLSv1_listen().
  void set_cookie_peer(const SocketAddress& addr) {
    current_cookie_peer_ = addr;
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(DTLSContext)
  SET_SELF_SIZE(DTLSContext)

 private:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetKey(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void AddCACert(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetCiphers(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetALPN(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSRTP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetVerifyMode(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadDefaultCAs(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetECDHCurve(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Compute the address-and-time-window-bound cookie for |window| into |out|
  // (which must have room for EVP_MAX_MD_SIZE bytes). Shared by the cookie
  // generate/verify callbacks.
  static bool ComputeCookie(SSL* ssl,
                            uint64_t window,
                            unsigned char* out,
                            unsigned int* out_len);

  // Automatic DTLS cookie callbacks
  static int CookieGenerateCallback(SSL* ssl,
                                    unsigned char* cookie,
                                    unsigned int* cookie_len);
  static int CookieVerifyCallback(SSL* ssl,
                                  const unsigned char* cookie,
                                  unsigned int cookie_len);

  // ALPN selection callback (server-side)
  static int ALPNSelectCallback(SSL* ssl,
                                const unsigned char** out,
                                unsigned char* outlen,
                                const unsigned char* in,
                                unsigned int inlen,
                                void* arg);

  ncrypto::SSLCtxPointer ctx_;
  bool is_server_;

  // Secret key for HMAC-based cookie generation
  std::vector<uint8_t> cookie_secret_;

  // Peer address for current DTLSv1_listen cookie exchange.
  // Set synchronously before DTLSv1_listen() and consumed by the
  // cookie generate/verify callbacks during that call.
  SocketAddress current_cookie_peer_;

  // ALPN protocols (server-side selection list)
  std::vector<uint8_t> alpn_protos_;
};

}  // namespace node::dtls

#endif  // HAVE_OPENSSL && HAVE_DTLS
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
