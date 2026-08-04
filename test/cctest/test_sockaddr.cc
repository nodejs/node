#include "gtest/gtest.h"
#include "node_sockaddr-inl.h"

using node::SocketAddress;
using node::SocketAddressBlockList;
using node::SocketAddressLRU;

TEST(SocketAddress, SocketAddress) {
  CHECK(SocketAddress::is_numeric_host("123.123.123.123"));
  CHECK(!SocketAddress::is_numeric_host("localhost"));

  sockaddr_storage storage;
  sockaddr_storage storage2;
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage);
  SocketAddress::ToSockAddr(AF_INET, "1.1.1.1", 80, &storage2);

  SocketAddress addr(reinterpret_cast<const sockaddr*>(&storage));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&storage2));

  CHECK_EQ(addr.length(), sizeof(sockaddr_in));
  CHECK_EQ(addr.family(), AF_INET);
  CHECK_EQ(addr.address(), "123.123.123.123");
  CHECK_EQ(addr.port(), 443);

  addr.set_flow_label(12345);
  CHECK_EQ(addr.flow_label(), 0);

  CHECK_NE(addr, addr2);
  CHECK_EQ(addr, addr);

  CHECK_EQ(SocketAddress::Hash()(addr), SocketAddress::Hash()(addr));
  CHECK_NE(SocketAddress::Hash()(addr), SocketAddress::Hash()(addr2));

  addr.Update(reinterpret_cast<uint8_t*>(&storage2), sizeof(sockaddr_in));
  CHECK_EQ(addr.length(), sizeof(sockaddr_in));
  CHECK_EQ(addr.family(), AF_INET);
  CHECK_EQ(addr.address(), "1.1.1.1");
  CHECK_EQ(addr.port(), 80);

  SocketAddress::Map<size_t> map;
  map[addr]++;
  map[addr]++;
  CHECK_EQ(map[addr], 2);
}

TEST(SocketAddress, IpHashAndIpEqual) {
  sockaddr_storage s1, s2, s3;
  // Same IP, different ports.
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 443, &s1);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 8080, &s2);
  // Different IP.
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.2", 443, &s3);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress addr3(reinterpret_cast<const sockaddr*>(&s3));

  SocketAddress::IpHash ip_hash;
  SocketAddress::IpEqual ip_equal;

  // Same IP, different port: should hash equal and compare equal.
  CHECK_EQ(ip_hash(addr1), ip_hash(addr2));
  CHECK(ip_equal(addr1, addr2));

  // Different IP: should not compare equal.
  CHECK(!ip_equal(addr1, addr3));

  // Full Hash (includes port) should differ for same IP, different port.
  CHECK_NE(SocketAddress::Hash()(addr1), SocketAddress::Hash()(addr2));

  // IpMap should treat same-IP-different-port as the same key.
  SocketAddress::IpMap<uint16_t> map;
  map[addr1] = 1;
  map[addr2]++;  // Same IP as addr1, should increment the same entry.
  CHECK_EQ(map[addr1], 2);
  CHECK_EQ(map.size(), 1);

  map[addr3] = 10;
  CHECK_EQ(map.size(), 2);
  CHECK_EQ(map[addr3], 10);
}

TEST(SocketAddress, IpHashIPv6) {
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET6, "::1", 443, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 8080, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::2", 443, &s3);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress addr3(reinterpret_cast<const sockaddr*>(&s3));

  SocketAddress::IpHash ip_hash;
  SocketAddress::IpEqual ip_equal;

  // Same IPv6, different port: equal.
  CHECK_EQ(ip_hash(addr1), ip_hash(addr2));
  CHECK(ip_equal(addr1, addr2));

  // Different IPv6: not equal.
  CHECK(!ip_equal(addr1, addr3));

  // IpMap with IPv6 keys.
  SocketAddress::IpMap<uint16_t> map;
  map[addr1] = 5;
  map[addr2]++;
  CHECK_EQ(map[addr1], 6);
  CHECK_EQ(map.size(), 1);
}

TEST(SocketAddress, IpEqualCrossFamily) {
  sockaddr_storage s1, s2;
  SocketAddress::ToSockAddr(AF_INET, "127.0.0.1", 443, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 443, &s2);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&s2));

  SocketAddress::IpEqual ip_equal;

  // Different address families should never be equal.
  CHECK(!ip_equal(addr1, addr2));
}

TEST(SocketAddress, SocketAddressIPv6) {
  sockaddr_storage storage;
  SocketAddress::ToSockAddr(AF_INET6, "::1", 443, &storage);

  SocketAddress addr(reinterpret_cast<const sockaddr*>(&storage));

  CHECK_EQ(addr.length(), sizeof(sockaddr_in6));
  CHECK_EQ(addr.family(), AF_INET6);
  CHECK_EQ(addr.address(), "::1");
  CHECK_EQ(addr.port(), 443);

  addr.set_flow_label(12345);
  CHECK_EQ(addr.flow_label(), 12345);
}

TEST(SocketAddressLRU, SocketAddressLRU) {
  struct Foo {
    int c;
    bool expired;
  };

  struct FooLRUTraits {
    using Type = Foo;

    static bool CheckExpired(const SocketAddress& address,
                             const Type& type,
                             uint64_t now) {
      return type.expired;
    }

    static void Touch(const SocketAddress& address, Type* type, uint64_t now) {
      type->expired = false;
    }
  };

  SocketAddressLRU<FooLRUTraits> lru(2);

  sockaddr_storage storage[4];

  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage[0]);
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.124", 443, &storage[1]);
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.125", 443, &storage[2]);
  SocketAddress::ToSockAddr(AF_INET, "123.123.123.123", 443, &storage[3]);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&storage[0]));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&storage[1]));
  SocketAddress addr3(reinterpret_cast<const sockaddr*>(&storage[2]));
  SocketAddress addr4(reinterpret_cast<const sockaddr*>(&storage[3]));

  uint64_t now = uv_hrtime();
  Foo* foo = lru.Upsert(addr1, now);
  CHECK_NOT_NULL(foo);
  CHECK_EQ(foo->c, 0);
  CHECK_EQ(foo->expired, false);

  foo->c = 1;
  foo->expired = true;

  foo = lru.Upsert(addr1, now);
  CHECK_NOT_NULL(lru.Peek(addr1));
  CHECK_EQ(lru.Peek(addr1), lru.Peek(addr4));
  CHECK_EQ(lru.Peek(addr1)->c, 1);
  CHECK_EQ(lru.Peek(addr1)->expired, false);
  CHECK_EQ(lru.size(), 1);

  foo = lru.Upsert(addr2, now);
  foo->c = 2;
  foo->expired = true;
  CHECK_NOT_NULL(lru.Peek(addr2));
  CHECK_EQ(lru.Peek(addr2)->c, 2);
  CHECK_EQ(lru.size(), 2);

  foo->expired = true;

  foo = lru.Upsert(addr3, now);
  foo->c = 3;
  foo->expired = false;
  CHECK_NOT_NULL(lru.Peek(addr3));
  CHECK_EQ(lru.Peek(addr3)->c, 3);
  CHECK_EQ(lru.size(), 1);

  // addr1 was removed because we exceeded size.
  // addr2 was removed because it was expired.
  CHECK_NULL(lru.Peek(addr1));
  CHECK_NULL(lru.Peek(addr2));
}

