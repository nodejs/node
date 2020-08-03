#include "node_sockaddr-inl.h"
#include "gtest/gtest.h"

using node::SocketAddress;
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

    static bool CheckExpired(const SocketAddress& address, const Type& type) {
      return type.expired;
    }

    static void Touch(const SocketAddress& address, Type* type) {
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

  Foo* foo = lru.Upsert(addr1);
  CHECK_NOT_NULL(foo);
  CHECK_EQ(foo->c, 0);
  CHECK_EQ(foo->expired, false);

  foo->c = 1;
  foo->expired = true;

  foo = lru.Upsert(addr1);
  CHECK_NOT_NULL(lru.Peek(addr1));
  CHECK_EQ(lru.Peek(addr1), lru.Peek(addr4));
  CHECK_EQ(lru.Peek(addr1)->c, 1);
  CHECK_EQ(lru.Peek(addr1)->expired, false);
  CHECK_EQ(lru.size(), 1);

  foo = lru.Upsert(addr2);
  foo->c = 2;
  foo->expired = true;
  CHECK_NOT_NULL(lru.Peek(addr2));
  CHECK_EQ(lru.Peek(addr2)->c, 2);
  CHECK_EQ(lru.size(), 2);

  foo->expired = true;

  foo = lru.Upsert(addr3);
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
