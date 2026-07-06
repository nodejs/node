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

#include <cstring>

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

namespace dtls {

namespace {
struct SendReq {
  uv_udp_send_t req;
  uv_buf_t buf;
  std::vector<uint8_t> data;
};
}  // namespace

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
                                                 bool verify_is_ip) {
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
                                     context->ssl_ctx(),
                                     remote,
                                     false /* is_server */,
                                     servername,
                                     verify_host,
                                     verify_is_ip);

  if (!session) return {};

  sessions_[remote] = session;
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

  if (err == static_cast<int>(len)) {
    DTLS_STAT_INCREMENT_N(DTLSEndpointStats, bytes_sent, len);
    DTLS_STAT_INCREMENT(DTLSEndpointStats, packets_sent);
    return 0;  // Sent successfully.
  }

  if (err != UV_EAGAIN && err < 0) {
    return err;  // Real error.
  }

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
  sessions_.erase(addr);
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
    Local<Value> argv[] = {
        String::NewFromUtf8(endpoint->env()->isolate(), uv_strerror(nread))
            .ToLocalChecked(),
    };
    Local<Function> cb = endpoint->GetCallback(DTLS_CB_ENDPOINT_ERROR);
    if (!cb.IsEmpty()) {
      endpoint->MakeCallback(cb, 1, argv);
    }
    return;
  }

  if (addr == nullptr) {
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

void DTLSEndpoint::AcceptConnection(const uint8_t* data,
                                    size_t len,
                                    const SocketAddress& remote) {
  if (state_->busy) {
    DTLS_STAT_INCREMENT(DTLSEndpointStats, server_busy_count);
    return;
  }

  HandleScope handle_scope(env()->isolate());
  Context::Scope context_scope(env()->context());

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
  SSL* tmp_ssl = SSL_new(server_context_->ssl_ctx());
  if (tmp_ssl == nullptr) return;

  BIO* in = BIO_new(BIO_s_mem());
  BIO* out = BIO_new(BIO_s_mem());
  if (in == nullptr || out == nullptr) {
    BIO_free(in);
    BIO_free(out);
    SSL_free(tmp_ssl);
    return;
  }

  BIO_set_mem_eof_return(in, -1);
  BIO_set_mem_eof_return(out, -1);
  SSL_set_bio(tmp_ssl, in, out);
  SSL_set_accept_state(tmp_ssl);
  SSL_set_options(tmp_ssl, SSL_OP_NO_QUERY_MTU | SSL_OP_COOKIE_EXCHANGE);
  SSL_set_mtu(tmp_ssl, mtu_);

  // Set peer address on context for the cookie callbacks.
  server_context_->set_cookie_peer(remote);

  BIO_write(in, data, len);

  BIO_ADDR* peer = BIO_ADDR_new();
  int ret = DTLSv1_listen(tmp_ssl, peer);
  BIO_ADDR_free(peer);

  if (ret == 0) {
    // Send HelloVerifyRequest.
    uint8_t resp_buf[65536];
    int resp_len;
    while ((resp_len = BIO_read(out, resp_buf, sizeof(resp_buf))) > 0) {
      SendTo(remote, resp_buf, resp_len);
    }
    SSL_free(tmp_ssl);
    return;
  }

  if (ret < 0) {
    SSL_free(tmp_ssl);
    return;  // Error — drop packet.
  }

  // Cookie verified. Hand the SSL (which has already completed cookie
  // exchange and consumed the ClientHello) to a DTLSSession. Calling
  // Cycle() will drive SSL_do_handshake to produce the ServerHello.
  ncrypto::SSLPointer ssl(tmp_ssl);

  auto session =
      DTLSSession::CreateFromSSL(env(), this, std::move(ssl), in, out, remote);

  if (!session) return;

  sessions_[remote] = session;
  state_->session_count = sessions_.size();
  DTLS_STAT_INCREMENT(DTLSEndpointStats, server_sessions);

  uv_ref(reinterpret_cast<uv_handle_t*>(&handle_));

  // Drive the handshake forward — produces ServerHello etc.
  session->Cycle();

  // Emit the new session to JS.
  Local<Value> argv[] = {session->object()};
  Local<Function> cb = GetCallback(DTLS_CB_SESSION_NEW);
  if (!cb.IsEmpty()) {
    MakeCallback(cb, 1, argv);
  }
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

  auto session = endpoint->Connect(
      context, remote, servername_ptr, verify_host_ptr, verify_is_ip);
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

void DTLSEndpoint::SetMTU(const FunctionCallbackInfo<Value>& args) {
  DTLSEndpoint* endpoint;
  ASSIGN_OR_RETURN_UNWRAP(&endpoint, args.This());

  CHECK(args[0]->IsInt32());
  int mtu = args[0].As<Int32>()->Value();
  if (mtu < 256 || mtu > 65535) {
    return THROW_ERR_OUT_OF_RANGE(endpoint->env(),
                                  "MTU must be between 256 and 65535");
  }
  endpoint->mtu_ = mtu;
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