TEST(SocketAddress, Comparison) {
  sockaddr_storage storage[6];

  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &storage[0]);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.2", 0, &storage[1]);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 0, &storage[2]);
  SocketAddress::ToSockAddr(AF_INET6, "::2", 0, &storage[3]);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.1", 0, &storage[4]);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.2", 0, &storage[5]);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&storage[0]));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&storage[1]));
  SocketAddress addr3(reinterpret_cast<const sockaddr*>(&storage[2]));
  SocketAddress addr4(reinterpret_cast<const sockaddr*>(&storage[3]));
  SocketAddress addr5(reinterpret_cast<const sockaddr*>(&storage[4]));
  SocketAddress addr6(reinterpret_cast<const sockaddr*>(&storage[5]));

  CHECK_EQ(addr1.compare(addr1), std::partial_ordering::equivalent);
  CHECK_EQ(addr1.compare(addr2), std::partial_ordering::less);
  CHECK_EQ(addr2.compare(addr1), std::partial_ordering::greater);
  CHECK(addr1 <= addr1);
  CHECK(addr1 < addr2);
  CHECK(addr1 <= addr2);
  CHECK(addr2 >= addr2);
  CHECK(addr2 > addr1);
  CHECK(addr2 >= addr1);

  CHECK_EQ(addr3.compare(addr3), std::partial_ordering::equivalent);
  CHECK_EQ(addr3.compare(addr4), std::partial_ordering::less);
  CHECK_EQ(addr4.compare(addr3), std::partial_ordering::greater);
  CHECK(addr3 <= addr3);
  CHECK(addr3 < addr4);
  CHECK(addr3 <= addr4);
  CHECK(addr4 >= addr4);
  CHECK(addr4 > addr3);
  CHECK(addr4 >= addr3);

  // Not comparable
  CHECK_EQ(addr1.compare(addr3), std::partial_ordering::unordered);
  CHECK_EQ(addr3.compare(addr1), std::partial_ordering::unordered);
  CHECK(!(addr1 < addr3));
  CHECK(!(addr1 > addr3));
  CHECK(!(addr1 >= addr3));
  CHECK(!(addr1 <= addr3));
  CHECK(!(addr3 < addr1));
  CHECK(!(addr3 > addr1));
  CHECK(!(addr3 >= addr1));
  CHECK(!(addr3 <= addr1));

  // Comparable
  CHECK_EQ(addr1.compare(addr5), std::partial_ordering::equivalent);
  CHECK_EQ(addr2.compare(addr6), std::partial_ordering::equivalent);
  CHECK_EQ(addr1.compare(addr6), std::partial_ordering::less);
  CHECK_EQ(addr6.compare(addr1), std::partial_ordering::greater);
  CHECK(addr1 <= addr5);
  CHECK(addr1 <= addr6);
  CHECK(addr1 < addr6);
  CHECK(addr6 > addr1);
  CHECK(addr6 >= addr1);
  CHECK(addr2 >= addr6);
  CHECK(addr2 >= addr5);
}

TEST(SocketAddress, NewAutoFamily) {
  // SocketAddress::New(host, port) without explicit family.
  // Tries AF_INET first, then AF_INET6.
  SocketAddress addr;

  // IPv4 address should succeed.
  CHECK(SocketAddress::New("192.168.1.1", 8080, &addr));
  CHECK_EQ(addr.family(), AF_INET);
  CHECK_EQ(addr.address(), "192.168.1.1");
  CHECK_EQ(addr.port(), 8080);

  // IPv6 address should succeed (fails AF_INET, falls through to AF_INET6).
  CHECK(SocketAddress::New("::1", 443, &addr));
  CHECK_EQ(addr.family(), AF_INET6);
  CHECK_EQ(addr.address(), "::1");
  CHECK_EQ(addr.port(), 443);

  // Invalid address should fail.
  CHECK(!SocketAddress::New("not_an_address", 0, &addr));
}

