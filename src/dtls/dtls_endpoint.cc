#include "dtls_endpoint.h"
#include "dtls.h"
#include "dtls_context.h"
#include "dtls_session.h"

#if HAVE_OPENSSL && HAVE_DTLS

#include <aliased_struct-inl.h>
#include <base_object-inl.h>
#include <env-inl.h>
#include <handle_wrap.h>
#include <memory_tracker-inl.h>
#include <node_buffer.h>
#include <node_errors.h>
#include <node_sockaddr-inl.h>
#include <permission/permission.h>
#include <util-inl.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <cstddef>
#include <cstring>

namespace node {

using v8::ArrayBufferView;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace dtls {

namespace {
struct SendReq {
  uv_udp_send_t req;
  uv_buf_t buf;
  std::vector<uint8_t> data;
};
}  // namespace

// The endpoint state "indices" are byte offsets into DTLSEndpointStateData,
// accessed from JS via a DataView. Pin them to the actual struct layout so a
// mismatch (as once existed for `busy`, which follows a uint32) can't recur.
static_assert(IDX_ENDPOINT_STATE_BOUND ==
              offsetof(DTLSEndpointStateData, bound));
static_assert(IDX_ENDPOINT_STATE_LISTENING ==
              offsetof(DTLSEndpointStateData, listening));
static_assert(IDX_ENDPOINT_STATE_CLOSING ==
              offsetof(DTLSEndpointStateData, closing));
static_assert(IDX_ENDPOINT_STATE_DESTROYED ==
              offsetof(DTLSEndpointStateData, destroyed));
static_assert(IDX_ENDPOINT_STATE_SESSION_COUNT ==
              offsetof(DTLSEndpointStateData, session_count));
static_assert(IDX_ENDPOINT_STATE_BUSY == offsetof(DTLSEndpointStateData, busy));

DTLSEndpoint::DTLSEndpoint(Environment* env, Local<Object> wrap)
    : HandleWrap(env,
                 wrap,
                 reinterpret_cast<uv_handle_t*>(&handle_),
                 PROVIDER_DTLS_ENDPOINT),
      state_(env->isolate()),
      stats_(env->isolate()) {
  CHECK_EQ(uv_udp_init(env->event_loop(), &handle_), 0);
  handle_.data = this;
  MakeWeak();
  DTLS_STAT_RECORD_TIMESTAMP(DTLSEndpointStats, created_at);
}

Local<FunctionTemplate> DTLSEndpoint::GetConstructorTemplate(Environment* env) {
  auto tmpl = env->dtls_endpoint_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "DTLSEndpoint"));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        HandleWrap::kInternalFieldCount);

    SetProtoMethod(isolate, tmpl, "bind", DoBind);
    SetProtoMethod(isolate, tmpl, "listen", DoListen);
    SetProtoMethod(isolate, tmpl, "connect", DoConnect);
    SetProtoMethod(isolate, tmpl, "close", DoClose);
    SetProtoMethod(isolate, tmpl, "destroy", DoDestroy);
    SetProtoMethod(isolate, tmpl, "getState", GetState);
    SetProtoMethod(isolate, tmpl, "getStats", GetStats);
    SetProtoMethod(isolate, tmpl, "getAddress", GetAddress);
    SetProtoMethod(isolate, tmpl, "setMTU", SetMTU);
    SetProtoMethod(
        isolate, tmpl, "setHandshakeTimeout", SetHandshakeTimeout);
    SetProtoMethod(
        isolate, tmpl, "setSessionLimits", SetSessionLimits);
    SetProtoMethod(isolate, tmpl, "setCallbacks", DoSetCallbacks);

    env->set_dtls_endpoint_constructor_template(tmpl);
  }
  return tmpl;
}

void DTLSEndpoint::InitPerContext(Local<Object> target,
                                  Local<Context> context,
                                  Environment* env) {
  SetConstructorFunction(
      context, target, "DTLSEndpoint", GetConstructorTemplate(env));
}

void DTLSEndpoint::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(DoBind);
  registry->Register(DoListen);
  registry->Register(DoConnect);
  registry->Register(DoClose);
  registry->Register(DoDestroy);
  registry->Register(GetState);
  registry->Register(GetStats);
  registry->Register(GetAddress);
  registry->Register(SetMTU);
  registry->Register(SetHandshakeTimeout);
  registry->Register(SetSessionLimits);
  registry->Register(DoSetCallbacks);
}

void DTLSEndpoint::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args.IsConstructCall());
  new DTLSEndpoint(env, args.This());
}

int DTLSEndpoint::Bind(const SocketAddress& address) {
  if (IsHandleClosing()) return UV_EINVAL;
  if (state_->bound) return UV_EALREADY;

  unsigned int flags = 0;
  if (address.family() == AF_INET6) {
    flags |= UV_UDP_IPV6ONLY;
  }

  int err = uv_udp_bind(&handle_, address.data(), flags);
  if (err != 0) return err;

  state_->bound = 1;

  // Don't keep the event loop alive unless we're listening or have sessions.
  uv_unref(reinterpret_cast<uv_handle_t*>(&handle_));

  return 0;
}

int DTLSEndpoint::Listen(DTLSContext* context) {
  if (IsHandleClosing()) return UV_EINVAL;
  if (listening_) return UV_EALREADY;

  server_context_.reset(context);
  listening_ = true;
  state_->listening = 1;

  // Start receiving UDP datagrams.
  int err = uv_udp_recv_start(&handle_, OnAlloc, OnRecv);
  if (err != 0) {
    listening_ = false;
    state_->listening = 0;
    server_context_.reset();
    return err;
  }

  // Ref the handle while listening.
  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));

  return 0;
}

BaseObjectPtr<DTLSSession> DTLSEndpoint::Connect(DTLSContext* context,
                                                 const SocketAddress& remote,
                                                 const char* servername,
                                                 const char* verify_host,
                                                 bool verify_is_ip,
                                                 const ncrypto::Buffer<
                                                     const unsigned char>&
                                                     resume) {
  if (IsHandleClosing()) {
    THROW_ERR_INVALID_STATE(env(), "Endpoint is closing");
    return {};
  }

  // Check if we already have a session for this address.
  auto it = sessions_.find(remote);
  if (it != sessions_.end()) {
    THROW_ERR_INVALID_STATE(env(), "Session already exists for this address");
    return {};
  }

  auto session = DTLSSession::Create(env(),
                                     this,
                                     context,
                                     remote,
                                     false /* is_server */,
                                     servername,
                                     verify_host,
                                     verify_is_ip,
                                     resume);

  if (!session) return {};

  sessions_[remote] = session;
  sessions_per_host_[remote]++;
  state_->session_count = sessions_.size();
  DTLS_STAT_INCREMENT(DTLSEndpointStats, client_sessions);

  // Ref the handle while we have sessions.
  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));

  // Start receiving if not already.
  if (!listening_) {
    uv_udp_recv_start(&handle_, OnAlloc, OnRecv);
  }

  // Initiate the DTLS handshake by running Cycle.
  session->Cycle();

  return session;
}

int DTLSEndpoint::SendTo(const SocketAddress& dest,
                         const uint8_t* data,
                         size_t len) {
  if (IsHandleClosing()) return UV_EINVAL;

  // Try synchronous send first.
  uv_buf_t buf =
      uv_buf_init(const_cast<char*>(reinterpret_cast<const char*>(data)), len);
  int err = uv_udp_try_send(&handle_, &buf, 1, dest.data());

  // A datagram is sent whole or not at all, and libuv documents a
  // non-negative return as always matching the buffer size, so there is no
  // partial send to resume. Testing for == len instead let any other
  // non-negative value fall through to the queued send below and put the
  // same datagram on the wire a second time.
  if (err >= 0) {
    DTLS_STAT_INCREMENT_N(DTLSEndpointStats, bytes_sent, len);
    DTLS_STAT_INCREMENT(DTLSEndpointStats, packets_sent);
    return 0;
  }

  if (err != UV_EAGAIN) {
    return err;  // Real error.
  }

  // EAGAIN: the socket buffer is full, so queue it.

  // Async send: copy the data since it won't outlive this call.
  auto* req = new SendReq();
  req->data.assign(data, data + len);
  req->buf = uv_buf_init(reinterpret_cast<char*>(req->data.data()), len);

  err = uv_udp_send(&req->req, &handle_, &req->buf, 1, dest.data(), OnSend);
  if (err != 0) {
    delete req;
    return err;
  }

  DTLS_STAT_INCREMENT_N(DTLSEndpointStats, bytes_sent, len);
  DTLS_STAT_INCREMENT(DTLSEndpointStats, packets_sent);
  return 0;
}

