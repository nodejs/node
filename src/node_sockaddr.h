#ifndef SRC_NODE_SOCKADDR_H_
#define SRC_NODE_SOCKADDR_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "node.h"
#include "uv.h"
#include "v8.h"

#include <string>

namespace node {

class SocketAddress {
 public:
  struct Hash {
    inline size_t operator()(const sockaddr* addr) const;
    inline size_t operator()(const SocketAddress& addr) const;
  };

  struct Compare {
    inline bool operator()(
        const sockaddr* laddr,
        const sockaddr* raddr) const;
    inline bool operator()(
        const SocketAddress& laddr,
        const SocketAddress& raddr) const;
  };

  inline static bool is_numeric_host(const char* hostname);

  inline static bool is_numeric_host(const char* hostname, int family);

  inline static sockaddr_storage* ToSockAddr(
      int32_t family,
      const char* host,
      uint32_t port,
      sockaddr_storage* addr);

  inline static int GetPort(const sockaddr* addr);

  inline static int GetPort(const sockaddr_storage* addr);

  inline static std::string GetAddress(const sockaddr* addr);

  inline static std::string GetAddress(const sockaddr_storage* addr);

  inline static size_t GetLength(const sockaddr* addr);

  inline static size_t GetLength(const sockaddr_storage* addr);

  SocketAddress() {}

  inline explicit SocketAddress(const sockaddr* addr);

  inline explicit SocketAddress(const SocketAddress& addr);

  inline SocketAddress& operator=(const sockaddr* other);

  inline SocketAddress& operator=(const SocketAddress& other);

  inline const sockaddr& operator*() const;
  inline const sockaddr* operator->() const;
  inline const sockaddr* data() const;

  inline size_t GetLength() const;

  inline int GetFamily() const;

  inline std::string GetAddress() const;

  inline int GetPort() const;

  inline void Update(uint8_t* data, size_t len);

  inline static SocketAddress* FromSockName(
      const uv_tcp_t* handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromSockName(
      const uv_tcp_t& handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromSockName(
      const uv_udp_t* handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromSockName(
      const uv_udp_t& handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromPeerName(
      const uv_tcp_t* handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromPeerName(
      const uv_tcp_t& handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromPeerName(
      const uv_udp_t* handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* FromPeerName(
      const uv_udp_t& handle,
      SocketAddress* addr = nullptr);

  inline static SocketAddress* New(
      const char* host,
      uint32_t port,
      int32_t family = AF_INET,
      SocketAddress* addr = nullptr);

  inline v8::Local<v8::Object> ToJS(
      Environment* env,
      v8::Local<v8::Object> obj = v8::Local<v8::Object>()) const;

 private:
  template <typename T, typename F>
  static SocketAddress* FromUVHandle(
      F fn,
      T* handle,
      SocketAddress* addr = nullptr);

  template <typename T, typename F>
  static SocketAddress* FromUVHandle(
      F fn,
      const T* handle,
      SocketAddress* addr = nullptr);

  sockaddr_storage address_;
};

}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_NODE_SOCKADDR_H_
