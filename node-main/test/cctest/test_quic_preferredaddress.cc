#if HAVE_OPENSSL
#include "quic/guard.h"
#ifndef OPENSSL_NO_QUIC
#include <env-inl.h>
#include <gtest/gtest.h>
#include <ngtcp2/ngtcp2.h>
#include <node_sockaddr-inl.h>
#include <quic/cid.h>
#include <quic/data.h>
#include <quic/preferredaddress.h>
#include <util-inl.h>
#include <string>
#include <unordered_map>

using node::SocketAddress;
using node::quic::CID;
using node::quic::Path;
using node::quic::PreferredAddress;

TEST(PreferredAddress, Basic) {
  const auto cid = CID::Factory::random().Generate();
  ngtcp2_preferred_addr paddr;
  paddr.cid = cid;

  sockaddr_storage storage;
  sockaddr_storage storage6;
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::1", 123, &storage6);

  memcpy(&paddr.ipv4, &storage, sizeof(sockaddr_in));
  paddr.ipv4.sin_family = AF_INET;
  paddr.ipv4_present = 1;
  paddr.ipv6_present = 0;

  sockaddr_storage storage1;
  sockaddr_storage storage2;
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage1);
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.124", 443, &storage2);
  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&storage1));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&storage2));
  Path path(addr1, addr2);

  PreferredAddress preferred_address(&path, &paddr);

  CHECK_EQ(preferred_address.ipv4().has_value(), true);
  CHECK_EQ(preferred_address.ipv6().has_value(), false);

  const auto ipv4 = preferred_address.ipv4().value();
  CHECK_EQ(ipv4.family, AF_INET);
  CHECK_EQ(htons(ipv4.port), 443);
  CHECK_EQ(ipv4.address, "123.123.123.123");

  memcpy(&paddr.ipv6, &storage6, sizeof(sockaddr_in6));
  paddr.ipv6_present = 1;

  CHECK_EQ(preferred_address.ipv6().has_value(), true);
  const auto ipv6 = preferred_address.ipv6().value();
  CHECK_EQ(ipv6.family, AF_INET6);
  CHECK_EQ(htons(ipv6.port), 123);
  CHECK_EQ(ipv6.address, "2001:db8::1");

  CHECK_EQ(preferred_address.cid(), cid);
}

TEST(PreferredAddress, SetTransportParams) {
  sockaddr_storage storage1;
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage1);
  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&storage1));
  Path path(addr1, addr1);
  ngtcp2_transport_params params;
  ngtcp2_transport_params_default(&params);
  PreferredAddress::Set(&params, addr1.data());

  CHECK_EQ(params.preferred_addr_present, 1);

  PreferredAddress paddr2(&path, &params.preferred_addr);
  CHECK_EQ(paddr2.ipv4().has_value(), true);
  const auto ipv4_2 = paddr2.ipv4().value();
  CHECK_EQ(ipv4_2.family, AF_INET);
  CHECK_EQ(htons(ipv4_2.port), 443);
  CHECK_EQ(ipv4_2.address, "123.123.123.123");
}
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL
