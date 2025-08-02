#if HAVE_OPENSSL
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include "data.h"
#include <env-inl.h>
#include <memory_tracker-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_sockaddr-inl.h>
#include <string_bytes.h>
#include <v8.h>
#include "defs.h"
#include "util.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Uint8Array;
using v8::Undefined;
using v8::Value;

namespace quic {
int DebugIndentScope::indent_ = 0;

Path::Path(const SocketAddress& local, const SocketAddress& remote) {
  ngtcp2_addr_init(&this->local, local.data(), local.length());
  ngtcp2_addr_init(&this->remote, remote.data(), remote.length());
}

std::string Path::ToString() const {
  DebugIndentScope indent;
  auto prefix = indent.Prefix();

  const sockaddr* local_in = reinterpret_cast<const sockaddr*>(local.addr);
  auto local_addr = SocketAddress::GetAddress(local_in);
  auto local_port = SocketAddress::GetPort(local_in);

  const sockaddr* remote_in = reinterpret_cast<const sockaddr*>(remote.addr);
  auto remote_addr = SocketAddress::GetAddress(remote_in);
  auto remote_port = SocketAddress::GetPort(remote_in);

  std::string res("{");
  res += prefix + "local: " + local_addr + ":" + std::to_string(local_port);
  res += prefix + "remote: " + remote_addr + ":" + std::to_string(remote_port);
  res += indent.Close();
  return res;
}

PathStorage::PathStorage() {
  Reset();
}

void PathStorage::Reset() {
  ngtcp2_path_storage_zero(this);
}

void PathStorage::CopyTo(PathStorage* path) const {
  ngtcp2_path_copy(&path->path, &this->path);
}

bool PathStorage::operator==(const PathStorage& other) const {
  return ngtcp2_path_eq(&path, &other.path) != 0;
}

bool PathStorage::operator!=(const PathStorage& other) const {
  return ngtcp2_path_eq(&path, &other.path) == 0;
}

// ============================================================================

Store::Store(std::shared_ptr<BackingStore> store, size_t length, size_t offset)
    : store_(std::move(store)), length_(length), offset_(offset) {
  CHECK_LE(offset_, store_->ByteLength());
  CHECK_LE(length_, store_->ByteLength() - offset_);
}

Store::Store(std::unique_ptr<BackingStore> store, size_t length, size_t offset)
    : store_(std::move(store)), length_(length), offset_(offset) {
  CHECK_LE(offset_, store_->ByteLength());
  CHECK_LE(length_, store_->ByteLength() - offset_);
}

Maybe<Store> Store::From(
    Local<ArrayBuffer> buffer,
    Local<Value> detach_key) {
  if (!buffer->IsDetachable()) {
    return Nothing<Store>();
  }
  bool res;
  auto backing = buffer->GetBackingStore();
  auto length = buffer->ByteLength();
  if (!buffer->Detach(detach_key).To(&res) || !res) {
    return Nothing<Store>();
  }
  return Just(Store(std::move(backing), length, 0));
}

Maybe<Store> Store::From(
    Local<ArrayBufferView> view,
    Local<Value> detach_key) {
  if (!view->Buffer()->IsDetachable()) {
    return Nothing<Store>();
  }
  bool res;
  auto backing = view->Buffer()->GetBackingStore();
  auto length = view->ByteLength();
  auto offset = view->ByteOffset();
  if (!view->Buffer()->Detach(detach_key).To(&res) || !res) {
    return Nothing<Store>();
  }
  return Just(Store(std::move(backing), length, offset));
}

Local<Uint8Array> Store::ToUint8Array(Environment* env) const {
  return !store_
             ? Uint8Array::New(ArrayBuffer::New(env->isolate(), 0), 0, 0)
             : Uint8Array::New(
                   ArrayBuffer::New(env->isolate(), store_), offset_, length_);
}

Store::operator bool() const {
  return store_ != nullptr;
}
size_t Store::length() const {
  return length_;
}

size_t Store::total_length() const {
  return store_ ? store_->ByteLength() : 0;
}

template <typename T, OneByteType N>
T Store::convert() const {
  // We can only safely convert to T if we have a valid store.
  CHECK(store_);
  T buf;
  buf.base =
      store_ != nullptr ? static_cast<N*>(store_->Data()) + offset_ : nullptr;
  buf.len = length_;
  return buf;
}

Store::operator uv_buf_t() const {
  return convert<uv_buf_t, typeof(*uv_buf_t::base)>();
}

Store::operator ngtcp2_vec() const {
  return convert<ngtcp2_vec, typeof(*ngtcp2_vec::base)>();
}

Store::operator nghttp3_vec() const {
  return convert<nghttp3_vec, typeof(*ngtcp2_vec::base)>();
}

void Store::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("store", store_);
}

