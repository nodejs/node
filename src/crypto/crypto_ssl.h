#ifndef SRC_CRYPTO_CRYPTO_SSL_H_
#define SRC_CRYPTO_CRYPTO_SSL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "crypto/crypto_context.h"
#include "crypto/crypto_clienthello.h"
#include "crypto/crypto_util.h"
#include "env.h"
#include "v8.h"

namespace node {
namespace crypto {
// SSLWrap implicitly depends on the inheriting class' handle having an
// internal pointer to the Base class.
template <class Base>
class SSLWrap {
 public:
  enum Kind {
    kClient,
    kServer
  };

  SSLWrap(Environment* env, SecureContext* sc, Kind kind)
      : env_(env),
        kind_(kind),
        next_sess_(nullptr),
        session_callbacks_(false),
        awaiting_new_session_(false),
        cert_cb_(nullptr),
        cert_cb_arg_(nullptr),
        cert_cb_running_(false) {
    ssl_.reset(SSL_new(sc->ctx_.get()));
    CHECK(ssl_);
    env_->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
  }

  virtual ~SSLWrap() {
    DestroySSL();
  }

  inline void enable_session_callbacks() { session_callbacks_ = true; }
  inline bool is_server() const { return kind_ == kServer; }
  inline bool is_client() const { return kind_ == kClient; }
  inline bool is_awaiting_new_session() const { return awaiting_new_session_; }
  inline bool is_waiting_cert_cb() const { return cert_cb_ != nullptr; }

  void MemoryInfo(MemoryTracker* tracker) const;

 protected:
  typedef void (*CertCb)(void* arg);

  // OpenSSL structures are opaque. Estimate SSL memory size for OpenSSL 1.1.1b:
  //   SSL: 6224
  //   SSL->SSL3_STATE: 1040
  //   ...some buffers: 42 * 1024
  // NOTE: Actually it is much more than this
  static const int64_t kExternalSize = 6224 + 1040 + 42 * 1024;

  static void ConfigureSecureContext(SecureContext* sc);
  static void AddMethods(Environment* env, v8::Local<v8::FunctionTemplate> t);

  static SSL_SESSION* GetSessionCallback(SSL* s,
                                         const unsigned char* key,
                                         int len,
                                         int* copy);
  static int NewSessionCallback(SSL* s, SSL_SESSION* sess);
  static void KeylogCallback(const SSL* s, const char* line);
  static void OnClientHello(void* arg,
                            const ClientHelloParser::ClientHello& hello);

  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCertificate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFinished(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPeerFinished(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoadSession(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void IsSessionReused(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void VerifyError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCipher(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSharedSigalgs(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportKeyingMaterial(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EndParser(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CertCbDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Renegotiate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetTLSTicket(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NewSessionDone(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetOCSPResponse(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RequestOCSP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetEphemeralKeyInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetProtocol(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef SSL_set_max_send_fragment
  static void SetMaxSendFragment(
      const v8::FunctionCallbackInfo<v8::Value>& args);
#endif  // SSL_set_max_send_fragment

  static void GetALPNNegotiatedProto(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetALPNProtocols(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int SelectALPNCallback(SSL* s,
                                const unsigned char** out,
                                unsigned char* outlen,
                                const unsigned char* in,
                                unsigned int inlen,
                                void* arg);
  static int TLSExtStatusCallback(SSL* s, void* arg);
  static int SSLCertCallback(SSL* s, void* arg);

  void DestroySSL();
  void WaitForCertCb(CertCb cb, void* arg);
  int SetCACerts(SecureContext* sc);

  inline Environment* ssl_env() const {
    return env_;
  }

  Environment* const env_;
  Kind kind_;
  SSLSessionPointer next_sess_;
  SSLPointer ssl_;
  bool session_callbacks_;
  bool awaiting_new_session_;

  // SSL_set_cert_cb
  CertCb cert_cb_;
  void* cert_cb_arg_;
  bool cert_cb_running_;

  ClientHelloParser hello_parser_;

  v8::Global<v8::ArrayBufferView> ocsp_response_;
  BaseObjectPtr<SecureContext> sni_context_;

  friend class SecureContext;
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_CRYPTO_CRYPTO_SSL_H_
