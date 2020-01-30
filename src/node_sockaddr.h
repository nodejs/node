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

  const uint8_t* raw() const {
    return reinterpret_cast<const uint8_t*>(&address_);
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

  // If the SocketAddress is an IPv6 address, returns the
  // current value of the IPv6 flow label, if set. Otherwise
  // returns 0.
  inline uint32_t flow_label() const;

  // If the SocketAddress is an IPv6 address, sets the
  // current value of the IPv6 flow label. If not an
  // IPv6 address, set_flow_label is a non-op. It
  // is important to note that the flow label,
  // while represented as an uint32_t, the flow
  // label is strictly limited to 20 bits, and
  // this will assert if any value larger than
  // 20-bits is specified.
  inline void set_flow_label(uint32_t label = 0);

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