// ============================================================================

namespace {
constexpr std::string_view TypeName(QuicError::Type type) {
  switch (type) {
    case QuicError::Type::APPLICATION:
      return "application";
    case QuicError::Type::TRANSPORT:
      return "transport";
    case QuicError::Type::VERSION_NEGOTIATION:
      return "version_negotiation";
    case QuicError::Type::IDLE_CLOSE:
      return "idle_close";
    case QuicError::Type::DROP_CONNECTION:
      return "drop_connection";
    case QuicError::Type::RETRY:
      return "retry";
    default:
      return "<unknown>";
  }
}
}  // namespace

QuicError::QuicError(const std::string& reason)
    : reason_(reason), error_(), ptr_(&error_) {
  ngtcp2_ccerr_default(&error_);
}

// Keep in mind that reason_ in each of the constructors here will copy
// the string from the ngtcp2_ccerr input.
QuicError::QuicError(const ngtcp2_ccerr* ptr)
    : reason_(reinterpret_cast<const char*>(ptr->reason), ptr->reasonlen),
      error_(),
      ptr_(ptr) {}

QuicError::QuicError(const ngtcp2_ccerr& error)
    : reason_(reinterpret_cast<const char*>(error.reason), error.reasonlen),
      error_(error),
      ptr_(&error_) {}

const uint8_t* QuicError::reason_c_str() const {
  return reinterpret_cast<const uint8_t*>(reason_.c_str());
}

bool QuicError::operator!=(const QuicError& other) const {
  return !(*this == other);
}

bool QuicError::operator==(const QuicError& other) const {
  if (this == &other) return true;
  return type() == other.type() && code() == other.code() &&
         frame_type() == other.frame_type();
}

QuicError::Type QuicError::type() const {
  return static_cast<Type>(ptr_->type);
}

error_code QuicError::code() const {
  return ptr_->error_code;
}

uint64_t QuicError::frame_type() const {
  return ptr_->frame_type;
}

const std::string_view QuicError::reason() const {
  return reason_;
}

QuicError::operator const ngtcp2_ccerr&() const {
  return *ptr_;
}

QuicError::operator const ngtcp2_ccerr*() const {
  return ptr_;
}

std::string QuicError::reason_for_liberr(int liberr) {
  return ngtcp2_strerror(liberr);
}

std::string QuicError::reason_for_h3_liberr(int liberr) {
  return nghttp3_strerror(liberr);
}

bool QuicError::is_fatal_liberror(int liberr) {
  return ngtcp2_err_is_fatal(liberr) != 0;
}

bool QuicError::is_fatal_h3_liberror(int liberr) {
  return nghttp3_err_is_fatal(liberr) != 0;
}

error_code QuicError::liberr_to_code(int liberr) {
  return ngtcp2_err_infer_quic_transport_error_code(liberr);
}

error_code QuicError::h3_liberr_to_code(int liberr) {
  return nghttp3_err_infer_quic_app_error_code(liberr);
}

bool QuicError::is_crypto_error() const {
  return code() & NGTCP2_CRYPTO_ERROR;
}

std::optional<int> QuicError::get_crypto_error() const {
  if (!is_crypto_error()) return std::nullopt;
  return code() & ~NGTCP2_CRYPTO_ERROR;
}