void DTLSEndpoint::RemoveSession(const SocketAddress& addr) {
  if (sessions_.erase(addr) != 0) {
    // Drop the host entry entirely at zero so this map tracks live peers
    // rather than every peer ever seen.
    auto it = sessions_per_host_.find(addr);
    if (it != sessions_per_host_.end() && --it->second == 0) {
      sessions_per_host_.erase(it);
    }
  }
  state_->session_count = sessions_.size();

  // Unref if no more sessions and not listening.
  if (sessions_.empty() && !listening_ && !IsHandleClosing()) {
    uv_unref(reinterpret_cast<uv_handle_t*>(&handle_));
  }
}

void DTLSEndpoint::CloseGracefully() {
  if (IsHandleClosing()) return;

  state_->closing = 1;

  // Close all sessions gracefully (this may send close_notify).
  auto sessions_copy = sessions_;
  sessions_.clear();
  state_->session_count = 0;
  for (auto& [addr, session] : sessions_copy) {
    session->Close();
  }

  // Stop listening.
  if (listening_) {
    uv_udp_recv_stop(&handle_);
    listening_ = false;
    state_->listening = 0;
  }

  server_context_.reset();

  // Keep ourselves alive until OnClose() runs, so a garbage collection while
  // uv_close() is in flight cannot collect the wrapper before the close is
  // reported. Released in OnClose().
  self_ref_ = BaseObjectPtr<DTLSEndpoint>(this);

  // HandleWrap::Close() calls uv_close and manages the lifecycle.
  HandleWrap::Close();
}

void DTLSEndpoint::Destroy() {
  if (IsHandleClosing()) return;

  state_->destroyed = 1;

  // Copy session list to avoid iterator invalidation.
  auto sessions_copy = sessions_;
  sessions_.clear();
  state_->session_count = 0;
  for (auto& [addr, session] : sessions_copy) {
    session->Destroy();
  }

  server_context_.reset();

  if (listening_) {
    uv_udp_recv_stop(&handle_);
    listening_ = false;
    state_->listening = 0;
  }

  // Keep ourselves alive until OnClose() (see CloseGracefully()).
  self_ref_ = BaseObjectPtr<DTLSEndpoint>(this);

  HandleWrap::Close();
}

Local<Function> DTLSEndpoint::GetCallback(int index) const {
  if (index < 0 || index >= DTLS_CB_COUNT) return Local<Function>();
  Local<Function> cb = callbacks_[index].Get(env()->isolate());
  return cb;
}

void DTLSEndpoint::SetCallbacks(Local<Object> callbacks) {
  Isolate* isolate = env()->isolate();
  Local<Context> context = env()->context();

  const char* names[] = {
      "onEndpointClose",
      "onEndpointError",
      "onSessionNew",
      "onSessionClose",
      "onSessionError",
      "onSessionHandshake",
      "onSessionMessage",
      "onSessionKeylog",
      "onSessionTicket",
  };

  for (int i = 0; i < DTLS_CB_COUNT; i++) {
    Local<String> name;
    if (!String::NewFromUtf8(isolate, names[i]).ToLocal(&name)) {
      THROW_ERR_OPERATION_FAILED(isolate,
                                 "Failed to create callback name string");
      return;
    }
    Local<Value> val;
    if (!callbacks->Get(context, name).ToLocal(&val) || !val->IsFunction()) {
      THROW_ERR_MISSING_ARGS(
          isolate, ("Missing DTLS callback: " + std::string(names[i])).c_str());
      return;
    }
    callbacks_[i].Reset(isolate, val.As<Function>());
  }
}

// --- libuv callbacks ---

void DTLSEndpoint::OnAlloc(uv_handle_t* handle,
                           size_t suggested_size,
                           uv_buf_t* buf) {
  DTLSEndpoint* endpoint = static_cast<DTLSEndpoint*>(handle->data);
  // Reuse a single receive buffer. libuv delivers datagrams one at a time on
  // this thread, and OnRecv fully consumes each datagram (copying it into the
  // session's BIO) before the next OnAlloc, so a per-endpoint buffer suffices
  // and avoids a heap allocation on every packet.
  //
  // One datagram at a time holds only because recvmmsg is off: libuv sets
  // UV_HANDLE_UDP_RECVMMSG solely when uv_udp_init_ex() is passed
  // UV_UDP_RECVMMSG, and DTLSEndpoint uses plain uv_udp_init(). Enabling it
  // would have libuv pack several datagrams into this one buffer and deliver
  // them as UV_UDP_MMSG_CHUNK slices, which this design does not handle.
  //
  // The size matters too: OnRecv drops anything flagged UV_UDP_PARTIAL, so a
  // buffer below the 65507-byte maximum UDP payload would start discarding
  // large datagrams rather than truncating them.
  if (endpoint->recv_buf_.empty()) {
    endpoint->recv_buf_.resize(65536);
  }
  buf->base = endpoint->recv_buf_.data();
  buf->len = endpoint->recv_buf_.size();
}

