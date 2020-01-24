#ifndef SRC_NODE_SOCKADDR_H_
#define SRC_NODE_SOCKADDR_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "env.h"
#include "memory_tracker.h"
#include "node.h"
#include "uv.h"
#include "v8.h"

#include <string>
#include <unordered_map>

namespace node {

class SocketAddress : public MemoryRetainer {
 public:
  struct Hash {
    size_t operator()(const sockaddr* addr) const;
    size_t operator()(const SocketAddress& addr) const;
  };

  struct Compare {
    bool operator()(
        const sockaddr* laddr,
        const sockaddr* raddr) const;
    bool operator()(
        const SocketAddress& laddr,
        const SocketAddress& raddr) const;
  };

  inline static bool is_numeric_host(const char* hostname);

  inline static bool is_numeric_host(const char* hostname, int family);

  static sockaddr_storage* ToSockAddr(
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

  SocketAddress() = default;

  inline explicit SocketAddress(const sockaddr* addr);

  inline explicit SocketAddress(const SocketAddress& addr);

  inline SocketAddress& operator=(const sockaddr* other);

  inline SocketAddress& operator=(const SocketAddress& other);

  inline const sockaddr& operator*() const;
  inline const sockaddr* operator->() const;

  const sockaddr* data() const {
    return reinterpret_cast<const sockaddr*>(&address_);
  }

  sockaddr* storage() {
    return reinterpret_cast<sockaddr*>(&address_);
  }

  size_t length() const {
    return GetLength(&address_);
  }

  inline int family() const;

  inline std::string address() const;

  inline int port() const;

  inline void Update(uint8_t* data, size_t len);

  template <typename T>
  static SocketAddress* FromSockName(
      const T* handle,
      SocketAddress* addr = nullptr);

  template <typename T>
  static SocketAddress* FromSockName(
      const T& handle,
      SocketAddress* addr = nullptr);

  template <typename T>
  static SocketAddress* FromPeerName(
      const T* handle,
      SocketAddress* addr = nullptr);

  template <typename T>
  static SocketAddress* FromPeerName(
      const T& handle,
      SocketAddress* addr = nullptr);

  static SocketAddress* New(
      const char* host,
      uint32_t port,
      int32_t family = AF_INET,
      SocketAddress* addr = nullptr);

  inline v8::Local<v8::Object> ToJS(
      Environment* env,
      v8::Local<v8::Object> obj = v8::Local<v8::Object>()) const;

  inline std::string ToString() const;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SocketAddress)
  SET_SELF_SIZE(SocketAddress)

  template <typename T>
  using Map = std::unordered_map<SocketAddress, T, Hash, Compare>;

 private:
  sockaddr_storage address_;
};

}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_NODE_SOCKADDR_H_