TEST(SocketAddress, HashIPv6) {
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET6, "::1", 443, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 443, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::2", 443, &s3);

  SocketAddress a1(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress a2(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress a3(reinterpret_cast<const sockaddr*>(&s3));

  // Same address and port: hash must be equal.
  CHECK_EQ(SocketAddress::Hash()(a1), SocketAddress::Hash()(a2));

  // Different address: hash should (very likely) differ.
  CHECK_NE(SocketAddress::Hash()(a1), SocketAddress::Hash()(a3));
}

TEST(SocketAddress, IsMatchCrossFamily) {
  sockaddr_storage s1, s2, s3, s4;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.1", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 0, &s3);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.2", 0, &s4);

  SocketAddress ipv4(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress mapped(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress ipv6(reinterpret_cast<const sockaddr*>(&s3));
  SocketAddress other(reinterpret_cast<const sockaddr*>(&s4));

  // IPv4 matches its IPv4-mapped IPv6 counterpart.
  CHECK(ipv4.is_match(mapped));
  CHECK(mapped.is_match(ipv4));

  // IPv4 does not match a non-mapped IPv6 address.
  CHECK(!ipv4.is_match(ipv6));
  CHECK(!ipv6.is_match(ipv4));

  // Same family, different address.
  CHECK(!ipv4.is_match(other));

  // Self-match.
  CHECK(ipv4.is_match(ipv4));
  CHECK(ipv6.is_match(ipv6));
}

TEST(SocketAddress, InNetworkIPv4) {
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.100", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "192.168.0.0", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s3);

  SocketAddress addr(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress net(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress outside(reinterpret_cast<const sockaddr*>(&s3));

  CHECK(addr.is_in_network(net, 16));
  CHECK(!outside.is_in_network(net, 16));
  CHECK(!addr.is_in_network(net, 24));  // 192.168.1.x != 192.168.0.x
}

TEST(SocketAddress, InNetworkIPv6) {
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db9::1", 0, &s3);

  SocketAddress addr(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress net(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress outside(reinterpret_cast<const sockaddr*>(&s3));

  CHECK(addr.is_in_network(net, 32));
  CHECK(!outside.is_in_network(net, 32));

  // /128 prefix == exact match.
  CHECK(addr.is_in_network(addr, 128));
  CHECK(!outside.is_in_network(addr, 128));
}

TEST(SocketAddress, InNetworkCrossFamily) {
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.0", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.1", 0, &s3);

  SocketAddress ipv4(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress net6(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress mapped(reinterpret_cast<const sockaddr*>(&s3));

  // IPv4 address in an IPv4-mapped IPv6 subnet.
  CHECK(ipv4.is_in_network(net6, 120));  // prefix 120 = /24 on the IPv4 part
  CHECK(mapped.is_in_network(net6, 120));

  // IPv6 address in IPv4 network.
  sockaddr_storage s4;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.0", 0, &s4);
  SocketAddress net4(reinterpret_cast<const sockaddr*>(&s4));

  CHECK(mapped.is_in_network(net4, 24));
}

TEST(SocketAddressBlockList, Simple) {
  SocketAddressBlockList bl;

  sockaddr_storage storage[2];
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &storage[0]);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.2", 0, &storage[1]);
  std::shared_ptr<SocketAddress> addr1 = std::make_shared<SocketAddress>(
      reinterpret_cast<const sockaddr*>(&storage[0]));
  std::shared_ptr<SocketAddress> addr2 = std::make_shared<SocketAddress>(
      reinterpret_cast<const sockaddr*>(&storage[1]));

  bl.AddSocketAddress(*addr1);
  bl.AddSocketAddress(*addr2);

  CHECK(bl.Apply(*addr1));
  CHECK(bl.Apply(*addr2));

  bl.RemoveSocketAddress(*addr1);

  CHECK(!bl.Apply(*addr1));
  CHECK(bl.Apply(*addr2));
}

TEST(SocketAddressBlockList, CrossFamilyAddress) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.1", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::1", 0, &s3);

  SocketAddress ipv4(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress mapped(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress other(reinterpret_cast<const sockaddr*>(&s3));

  // Adding IPv4 should also match the IPv4-mapped IPv6 form.
  bl.AddSocketAddress(ipv4);
  CHECK(bl.Apply(ipv4));
  CHECK(bl.Apply(mapped));
  CHECK(!bl.Apply(other));

  // Remove should clean up cross-family counterpart.
  bl.RemoveSocketAddress(ipv4);
  CHECK(!bl.Apply(ipv4));
  CHECK(!bl.Apply(mapped));
}

TEST(SocketAddressBlockList, CrossFamilyAddressIPv6) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2;
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:192.168.1.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.1", 0, &s2);

  SocketAddress mapped(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress ipv4(reinterpret_cast<const sockaddr*>(&s2));

  // Adding an IPv4-mapped IPv6 address should also match the IPv4 form.
  bl.AddSocketAddress(mapped);
  CHECK(bl.Apply(mapped));
  CHECK(bl.Apply(ipv4));

  // Remove the IPv6 form should clean up the IPv4 counterpart.
  bl.RemoveSocketAddress(mapped);
  CHECK(!bl.Apply(mapped));
  CHECK(!bl.Apply(ipv4));
}

TEST(SocketAddressBlockList, BatchAddresses) {
  SocketAddressBlockList bl;

  sockaddr_storage storage[3];
  SocketAddress::ToSockAddr(AF_INET, "1.1.1.1", 0, &storage[0]);
  SocketAddress::ToSockAddr(AF_INET, "2.2.2.2", 0, &storage[1]);
  SocketAddress::ToSockAddr(AF_INET, "3.3.3.3", 0, &storage[2]);

  SocketAddress addrs[3] = {
      SocketAddress(reinterpret_cast<const sockaddr*>(&storage[0])),
      SocketAddress(reinterpret_cast<const sockaddr*>(&storage[1])),
      SocketAddress(reinterpret_cast<const sockaddr*>(&storage[2])),
  };

  bl.AddSocketAddresses(addrs, 3);

  CHECK(bl.Apply(addrs[0]));
  CHECK(bl.Apply(addrs[1]));
  CHECK(bl.Apply(addrs[2]));

  sockaddr_storage s4;
  SocketAddress::ToSockAddr(AF_INET, "4.4.4.4", 0, &s4);
  SocketAddress addr4(reinterpret_cast<const sockaddr*>(&s4));
  CHECK(!bl.Apply(addr4));
}

TEST(SocketAddressBlockList, Range) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3, s4, s5;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.10", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.5", 0, &s3);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.11", 0, &s4);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.0", 0, &s5);

  SocketAddress start(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress end(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress mid(reinterpret_cast<const sockaddr*>(&s3));
  SocketAddress above(reinterpret_cast<const sockaddr*>(&s4));
  SocketAddress below(reinterpret_cast<const sockaddr*>(&s5));

  bl.AddSocketAddressRange(start, end);

  CHECK(bl.Apply(start));
  CHECK(bl.Apply(end));
  CHECK(bl.Apply(mid));
  CHECK(!bl.Apply(above));
  CHECK(!bl.Apply(below));

  // Remove range.
  bl.RemoveSocketAddressRange(start, end);
  CHECK(!bl.Apply(mid));
}

TEST(SocketAddressBlockList, Subnet) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.0", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.100", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "192.168.2.1", 0, &s3);

  SocketAddress net(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress inside(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress outside(reinterpret_cast<const sockaddr*>(&s3));

  bl.AddSocketAddressMask(net, 24);

  CHECK(bl.Apply(inside));
  CHECK(!bl.Apply(outside));

  // Remove subnet.
  bl.RemoveSocketAddressMask(net, 24);
  CHECK(!bl.Apply(inside));
}

TEST(SocketAddressBlockList, SubnetIPv6) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::1", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db9::1", 0, &s3);

  SocketAddress net(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress inside(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress outside(reinterpret_cast<const sockaddr*>(&s3));

  bl.AddSocketAddressMask(net, 32);

  CHECK(bl.Apply(inside));
  CHECK(!bl.Apply(outside));

  bl.RemoveSocketAddressMask(net, 32);
  CHECK(!bl.Apply(inside));
}

TEST(SocketAddressBlockList, SubnetCrossFamily) {
  SocketAddressBlockList bl;

  // Adding an IPv4 subnet should also match IPv4-mapped IPv6 addresses.
  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.0", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.5", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET6, "::ffff:10.0.0.5", 0, &s3);

  SocketAddress net(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress ipv4(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress mapped(reinterpret_cast<const sockaddr*>(&s3));

  bl.AddSocketAddressMask(net, 24);

  CHECK(bl.Apply(ipv4));
  CHECK(bl.Apply(mapped));
}

TEST(SocketAddressBlockList, ClearAll) {
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "1.1.1.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.1", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "192.168.0.0", 0, &s3);

  SocketAddress addr(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress rangeStart(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress subnet(reinterpret_cast<const sockaddr*>(&s3));

  sockaddr_storage s4;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.10", 0, &s4);
  SocketAddress rangeEnd(reinterpret_cast<const sockaddr*>(&s4));

  bl.AddSocketAddress(addr);
  bl.AddSocketAddressRange(rangeStart, rangeEnd);
  bl.AddSocketAddressMask(subnet, 16);

  CHECK(bl.Apply(addr));
  CHECK(bl.Apply(rangeStart));

  sockaddr_storage s5;
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.1", 0, &s5);
  SocketAddress subnetAddr(reinterpret_cast<const sockaddr*>(&s5));
  CHECK(bl.Apply(subnetAddr));

  bl.Clear();

  CHECK(!bl.Apply(addr));
  CHECK(!bl.Apply(rangeStart));
  CHECK(!bl.Apply(subnetAddr));
}

TEST(SocketAddressBlockList, ParentBlockList) {
  auto parent = std::make_shared<SocketAddressBlockList>();
  SocketAddressBlockList child(parent);

  sockaddr_storage s1, s2;
  SocketAddress::ToSockAddr(AF_INET, "1.1.1.1", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "2.2.2.2", 0, &s2);

  SocketAddress addr1(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress addr2(reinterpret_cast<const sockaddr*>(&s2));

  parent->AddSocketAddress(addr1);
  child.AddSocketAddress(addr2);

  // Child should match both its own rules and parent's.
  CHECK(child.Apply(addr1));
  CHECK(child.Apply(addr2));

  // Parent should only match its own rules.
  CHECK(parent->Apply(addr1));
  CHECK(!parent->Apply(addr2));
}

TEST(SocketAddressBlockList, SubnetOverlapRemoval) {
  // Removing a broader subnet must restore narrower subnets that were
  // subsumed by the broader prefix in the trie.
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3;
  SocketAddress::ToSockAddr(AF_INET, "10.0.0.0", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET, "10.1.0.0", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "10.1.2.3", 0, &s3);

  SocketAddress broad(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress narrow(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress target(reinterpret_cast<const sockaddr*>(&s3));

  bl.AddSocketAddressMask(broad, 8);    // 10.0.0.0/8
  bl.AddSocketAddressMask(narrow, 16);  // 10.1.0.0/16 (subsumed by /8)

  CHECK(bl.Apply(target));  // Covered by /8.

  bl.RemoveSocketAddressMask(broad, 8);

  // After removing /8, the /16 must still work.
  CHECK(bl.Apply(target));

  // Address outside /16 but inside old /8 should no longer match.
  sockaddr_storage s4;
  SocketAddress::ToSockAddr(AF_INET, "10.2.0.1", 0, &s4);
  SocketAddress outside(reinterpret_cast<const sockaddr*>(&s4));
  CHECK(!bl.Apply(outside));
}

TEST(SocketAddressBlockList, SubnetRemoveMixedFamily) {
  // Removing one family's subnet must correctly rebuild the remaining
  // rules, including those from the other family.
  SocketAddressBlockList bl;

  sockaddr_storage s1, s2, s3, s4;
  SocketAddress::ToSockAddr(AF_INET, "192.168.0.0", 0, &s1);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::", 0, &s2);
  SocketAddress::ToSockAddr(AF_INET, "192.168.1.1", 0, &s3);
  SocketAddress::ToSockAddr(AF_INET6, "2001:db8::1", 0, &s4);

  SocketAddress ipv4Net(reinterpret_cast<const sockaddr*>(&s1));
  SocketAddress ipv6Net(reinterpret_cast<const sockaddr*>(&s2));
  SocketAddress ipv4Addr(reinterpret_cast<const sockaddr*>(&s3));
  SocketAddress ipv6Addr(reinterpret_cast<const sockaddr*>(&s4));

  bl.AddSocketAddressMask(ipv4Net, 16);
  bl.AddSocketAddressMask(ipv6Net, 32);

  CHECK(bl.Apply(ipv4Addr));
  CHECK(bl.Apply(ipv6Addr));

  // Remove IPv4 subnet — IPv6 subnet must survive the rebuild.
  bl.RemoveSocketAddressMask(ipv4Net, 16);
  CHECK(!bl.Apply(ipv4Addr));
  CHECK(bl.Apply(ipv6Addr));

  // Re-add IPv4, then remove IPv6 — IPv4 must survive.
  bl.AddSocketAddressMask(ipv4Net, 16);
  bl.RemoveSocketAddressMask(ipv6Net, 32);
  CHECK(bl.Apply(ipv4Addr));
  CHECK(!bl.Apply(ipv6Addr));
}
