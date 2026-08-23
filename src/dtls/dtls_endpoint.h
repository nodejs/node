#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if HAVE_OPENSSL && HAVE_DTLS

#include <aliased_struct.h>
#include <env.h>
#include <handle_wrap.h>
#include <node_sockaddr.h>
#include <uv.h>
#include <v8.h>

#include <openssl/ssl.h>

#include <unordered_map>
#include <vector>

#include "dtls.h"
#include "dtls_context.h"
#include "dtls_session.h"

namespace node::dtls {

// Shared C++ <-> JS state for a DTLS endpoint.
struct DTLSEndpointStateData {
  uint8_t bound = 0;
  uint8_t listening = 0;
  uint8_t closing = 0;
  uint8_t destroyed = 0;
  uint32_t session_count = 0;
  uint8_t busy = 0;
};

// Stats collected for a DTLS endpoint, backed by a BigUint64Array.
struct DTLSEndpointStats {
  DTLS_ENDPOINT_STATS(DTLS_STAT_FIELD)
};

// DTLSEndpoint manages a single UDP socket and dispatches incoming
// datagrams to the appropriate DTLSSession based on the remote address.
// For server mode, it handles stateless cookie exchange via DTLSv1_listen()
// before creating new sessions.
class DTLSEndpoint final : public HandleWrap {
 public:
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void InitPerContext(v8::Local<v8::Object> target,
                             v8::Local<v8::Context> context,
                             Environment* env);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  DTLSEndpoint(Environment* env, v8::Local<v8::Object> wrap);

  // Bind the UDP socket to the given address.
  int Bind(const SocketAddress& address);

  // Start listening for incoming DTLS connections (server mode).
  // |context| provides the SSL_CTX for creating new sessions.
  int Listen(DTLSContext* context);

  // Initiate a client connection to the given address.
  // |servername|/|verify_host|/|verify_is_ip| configure SNI and peer identity
  // verification on the client SSL before the handshake begins; see
  // DTLSSession::Create.
  // Returns the created DTLSSession.
  BaseObjectPtr<DTLSSession> Connect(
      DTLSContext* context,
      const SocketAddress& remote,
      const char* servername = nullptr,
      const char* verify_host = nullptr,
      bool verify_is_ip = false,
      const ncrypto::Buffer<const unsigned char>& resume = {});

  // Send a raw UDP datagram to the given address.
  // Called by DTLSSession to send encrypted packets.
  int SendTo(const SocketAddress& dest, const uint8_t* data, size_t len);

  // Remove a session from the endpoint (called on session close/destroy).
  void RemoveSession(const SocketAddress& addr);

  // Close the endpoint gracefully (close all sessions first).
  void CloseGracefully();

  // Immediately destroy the endpoint.
  void Destroy();

  // Get the JS callback function for a given callback index.
  v8::Local<v8::Function> GetCallback(int index) const;

  // Set the JS callbacks.
  void SetCallbacks(v8::Local<v8::Object> callbacks);

  bool is_listening() const { return listening_; }
  uint32_t mtu() const { return mtu_; }
  uint64_t handshake_timeout() const { return handshake_timeout_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(DTLSEndpoint)
  SET_SELF_SIZE(DTLSEndpoint)

 private:
  // JS binding methods
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoBind(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoListen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoConnect(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoDestroy(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetState(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStats(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetAddress(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetMTU(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetHandshakeTimeout(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetSessionLimits(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DoSetCallbacks(const v8::FunctionCallbackInfo<v8::Value>& args);

  // libuv callbacks
  static void OnAlloc(uv_handle_t* handle,
                      size_t suggested_size,
                      uv_buf_t* buf);
  static void OnRecv(uv_udp_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buf,
                     const struct sockaddr* addr,
                     unsigned int flags);
  static void OnSend(uv_udp_send_t* req, int status);

  // Called by HandleWrap after uv_close completes.
  void OnClose() override;

  // Process an incoming datagram.
  void ProcessDatagram(const uint8_t* data,
                       size_t len,
                       const SocketAddress& remote);

  // True if another server session can be accepted from this address without
  // exceeding either cap.
  bool HasCapacityFor(const SocketAddress& remote) const;

  // True if the datagram could plausibly be a ClientHello. Used to reject
  // obvious junk before allocating an SSL for it.
  static bool CouldBeClientHello(const uint8_t* data, size_t len);

  // Handle a new client connection (server mode).
  void AcceptConnection(const uint8_t* data,
                        size_t len,
                        const SocketAddress& remote);

  uv_udp_t handle_;

  // Reusable receive buffer for uv_udp_recv (see OnAlloc). libuv delivers one
  // datagram at a time and OnRecv consumes each before the next OnAlloc, so a
  // single buffer per endpoint avoids a heap allocation on every packet.
  std::vector<char> recv_buf_;

  // Session table: remote address -> session. PeerMap and not Map: two
  // datagrams from one peer must find one session, and operator== would treat
  // them as different peers over a sin6_flowinfo the sender is free to vary.
  SocketAddress::PeerMap<BaseObjectPtr<DTLSSession>> sessions_;

  // Concurrent sessions per source IP, ignoring port, so one host cannot fill
  // the whole table. Entries are erased when their count reaches zero, so this
  // stays proportional to the number of distinct peers, not to peers ever
  // seen.
  SocketAddress::IpMap<uint32_t> sessions_per_host_;

  // Server context (set when listening).
  BaseObjectPtr<DTLSContext> server_context_;

  // JS callbacks
  v8::Global<v8::Function> callbacks_[DTLS_CB_COUNT];

  AliasedStruct<DTLSEndpointStateData> state_;
  AliasedStruct<DTLSEndpointStats> stats_;

  // Strong self-reference held while a graceful close/destroy is in flight, so
  // the wrapper is not garbage-collected before OnClose() runs and reports the
  // close. Cleared in OnClose().
  BaseObjectPtr<DTLSEndpoint> self_ref_;

  bool listening_ = false;
  // Maximum size of a DTLS datagram, i.e. the UDP payload, not the
  // application payload -- a record carries less than this once its header
  // and MAC are counted. Read by DTLSSession when it creates its SSL, so a
  // change only affects sessions created afterwards.
  uint32_t mtu_ = 1200;

  // Milliseconds a handshake may take before it is abandoned; 0 disables it.
  uint64_t handshake_timeout_ = 60000;

  // Caps on accepted server sessions. Zero means unlimited.
  uint32_t max_sessions_ = 0;
  uint32_t max_sessions_per_host_ = 0;
};

}  // namespace node::dtls

#endif  // HAVE_OPENSSL && HAVE_DTLS
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