void DTLSEndpoint::OnRecv(uv_udp_t* handle,
                          ssize_t nread,
                          const uv_buf_t* buf,
                          const struct sockaddr* addr,
                          unsigned int flags) {
  DTLSEndpoint* endpoint = static_cast<DTLSEndpoint*>(handle->data);

  // buf->base is the endpoint's reusable recv_buf_; it is not freed here.
  if (nread == 0 && addr == nullptr) {
    return;
  }

  if (nread < 0) {
    HandleScope handle_scope(endpoint->env()->isolate());
    Context::Scope context_scope(endpoint->env()->context());
    Local<String> message;
    if (!String::NewFromUtf8(endpoint->env()->isolate(), uv_strerror(nread))
             .ToLocal(&message)) {
      return;
    }
    Local<Value> argv[] = {message};
    Local<Function> cb = endpoint->GetCallback(DTLS_CB_ENDPOINT_ERROR);
    if (!cb.IsEmpty()) {
      endpoint->MakeCallback(cb, 1, argv);
    }
    return;
  }

  if (addr == nullptr) {
    return;
  }

  // A truncated datagram is not a short DTLS record, it is a corrupt one, so
  // drop it rather than hand a fragment to OpenSSL.
  //
  // This cannot currently fire. libuv raises UV_UDP_PARTIAL from MSG_TRUNC,
  // which the kernel only sets when the datagram did not fit the buffer, and
  // OnAlloc always supplies 65536 -- above the largest possible UDP payload,
  // 65507 over IPv4 and 65527 over IPv6. The check is here so that shrinking
  // that buffer degrades to dropped packets instead of corrupt records.
  if ((flags & UV_UDP_PARTIAL) != 0) {
    return;
  }

  IncrementStat<DTLSEndpointStats, &DTLSEndpointStats::bytes_received>(
      endpoint->stats_.Data(), nread);
  IncrementStat<DTLSEndpointStats, &DTLSEndpointStats::packets_received>(
      endpoint->stats_.Data());

  SocketAddress remote(addr);
  endpoint->ProcessDatagram(
      reinterpret_cast<const uint8_t*>(buf->base), nread, remote);
}

void DTLSEndpoint::OnSend(uv_udp_send_t* req, int status) {
  SendReq* send_req = reinterpret_cast<SendReq*>(req);
  delete send_req;
}

void DTLSEndpoint::OnClose() {
  state_->closing = 0;
  state_->destroyed = 1;
  DTLS_STAT_RECORD_TIMESTAMP(DTLSEndpointStats, destroyed_at);

  // Release the strong self-reference taken when the close was initiated.
  // HandleWrap::OnClose still holds its own reference for the duration of this
  // call, so this does not free us here.
  self_ref_.reset();

  // A close initiated outside CloseGracefully()/Destroy() (e.g. an endpoint
  // abandoned mid-construction and closed at environment teardown) takes no
  // self-reference, so its wrapper may already be collected. There is no JS
  // side to notify in that case; skip it rather than touch a freed wrapper.
  if (persistent().IsEmpty()) return;

  Local<Function> cb = GetCallback(DTLS_CB_ENDPOINT_CLOSE);
  if (!cb.IsEmpty()) {
    Local<Value> argv[] = {};
    MakeCallback(cb, 0, argv);
  }
}

void DTLSEndpoint::ProcessDatagram(const uint8_t* data,
                                   size_t len,
                                   const SocketAddress& remote) {
  if (IsHandleClosing()) return;

  // An empty datagram is legal UDP but can never carry a DTLS record, and
  // anyone can send one. Dropping it here keeps it out of the accept path,
  // where it would otherwise cost a full SSL_new()/DTLSv1_listen()/SSL_free()
  // cycle, and out of the session BIOs, where a zero length write to a
  // datagram BIO queues an empty datagram whose read reports EOF rather than
  // "try again".
  if (len == 0) return;

  // Look up existing session by remote address.
  auto it = sessions_.find(remote);
  if (it != sessions_.end()) {
    it->second->Receive(data, len);
    return;
  }

  // No existing session. If we're in server mode, try to accept.
  if (listening_ && server_context_) {
    AcceptConnection(data, len, remote);
  }
}

