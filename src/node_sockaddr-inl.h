#ifndef SRC_NODE_SOCKADDR_INL_H_
#define SRC_NODE_SOCKADDR_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_internals.h"
#include "node_sockaddr.h"
#include "util-inl.h"

#include <string>

namespace node {

static constexpr uint32_t kLabelMask = 0xFFFFF;

inline void hash_combine(size_t* seed) { }

template <typename T, typename... Args>
inline void hash_combine(size_t* seed, const T& value, Args... rest) {
    *seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (*seed << 6) + (*seed >> 2);
    hash_combine(seed, rest...);
}

bool SocketAddress::is_numeric_host(const char* hostname) {
  return is_numeric_host(hostname, AF_INET) ||
         is_numeric_host(hostname, AF_INET6);
}

bool SocketAddress::is_numeric_host(const char* hostname, int family) {
  in6_addr dst;
  return inet_pton(family, hostname, &dst) == 1;
}

int SocketAddress::GetPort(const sockaddr* addr) {
  CHECK(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);
  return ntohs(addr->sa_family == AF_INET ?
      reinterpret_cast<const sockaddr_in*>(addr)->sin_port :
      reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);
}

int SocketAddress::GetPort(const sockaddr_storage* addr) {
  return GetPort(reinterpret_cast<const sockaddr*>(addr));
}

std::string SocketAddress::GetAddress(const sockaddr* addr) {
  CHECK(addr->sa_family == AF_INET || addr->sa_family == AF_INET6);
  char host[INET6_ADDRSTRLEN];
  const void* src = addr->sa_family == AF_INET ?
      static_cast<const void*>(
          &(reinterpret_cast<const sockaddr_in*>(addr)->sin_addr)) :
      static_cast<const void*>(
          &(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_addr));
  uv_inet_ntop(addr->sa_family, src, host, INET6_ADDRSTRLEN);
  return std::string(host);
}

std::string SocketAddress::GetAddress(const sockaddr_storage* addr) {
  return GetAddress(reinterpret_cast<const sockaddr*>(addr));
}

size_t SocketAddress::GetLength(const sockaddr* addr) {
  return addr->sa_family == AF_INET ?
      sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

size_t SocketAddress::GetLength(const sockaddr_storage* addr) {
  return GetLength(reinterpret_cast<const sockaddr*>(addr));
}

SocketAddress::SocketAddress(const sockaddr* addr) {
  memcpy(&address_, addr, GetLength(addr));
}

SocketAddress::SocketAddress(const SocketAddress& addr) {
  memcpy(&address_, &addr.address_, addr.length());
}

SocketAddress& SocketAddress::operator=(const sockaddr* addr) {
  memcpy(&address_, addr, GetLength(addr));
  return *this;
}

SocketAddress& SocketAddress::operator=(const SocketAddress& addr) {
  memcpy(&address_, &addr.address_, addr.length());
  return *this;
}

const sockaddr& SocketAddress::operator*() const {
  return *this->data();
}

const sockaddr* SocketAddress::operator->() const {
  return this->data();
}

size_t SocketAddress::length() const {
  return GetLength(&address_);
}

const sockaddr* SocketAddress::data() const {
  return reinterpret_cast<const sockaddr*>(&address_);
}

const uint8_t* SocketAddress::raw() const {
  return reinterpret_cast<const uint8_t*>(&address_);
}

sockaddr* SocketAddress::storage() {
  return reinterpret_cast<sockaddr*>(&address_);
}

int SocketAddress::family() const {
  return address_.ss_family;
}

std::string SocketAddress::address() const {
  return GetAddress(&address_);
}

int SocketAddress::port() const {
  return GetPort(&address_);
}

uint32_t SocketAddress::flow_label() const {
  if (family() != AF_INET6)
    return 0;
  const sockaddr_in6* in = reinterpret_cast<const sockaddr_in6*>(data());
  return in->sin6_flowinfo;
}

void SocketAddress::set_flow_label(uint32_t label) {
  if (family() != AF_INET6)
    return;
  CHECK_LE(label, kLabelMask);
  sockaddr_in6* in = reinterpret_cast<sockaddr_in6*>(&address_);
  in->sin6_flowinfo = label;
}

std::string SocketAddress::ToString() const {
  if (family() != AF_INET && family() != AF_INET6) return "";
  return (family() == AF_INET6 ?
              std::string("[") + address() + "]:" :
              address() + ":") +
      std::to_string(port());
}

void SocketAddress::Update(uint8_t* data, size_t len) {
  CHECK_LE(len, sizeof(address_));
  memcpy(&address_, data, len);
}

v8::Local<v8::Object> SocketAddress::ToJS(
    Environment* env,
    v8::Local<v8::Object> info) const {
  return AddressToJS(env, data(), info);
}

bool SocketAddress::operator==(const SocketAddress& other) const {
  if (family() != other.family()) return false;
  return memcmp(raw(), other.raw(), length()) == 0;
}

bool SocketAddress::operator!=(const SocketAddress& other) const {
  return !(*this == other);
}
}  // namespace node

#endif  // NODE_WANT_INTERNALS
#endif  // SRC_NODE_SOCKADDR_INL_H_
