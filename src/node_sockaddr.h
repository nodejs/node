#ifndef SRC_NODE_SOCKADDR_H_
#define SRC_NODE_SOCKADDR_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "memory_tracker.h"
#include "node.h"
#include "uv.h"
#include "v8.h"

#include <string>
#include <unordered_map>

namespace node {

class Environment;

class SocketAddress : public MemoryRetainer {
 public:
  struct Hash {
    size_t operator()(const SocketAddress& addr) const;
  };

  inline bool operator==(const SocketAddress& other) const;
  inline bool operator!=(const SocketAddress& other) const;

  inline static bool is_numeric_host(const char* hostname);
  inline static bool is_numeric_host(const char* hostname, int family);

  // Returns true if converting {family, host, port} to *addr succeeded.
  static bool ToSockAddr(
      int32_t family,
      const char* host,
      uint32_t port,
      sockaddr_storage* addr);

  // Returns true if converting {family, host, port} to *addr succeeded.
  static bool New(
      int32_t family,
      const char* host,
      uint32_t port,
      SocketAddress* addr);

  static bool New(
      const char* host,
      uint32_t port,
      SocketAddress* addr);

  // Returns the port for an IPv4 or IPv6 address.
  inline static int GetPort(const sockaddr* addr);
  inline static int GetPort(const sockaddr_storage* addr);

  // Returns the numeric host as a string for an IPv4 or IPv6 address.
  inline static std::string GetAddress(const sockaddr* addr);
  inline static std::string GetAddress(const sockaddr_storage* addr);

  // Returns the struct length for an IPv4, IPv6 or UNIX domain.
  inline static size_t GetLength(const sockaddr* addr);
  inline static size_t GetLength(const sockaddr_storage* addr);

  SocketAddress() = default;

  inline explicit SocketAddress(const sockaddr* addr);
  inline SocketAddress(const SocketAddress& addr);
  inline SocketAddress& operator=(const sockaddr* other);
  inline SocketAddress& operator=(const SocketAddress& other);

  inline const sockaddr& operator*() const;
  inline const sockaddr* operator->() const;

  inline const sockaddr* data() const;
  inline const uint8_t* raw() const;
  inline sockaddr* storage();
  inline size_t length() const;

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

  static SocketAddress FromSockName(const uv_udp_t& handle);
  static SocketAddress FromSockName(const uv_tcp_t& handle);
  static SocketAddress FromPeerName(const uv_udp_t& handle);
  static SocketAddress FromPeerName(const uv_tcp_t& handle);

  inline v8::Local<v8::Object> ToJS(
      Environment* env,
      v8::Local<v8::Object> obj = v8::Local<v8::Object>()) const;

  inline std::string ToString() const;

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SocketAddress)
  SET_SELF_SIZE(SocketAddress)

  template <typename T>
  using Map = std::unordered_map<SocketAddress, T, Hash>;

 private:
  sockaddr_storage address_;
};

}  // namespace node

#endif  // NOE_WANT_INTERNALS

#endif  // SRC_NODE_SOCKADDR_H_
