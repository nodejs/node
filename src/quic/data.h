#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory_tracker.h>
#include <node_internals.h>
#include <node_sockaddr.h>
#include <ngtcp2/ngtcp2.h>
#include <nghttp3/nghttp3.h>
#include <v8.h>

namespace node {
namespace quic {

struct Path final : public ngtcp2_path {
  Path(const SocketAddress& local, const SocketAddress& remote);
};

struct PathStorage final: public ngtcp2_path_storage {
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

  operator uv_buf_t() const;
  operator ngtcp2_vec() const;
  operator nghttp3_vec() const;
  operator bool() const;
  size_t length() const;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Store);
  SET_SELF_SIZE(Store);

 private:
   template <typename T, typename t> T convert() const;
   std::shared_ptr<v8::BackingStore> store_;
   size_t length_ = 0;
   size_t offset_ = 0;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
