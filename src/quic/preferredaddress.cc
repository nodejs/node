#if HAVE_OPENSSL
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <env-inl.h>
#include <ngtcp2/ngtcp2.h>
#include <node_errors.h>
#include <node_realm-inl.h>
#include <node_sockaddr-inl.h>
#include <util-inl.h>
#include <uv.h>
#include <v8.h>
#include "cid.h"
#include "preferredaddress.h"

namespace node {

using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::Value;

namespace quic {

namespace {
template <int FAMILY>
std::optional<const PreferredAddress::AddressInfo> get_address_info(
    const ngtcp2_preferred_addr& paddr) {
  PreferredAddress::AddressInfo address;
  address.family = FAMILY;
  if constexpr (FAMILY == AF_INET) {
    if (!paddr.ipv4_present) return std::nullopt;
    address.port = paddr.ipv4.sin_port;
    if (uv_inet_ntop(
            FAMILY, &paddr.ipv4.sin_addr, address.host, sizeof(address.host)) ==
        0) {
      address.address = address.host;
    }
  } else {
    if (!paddr.ipv6_present) return std::nullopt;
    address.port = paddr.ipv6.sin6_port;
    if (uv_inet_ntop(FAMILY,
                     &paddr.ipv6.sin6_addr,
                     address.host,
                     sizeof(address.host)) == 0) {
      address.address = address.host;
    }
  }
  return address;
}

template <int FAMILY>
void copy_to_transport_params(ngtcp2_transport_params* params,
                              const sockaddr* addr) {
  params->preferred_addr_present = 1;
  if constexpr (FAMILY == AF_INET) {
    params->preferred_addr.ipv4_present = 1;
    memcpy(&params->preferred_addr.ipv4, addr, sizeof(sockaddr_in));
  } else {
    DCHECK_EQ(FAMILY, AF_INET6);
    params->preferred_addr.ipv6_present = 1;
    memcpy(&params->preferred_addr.ipv6, addr, sizeof(sockaddr_in6));
  }
}

bool resolve(const PreferredAddress::AddressInfo& address,
             uv_getaddrinfo_t* req) {
  addrinfo hints{};
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
  hints.ai_family = address.family;
  hints.ai_socktype = SOCK_DGRAM;

  // ngtcp2 requires the selection of the preferred address
  // to be synchronous, which means we have to do a sync resolve
  // using uv_getaddrinfo here.
  return uv_getaddrinfo(nullptr,
                        req,
                        nullptr,
                        address.host,
                        std::to_string(address.port).data(),
                        &hints) == 0 &&
         req->addrinfo != nullptr;
}
}  // namespace

PreferredAddress::PreferredAddress(ngtcp2_path* dest,
                                   const ngtcp2_preferred_addr* paddr)
    : dest_(dest), paddr_(paddr) {
  DCHECK_NOT_NULL(paddr);
  DCHECK_NOT_NULL(dest);
}

std::optional<const PreferredAddress::AddressInfo> PreferredAddress::ipv4()
    const {
  return get_address_info<AF_INET>(*paddr_);
}

std::optional<const PreferredAddress::AddressInfo> PreferredAddress::ipv6()
    const {
  return get_address_info<AF_INET6>(*paddr_);
}

void PreferredAddress::Use(const AddressInfo& address) {
  uv_getaddrinfo_t req;
  auto on_exit = OnScopeLeave([&] {
    if (req.addrinfo != nullptr) uv_freeaddrinfo(req.addrinfo);
  });

  if (resolve(address, &req)) {
    DCHECK_NOT_NULL(req.addrinfo);
    dest_->remote.addrlen = req.addrinfo->ai_addrlen;
    memcpy(dest_->remote.addr, req.addrinfo->ai_addr, req.addrinfo->ai_addrlen);
  }
}

void PreferredAddress::Set(ngtcp2_transport_params* params,
                           const sockaddr* addr) {
  DCHECK_NOT_NULL(params);
  DCHECK_NOT_NULL(addr);
  switch (addr->sa_family) {
    case AF_INET:
      return copy_to_transport_params<AF_INET>(params, addr);
    case AF_INET6:
      return copy_to_transport_params<AF_INET6>(params, addr);
    default:
      UNREACHABLE("Unreachable");
  }
  // Any other value is just ignored.
}

Maybe<PreferredAddress::Policy> PreferredAddress::tryGetPolicy(
    Environment* env, Local<Value> value) {
  return value->IsUndefined() ? Just(Policy::USE_PREFERRED)
                              : Just(FromV8Value<Policy>(value));
}

void PreferredAddress::Initialize(Environment* env, Local<v8::Object> target) {
  // The QUIC_* constants are expected to be exported out to be used on
  // the JavaScript side of the API.
  static constexpr auto PREFERRED_ADDRESS_USE =
      static_cast<uint8_t>(Policy::USE_PREFERRED);
  static constexpr auto PREFERRED_ADDRESS_IGNORE =
      static_cast<uint8_t>(Policy::IGNORE_PREFERRED);
  static constexpr auto DEFAULT_PREFERRED_ADDRESS_POLICY =
      static_cast<uint8_t>(Policy::USE_PREFERRED);

  NODE_DEFINE_CONSTANT(target, PREFERRED_ADDRESS_IGNORE);
  NODE_DEFINE_CONSTANT(target, PREFERRED_ADDRESS_USE);
  NODE_DEFINE_CONSTANT(target, DEFAULT_PREFERRED_ADDRESS_POLICY);
}

const CID PreferredAddress::cid() const {
  return CID(&paddr_->cid);
}

}  // namespace quic
}  // namespace node

#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL
