#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if HAVE_OPENSSL && HAVE_DTLS

#include <aliased_struct.h>
#include <async_wrap.h>
#include <base_object.h>
#include <env.h>
#include <ncrypto.h>
#include <node_sockaddr.h>
#include <timer_wrap.h>
#include <uv.h>
#include <v8.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include "dtls.h"

namespace node::dtls {

class DTLSEndpoint;

// Shared C++ <-> JS state for a DTLS session.
struct DTLSSessionStateData {
  uint8_t handshaking = 0;
  uint8_t open = 0;
  uint8_t closing = 0;
  uint8_t destroyed = 0;
  uint8_t has_message_listener = 0;
};

// Stats collected for a DTLS session, backed by a BigUint64Array.
struct DTLSSessionStats {
  DTLS_SESSION_STATS(DTLS_STAT_FIELD)
};

// DTLSSession represents a single DTLS association with a remote peer.
// It wraps an OpenSSL SSL* object configured for DTLS, using memory BIOs
// to interface with the endpoint's UDP socket.
class DTLSSession final : public AsyncWrap {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void InitPerContext(v8::Local<v8::Object> target,
                             v8::Local<v8::Context> context,
                             Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  // Create a new DTLS session.
  // |endpoint| - the owning endpoint (for sending packets)
  // |ssl_ctx| - the SSL_CTX to create the SSL* from
  // |remote| - the peer address
  // |is_server| - true if this is a server-side session
  // |servername|   - SNI to advertise (client only); nullptr to omit.
  // |verify_host|  - expected peer identity to verify (client only);
  //                  nullptr disables identity checking.
  // |verify_is_ip| - true if |verify_host| is an IP literal (verified
  //                  against iPAddress SANs) rather than a DNS name.
  static BaseObjectPtr<DTLSSession> Create(Environment* env,
                                           DTLSEndpoint* endpoint,
                                           SSL_CTX* ssl_ctx,
                                           const SocketAddress& remote,
                                           bool is_server,
                                           const char* servername = nullptr,
                                           const char* verify_host = nullptr,
                                           bool verify_is_ip = false);

  // Create a session from an already-initialized SSL object.
  // Used by the server after DTLSv1_listen() returns 1 — the SSL
  // has already verified the cookie and is ready to continue.
  static BaseObjectPtr<DTLSSession> CreateFromSSL(Environment* env,
                                                  DTLSEndpoint* endpoint,
                                                  ncrypto::SSLPointer ssl,
                                                  BIO* enc_in,
                                                  BIO* enc_out,
                                                  const SocketAddress& remote);

  ~DTLSSession() override;

  // Called by the endpoint when a datagram arrives from this session's peer.
  void Receive(const uint8_t* data, size_t len);

  // Send application data to the peer.
  int Send(const uint8_t* data, size_t len);

  // Initiate a graceful shutdown (sends close_notify).
  void Close();

  // Immediately destroy the session without sending close_notify.
  void Destroy();

  const SocketAddress& remote_address() const { return remote_address_; }
  bool is_server() const { return is_server_; }
  bool is_handshake_complete() const { return handshake_complete_; }
  bool is_closed() const { return closed_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(DTLSSession)
  SET_SELF_SIZE(DTLSSession)

  // Public constructor required by MakeBaseObject<>.
  DTLSSession(Environment* env,
              v8::Local<v8::Object> wrap,
              DTLSEndpoint* endpoint,
              ncrypto::SSLPointer ssl,
              BIO* enc_in,
              BIO* enc_out,
              const SocketAddress& remote,
              bool is_server);

 private:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSend(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoDestroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStats(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetRemoteAddress(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetProtocol(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCipher(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPeerCertificate(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetALPNProtocol(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ExportKeyingMaterial(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSRTPProfile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetServername(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetServername(const v8::FunctionCallbackInfo<v8::Value>& args);

 public:
  // The core state machine pump. Processes pending OpenSSL I/O:
  //   1. ClearOut() - SSL_read() -> emit decrypted data to JS
  //   2. ClearIn()  - SSL_write() pending cleartext
  //   3. EncOut()   - read enc_out_ BIO -> send via endpoint UDP
  //   4. UpdateTimer() - schedule retransmit timer if needed
  void Cycle();

 private:
  // Read decrypted application data from OpenSSL and emit to JS.
  void ClearOut();

  // Flush encrypted data from enc_out_ BIO and send via the endpoint.
  void EncOut();

  // Update the DTLS retransmission timer based on OpenSSL's timeout.
  void UpdateTimer();

  // OpenSSL keylog callback.
  static void SSLKeylogCallback(const SSL* ssl, const char* line);

  // Emit a callback to JS via the endpoint's callback dispatch.
  v8::MaybeLocal<v8::Value> EmitCallback(int cb_index,
                                         int argc,
                                         v8::Local<v8::Value>* argv);

  BaseObjectWeakPtr<DTLSEndpoint> endpoint_;
  ncrypto::SSLPointer ssl_;

  // Memory BIOs: encrypted data flows through these.
  // enc_in_: network datagrams written here -> SSL_read() extracts cleartext
  // enc_out_: SSL_write() puts ciphertext here -> we read and send via UDP
  BIO* enc_in_ = nullptr;
  BIO* enc_out_ = nullptr;

  TimerWrapHandle retransmit_timer_;

  SocketAddress remote_address_;
  bool is_server_;
  bool handshake_complete_ = false;
  bool closed_ = false;
  bool destroyed_ = false;
  int cycle_depth_ = 0;

  AliasedStruct<DTLSSessionStateData> state_;
  AliasedStruct<DTLSSessionStats> stats_;
};

}  // namespace node::dtls

#endif  // HAVE_OPENSSL && HAVE_DTLS
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
