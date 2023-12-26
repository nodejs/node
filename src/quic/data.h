#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#if HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC

#include <env.h>
#include <memory_tracker.h>
#include <nghttp3/nghttp3.h>
#include <ngtcp2/ngtcp2.h>
#include <node_internals.h>
#include <node_sockaddr.h>
#include <v8.h>

namespace node {
namespace quic {

struct Path final : public ngtcp2_path {
  Path(const SocketAddress& local, const SocketAddress& remote);
  inline operator ngtcp2_path*() { return this; }
};

struct PathStorage final : public ngtcp2_path_storage {
  PathStorage();
  operator ngtcp2_path();
};

class Store final : public MemoryRetainer {
 public:
  Store() = default;

  Store(std::shared_ptr<v8::BackingStore> store,
        size_t length,
        size_t offset = 0);
  Store(std::unique_ptr<v8::BackingStore> store,
        size_t length,
        size_t offset = 0);

  enum class Option {
    NONE,
    DETACH,
  };

  Store(v8::Local<v8::ArrayBuffer> buffer, Option option = Option::NONE);
  Store(v8::Local<v8::ArrayBufferView> view, Option option = Option::NONE);

  v8::Local<v8::Uint8Array> ToUint8Array(Environment* env) const;

  operator uv_buf_t() const;
  operator ngtcp2_vec() const;
  operator nghttp3_vec() const;
  operator bool() const;
  size_t length() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Store)
  SET_SELF_SIZE(Store)

 private:
  template <typename T, typename t>
  T convert() const;
  std::shared_ptr<v8::BackingStore> store_;
  size_t length_ = 0;
  size_t offset_ = 0;
};

class QuicError final : public MemoryRetainer {
 public:
  using error_code = uint64_t;

  static constexpr error_code QUIC_NO_ERROR = NGTCP2_NO_ERROR;
  static constexpr error_code QUIC_APP_NO_ERROR = 65280;

  enum class Type {
    TRANSPORT = NGTCP2_CCERR_TYPE_TRANSPORT,
    APPLICATION = NGTCP2_CCERR_TYPE_APPLICATION,
    VERSION_NEGOTIATION = NGTCP2_CCERR_TYPE_VERSION_NEGOTIATION,
    IDLE_CLOSE = NGTCP2_CCERR_TYPE_IDLE_CLOSE,
  };

  static constexpr error_code QUIC_ERROR_TYPE_TRANSPORT =
      NGTCP2_CCERR_TYPE_TRANSPORT;
  static constexpr error_code QUIC_ERROR_TYPE_APPLICATION =
      NGTCP2_CCERR_TYPE_APPLICATION;

  explicit QuicError(const std::string_view reason = "");
  explicit QuicError(const ngtcp2_ccerr* ptr);
  explicit QuicError(const ngtcp2_ccerr& error);

  Type type() const;
  error_code code() const;
  const std::string_view reason() const;
  uint64_t frame_type() const;

  operator const ngtcp2_ccerr&() const;
  operator const ngtcp2_ccerr*() const;

  // Returns false if the QuicError uses a no_error code with type
  // transport or application.
  operator bool() const;

  bool operator==(const QuicError& other) const;
  bool operator!=(const QuicError& other) const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(QuicError)
  SET_SELF_SIZE(QuicError)

  std::string ToString() const;
  v8::MaybeLocal<v8::Value> ToV8Value(Environment* env) const;

  static QuicError ForTransport(error_code code,
                                const std::string_view reason = "");
  static QuicError ForApplication(error_code code,
                                  const std::string_view reason = "");
  static QuicError ForVersionNegotiation(const std::string_view reason = "");
  static QuicError ForIdleClose(const std::string_view reason = "");
  static QuicError ForNgtcp2Error(int code, const std::string_view reason = "");
  static QuicError ForTlsAlert(int code, const std::string_view reason = "");

  static QuicError FromConnectionClose(ngtcp2_conn* session);

  static QuicError TRANSPORT_NO_ERROR;
  static QuicError APPLICATION_NO_ERROR;
  static QuicError VERSION_NEGOTIATION;
  static QuicError IDLE_CLOSE;
  static QuicError INTERNAL_ERROR;

 private:
  const uint8_t* reason_c_str() const;

  std::string reason_;
  ngtcp2_ccerr error_;
  const ngtcp2_ccerr* ptr_ = nullptr;
};

}  // namespace quic
}  // namespace node

#endif  // HAVE_OPENSSL && NODE_OPENSSL_HAS_QUIC
#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