bool DTLSEndpoint::HasCapacityFor(const SocketAddress& remote) const {
  if (max_sessions_ != 0 && sessions_.size() >= max_sessions_) return false;

  if (max_sessions_per_host_ != 0) {
    auto it = sessions_per_host_.find(remote);
    if (it != sessions_per_host_.end() && it->second >= max_sessions_per_host_)
      return false;
  }

  return true;
}

// Cheap structural screen for a datagram that could plausibly begin a
// handshake, applied before anything is allocated for it.
//
// Everything reaching the accept path is unauthenticated and from a source
// address that has not been validated yet, and DTLSv1_listen() only ever
// proceeds on a ClientHello. Recognising the obvious rejects here avoids
// paying SSL_new() + two BIO_new()s + SSL_free() to reach the same verdict.
// This deliberately does not attempt to parse the ClientHello itself; that is
// OpenSSL's job, and getting it wrong would mean rejecting real clients.
bool DTLSEndpoint::CouldBeClientHello(const uint8_t* data, size_t len) {
  // DTLS record header is 13 bytes, then the handshake msg_type.
  if (len < 14) return false;

  // ContentType must be handshake(22).
  if (data[0] != 22) return false;

  // ProtocolVersion is DTLS 1.0 (0xfeff) or 1.2 (0xfefd); both have major
  // 0xfe. DTLS 1.3 keeps the record version at 0xfefd for compatibility.
  if (data[1] != 0xfe) return false;

  // The record must not claim more payload than the datagram actually holds.
  const size_t record_len = (static_cast<size_t>(data[11]) << 8) | data[12];
  if (record_len > len - 13) return false;

  // HandshakeType must be client_hello(1).
  if (data[13] != 1) return false;

  return true;
}

void DTLSEndpoint::AcceptConnection(const uint8_t* data,
                                    size_t len,
                                    const SocketAddress& remote) {
  if (state_->busy) {
    DTLS_STAT_INCREMENT(DTLSEndpointStats, server_busy_count);
    return;
  }

  if (!CouldBeClientHello(data, len)) {
    DTLS_STAT_INCREMENT(DTLSEndpointStats, server_rejected_count);
    return;
  }

  // Refuse before allocating anything. Staying silent rather than sending an
  // alert is deliberate: the peer has not completed the cookie exchange yet,
  // so replying would make this an amplification vector. A real client simply
  // retransmits and gets in once there is room.
  if (!HasCapacityFor(remote)) {
    DTLS_STAT_INCREMENT(DTLSEndpointStats, server_refused_count);
    return;
  }

  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  // Anything reaching this point is unauthenticated and may well be garbage,
  // so DTLSv1_listen() failing is routine rather than exceptional. Discard
  // whatever it queues instead of letting it accumulate across packets and
  // resurface as a bogus error somewhere else on this thread.
  ncrypto::MarkPopErrorOnReturn mark_pop_error_on_return;

  // Stateless cookie exchange via DTLSv1_listen() for DoS protection.
  //
  // The standard OpenSSL DTLS server flow (see s_server.c) is:
  //   1. Create SSL with BIO_s_datagram() wrapping the UDP socket
  //   2. DTLSv1_listen(ssl, &peer) -- stateless cookie exchange
  //   3. Connect the socket to the verified peer
  //   4. SSL_accept(ssl) -- continue the handshake on the SAME SSL
  //
  // We diverge in one key way: we use memory BIOs instead of datagram
  // BIOs because Node.js manages UDP I/O through libuv (uv_udp_t),
  // not through raw socket FDs. This means DTLSv1_listen()'s internal
  // BIO_dgram_get_peer()/set_peer() calls are no-ops -- we provide the
  // peer address to the cookie callbacks via DTLSContext::current_cookie_peer_
  // instead. After DTLSv1_listen() returns 1, we hand the SSL (with its
  // memory BIOs) to a DTLSSession via CreateFromSSL(). The SSL's internal
  // state machine has been prepared by DTLSv1_listen() to continue the
  // handshake from TLS_ST_SR_CLNT_HELLO, so Cycle() -> SSL_do_handshake()
  // immediately produces the ServerHello flight.
  ncrypto::SSLPointer ssl(SSL_new(server_context_->ssl_ctx()));
  if (!ssl) return;

  // These become the session's enc_in_/enc_out_, so both have to preserve
  // datagram boundaries -- see DTLSSession::Create().
  auto in = ncrypto::BIOPointer::New(BIO_s_dgram_mem());
  auto out = ncrypto::BIOPointer::New(BIO_s_dgram_mem());
  if (!in || !out) return;

  // SSL_set_bio takes ownership of both BIOs.
  BIO* in_raw = in.release();
  BIO* out_raw = out.release();
  SSL_set_bio(ssl.get(), in_raw, out_raw);
  SSL_set_accept_state(ssl.get());
  // SSL_OP_COOKIE_EXCHANGE is deliberately not set here: DTLSv1_listen() sets
  // it on the SSL it is given (d1_lib.c:804) before doing anything else, so
  // setting it again just invites the question of why the context does not.
  SSL_set_options(ssl.get(), SSL_OP_NO_QUERY_MTU);
  SSL_set_mtu(ssl.get(), mtu_);

  // Set peer address on context for the cookie callbacks.
  server_context_->set_cookie_peer(remote);

  BIO_write(in_raw, data, len);

  DeleteFnPtr<BIO_ADDR, BIO_ADDR_free> peer(BIO_ADDR_new());
  int ret = DTLSv1_listen(ssl.get(), peer.get());

  if (ret == 0) {
    // Send HelloVerifyRequest. `out_raw` is a datagram BIO, so BIO_pending()
    // gives the exact size of the next datagram; a HelloVerifyRequest is well
    // under a hundred bytes, so this never leaves the stack in practice.
    MaybeStackBuffer<uint8_t, 256> resp_buf;
    for (;;) {
      size_t pending = BIO_pending(out_raw);
      if (pending == 0) break;
      resp_buf.AllocateSufficientStorage(pending);
      int resp_len = BIO_read(out_raw, resp_buf.out(), pending);
      if (resp_len <= 0) break;
      SendTo(remote, resp_buf.out(), resp_len);
    }
    return;
  }

  if (ret < 0) {
    return;  // Error — drop packet.
  }

  // Cookie verified. Hand the SSL (which has already completed cookie
  // exchange and consumed the ClientHello) to a DTLSSession. Calling
  // Cycle() will drive SSL_do_handshake to produce the ServerHello.
  auto session = DTLSSession::CreateFromSSL(
      env(), this, server_context_.get(), std::move(ssl), in_raw, out_raw,
      remote);

  if (!session) return;

  sessions_[remote] = session;
  sessions_per_host_[remote]++;
  state_->session_count = sessions_.size();
  DTLS_STAT_INCREMENT(DTLSEndpointStats, server_sessions);

  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));

  // Emit the new session to JS before driving the handshake, so a listener
  // is in place for anything the handshake reports. Cycle() runs second.
  Local<Value> argv[] = {session->object()};
  Local<Function> cb = GetCallback(DTLS_CB_SESSION_NEW);
  if (!cb.IsEmpty()) {
    MakeCallback(cb, 1, argv);
  }

  // Drive the handshake forward — produces ServerHello etc.
  session->Cycle();
}

// --- JS binding methods ---

void DTLSEndpoint::DoBind(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  Environment* env = endpoint->env();

  CHECK(args[0]->IsString());  // host
  CHECK(args[1]->IsInt32());   // port

  Utf8Value host(env->isolate(), args[0]);
  int port = args[1].As<Int32>()->Value();

  SocketAddress addr;
  if (!SocketAddress::New(*host, port, &addr)) {
    return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid address");
  }

  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kNet, addr.ToString());

  int err = endpoint->Bind(addr);
  if (err != 0) {
    return THROW_ERR_INVALID_STATE(env, uv_strerror(err));
  }
}

void DTLSEndpoint::DoListen(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  Environment* env = endpoint->env();

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kNet, "");

  CHECK(DTLSContext::HasInstance(env, args[0]));
  DTLSContext* context;
  ASSIGN_OR_RETURN_UNWRAP(&context, args[0].As<Object>());

  int err = endpoint->Listen(context);
  if (err != 0) {
    return THROW_ERR_INVALID_STATE(env, uv_strerror(err));
  }
}

void DTLSEndpoint::DoConnect(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  Environment* env = endpoint->env();

  CHECK(DTLSContext::HasInstance(env, args[0]));
  DTLSContext* context;
  ASSIGN_OR_RETURN_UNWRAP(&context, args[0].As<Object>());

  CHECK(args[1]->IsString());  // host
  CHECK(args[2]->IsInt32());   // port

  Utf8Value host(env->isolate(), args[1]);
  int port = args[2].As<Int32>()->Value();

  SocketAddress remote;
  if (!SocketAddress::New(*host, port, &remote)) {
    return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid remote address");
  }

  THROW_IF_INSUFFICIENT_PERMISSIONS(
      env, permission::PermissionScope::kNet, remote.ToString());

  // Optional: servername (SNI), verifyHost (expected peer identity), and
  // whether verifyHost is an IP literal. These are resolved in JS and applied
  // to the client SSL before the handshake starts.
  Utf8Value servername(env->isolate(), args[3]);
  Utf8Value verify_host(env->isolate(), args[4]);
  const char* servername_ptr = args[3]->IsString() ? *servername : nullptr;
  const char* verify_host_ptr = args[4]->IsString() ? *verify_host : nullptr;
  bool verify_is_ip = args[5]->IsTrue();

  // Optional DER-encoded session to resume. The identity it was authenticated
  // for is checked in JS before it reaches here.
  ncrypto::Buffer<const unsigned char> resume{};
  ArrayBufferViewContents<unsigned char> resume_buf;
  if (args[6]->IsArrayBufferView()) {
    resume_buf.Read(args[6].As<ArrayBufferView>());
    resume = {resume_buf.data(), resume_buf.length()};
  }

  auto session = endpoint->Connect(
      context, remote, servername_ptr, verify_host_ptr, verify_is_ip, resume);
  if (session) {
    args.GetReturnValue().Set(session->object());
  }
}

void DTLSEndpoint::DoClose(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  endpoint->CloseGracefully();
}

void DTLSEndpoint::DoDestroy(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  endpoint->Destroy();
}

void DTLSEndpoint::GetState(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  args.GetReturnValue().Set(endpoint->state_.GetArrayBuffer());
}

void DTLSEndpoint::GetStats(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  args.GetReturnValue().Set(endpoint->stats_.GetArrayBuffer());
}

void DTLSEndpoint::GetAddress(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  if (endpoint->IsHandleClosing()) return;

  SocketAddress addr = SocketAddress::FromSockName(endpoint->handle_);
  Local<Object> obj;
  if (addr.ToJS(endpoint->env()).ToLocal(&obj)) {
    args.GetReturnValue().Set(obj);
  }
}

void DTLSEndpoint::SetSessionLimits(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  CHECK(args[0]->IsUint32());
  CHECK(args[1]->IsUint32());

  endpoint->max_sessions_ = args[0].As<Uint32>()->Value();
  endpoint->max_sessions_per_host_ = args[1].As<Uint32>()->Value();
}

void DTLSEndpoint::SetMTU(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  CHECK(args[0]->IsInt32());
  int mtu = args[0].As<Int32>()->Value();
  if (mtu < 256 || mtu > 65535) {
    return THROW_ERR_OUT_OF_RANGE(endpoint->env(),
                                  "MTU must be between 256 and 65535");
  }
  // Only affects sessions created after this point: DTLSSession reads the
  // value once, when it builds its SSL. Not currently reachable after
  // construction -- JS calls this from the DTLSEndpoint constructor only.
  endpoint->mtu_ = mtu;
}

void DTLSEndpoint::SetHandshakeTimeout(
    const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  CHECK(args[0]->IsNumber());
  double timeout = args[0].As<Number>()->Value();
  // Read once per session, when it is created, like the MTU above.
  endpoint->handshake_timeout_ = static_cast<uint64_t>(timeout);
}

void DTLSEndpoint::DoSetCallbacks(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());
  CHECK(args[0]->IsObject());
  endpoint->SetCallbacks(args[0].As<Object>());
}

void DTLSEndpoint::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("sessions", sessions_.size());
  tracker->TrackFieldWithSize("recv_buf", recv_buf_.size());
}

}  // namespace dtls
}  // namespace node

#endif  // HAVE_OPENSSL && HAVE_DTLS