MaybeLocal<Value> QuicError::ToV8Value(Environment* env) const {
  if ((type() == Type::TRANSPORT && code() == NGTCP2_NO_ERROR) ||
      (type() == Type::APPLICATION && code() == NGHTTP3_H3_NO_ERROR)) {
    // Note that we only return undefined for *known* no-error application
    // codes. It is possible that other application types use other specific
    // no-error codes, but since we don't know which application is being used,
    // we'll just return the error code value for those below.
    return Undefined(env->isolate());
  }

  Local<Value> type_str;
  if (!node::ToV8Value(env->context(), TypeName(type())).ToLocal(&type_str)) {
    return {};
  }

  Local<Value> argv[] = {
      type_str,
      BigInt::NewFromUnsigned(env->isolate(), code()),
      Undefined(env->isolate()),
  };

  // Note that per the QUIC specification, the reason, if present, is
  // expected to be UTF-8 encoded. The spec uses the term "SHOULD" here,
  // which means that is is entirely possible that some QUIC implementation
  // could choose a different encoding, in which case the conversion here
  // will produce garbage. That's ok though, we're going to use the default
  // assumption that the impl is following the guidelines.
  if (reason_.length() > 0 &&
      !node::ToV8Value(env->context(), reason()).ToLocal(&argv[2])) {
    return {};
  }

  return Array::New(env->isolate(), argv, arraysize(argv)).As<Value>();
}

std::string QuicError::ToString() const {
  std::string str = "QuicError(";
  str += TypeName(type());
  str += ") ";
  str += std::to_string(code());
  if (!reason_.empty()) str += ": " + reason_;
  return str;
}

void QuicError::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("reason", reason_.length());
}

const QuicError QuicError::ForTransport(TransportError code,
                                        std::string reason) {
  return ForTransport(static_cast<error_code>(code), std::move(reason));
}

const QuicError QuicError::ForTransport(error_code code, std::string reason) {
  QuicError error(std::move(reason));
  ngtcp2_ccerr_set_transport_error(
      &error.error_, code, error.reason_c_str(), error.reason().length());
  return error;
}

const QuicError QuicError::ForApplication(Http3Error code, std::string reason) {
  return ForApplication(static_cast<error_code>(code), std::move(reason));
}

const QuicError QuicError::ForApplication(error_code code, std::string reason) {
  QuicError error(std::move(reason));
  ngtcp2_ccerr_set_application_error(
      &error.error_, code, error.reason_c_str(), error.reason().length());
  return error;
}

const QuicError QuicError::ForVersionNegotiation(std::string reason) {
  return ForNgtcp2Error(NGTCP2_ERR_RECV_VERSION_NEGOTIATION, std::move(reason));
}

const QuicError QuicError::ForIdleClose(std::string reason) {
  return ForNgtcp2Error(NGTCP2_ERR_IDLE_CLOSE, std::move(reason));
}

const QuicError QuicError::ForDropConnection(std::string reason) {
  return ForNgtcp2Error(NGTCP2_ERR_DROP_CONN, std::move(reason));
}

const QuicError QuicError::ForRetry(std::string reason) {
  return ForNgtcp2Error(NGTCP2_ERR_RETRY, std::move(reason));
}

const QuicError QuicError::ForNgtcp2Error(int code, std::string reason) {
  QuicError error(std::move(reason));
  ngtcp2_ccerr_set_liberr(
      &error.error_, code, error.reason_c_str(), error.reason().length());
  return error;
}

const QuicError QuicError::ForTlsAlert(int code, std::string reason) {
  QuicError error(std::move(reason));
  ngtcp2_ccerr_set_tls_alert(
      &error.error_, code, error.reason_c_str(), error.reason().length());
  return error;
}

const QuicError QuicError::FromConnectionClose(ngtcp2_conn* session) {
  return QuicError(ngtcp2_conn_get_ccerr(session));
}

#define V(name)                                                                \
  const QuicError QuicError::TRANSPORT_##name =                                \
      ForTransport(TransportError::name);
QUIC_TRANSPORT_ERRORS(V)
#undef V

const QuicError QuicError::HTTP3_NO_ERROR = ForApplication(NGHTTP3_H3_NO_ERROR);
const QuicError QuicError::VERSION_NEGOTIATION = ForVersionNegotiation();
const QuicError QuicError::IDLE_CLOSE = ForIdleClose();
const QuicError QuicError::DROP_CONNECTION = ForDropConnection();
const QuicError QuicError::RETRY = ForRetry();
const QuicError QuicError::INTERNAL_ERROR = ForNgtcp2Error(NGTCP2_ERR_INTERNAL);

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL
