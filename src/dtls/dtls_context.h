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

#include <string>
#include <unordered_map>
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
              ncrypto::SSLCtxPointer ctx,
              bool is_server);

  SSL_CTX* ssl_ctx() const { return ctx_.get(); }

  // Record this context as the one an SSL was created from, so the callbacks
  // OpenSSL invokes without an argument of their own can find it again. Bound
  // to the SSL rather than the SSL_CTX because SNI reassigns the latter.
  void BindToSSL(SSL* ssl);


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
  static void SetSessionIdContext(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSNIContexts(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetTicketKeys(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetPSK(const v8::FunctionCallbackInfo<v8::Value>& args);

  // RFC 4279 pre-shared keys. The identity map is consulted first and needs
  // no call into JavaScript; the callback, if there is one, is only reached
  // when the map does not answer. Both are synchronous -- OpenSSL wants the
  // key returned from the callback, and node:tls does the same.
  static unsigned int PSKServerCallback(SSL* ssl,
                                        const char* identity,
                                        unsigned char* psk,
                                        unsigned int max_psk_len);
  static unsigned int PSKClientCallback(SSL* ssl,
                                        const char* hint,
                                        char* identity,
                                        unsigned int max_identity_len,
                                        unsigned char* psk,
                                        unsigned int max_psk_len);

  static void ReportCallbackError(SSL* ssl, v8::TryCatch* try_catch);

  DTLSContext* SelectSNIContextFromCallback(SSL* ssl, const char* servername);
  size_t sni_contexts_size() const { return sni_contexts_.size(); }

  // Recover the context a callback without an argument slot belongs to.
  static DTLSContext* FromSSL(SSL* ssl);

  // Server Name Indication. Selection is a lookup in sni_contexts_ with no
  // call into JavaScript, so a handshake never has to be suspended and
  // re-entered the way node:tls does it.
  static int SNISelectCallback(SSL* ssl, int* ad, void* arg);

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
  //
  // Value-initialised: SocketAddress's default constructor is `= default`,
  // which leaves its sockaddr_storage holding whatever was on the heap. Every
  // path sets this before the callbacks run, so reading it uninitialised is
  // not reachable today, but a zeroed family makes CanonicalizeAddress fail
  // closed instead of deriving a cookie from stale bytes.
  SocketAddress current_cookie_peer_{};

  // ALPN protocols (server-side selection list)
  std::vector<uint8_t> alpn_protos_;

  // Host name -> context, for SNI. Empty unless the application supplied an
  // sni map. The "*" key, if present, is the fallback for names that do not
  // match; without it an unmatched name is refused.
  std::unordered_map<std::string, BaseObjectPtr<DTLSContext>> sni_contexts_;

  // PSK identity -> key (server), and the single identity a client presents.
  std::unordered_map<std::string, std::vector<unsigned char>> psk_identities_;
  std::string psk_identity_hint_;
  std::string psk_client_identity_;
  std::vector<unsigned char> psk_client_key_;

  // Consulted only when the map does not answer.
  v8::Global<v8::Function> psk_callback_;
  v8::Global<v8::Function> sni_callback_;
};

}  // namespace node::dtls

#endif  // HAVE_OPENSSL && HAVE_DTLS
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
