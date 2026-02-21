#if HAVE_OPENSSL
#include "quic/guard.h"
#ifndef OPENSSL_NO_QUIC
#include <env-inl.h>
#include <gtest/gtest.h>
#include <ngtcp2/ngtcp2.h>
#include <quic/cid.h>
#include <util-inl.h>
#include <string>
#include <unordered_map>

using node::quic::CID;

TEST(CID, Basic) {
  auto& random = CID::Factory::random();
  {
    // Check that the kInvalid CID is indeed invalid.
    CHECK(!CID::kInvalid);
  }
  {
    const auto cid = random.Generate();
    CHECK_EQ(cid.length(), CID::kMaxLength);
    CHECK(cid);
    CHECK_EQ(cid, cid);
  }
  {
    const auto cid = random.Generate(5);
    CHECK_EQ(cid.length(), 5);
    CHECK(cid);
  }
  {
    const auto cid1 = random.Generate();
    const auto cid2 = random.Generate();
    CHECK_NE(cid1, cid2);
  }
  {
    const auto cid1 = random.Generate(5);
    const auto cid2 = random.Generate();
    CHECK_NE(cid1, cid2);
  }
  {
    const auto cid1 = random.Generate();
    const auto cid2 = random.Generate(5);
    CHECK_NE(cid1, cid2);
  }
  {
    // They are copy constructible...
    CID cid = CID::kInvalid;
    CHECK(!cid);
    CHECK_EQ(cid.length(), 0);
    CHECK_EQ(CID::kInvalid, cid);

    const auto cid2 = random.Generate();
    const auto cid3 = cid2;
    CHECK_EQ(cid2, cid3);
  }
  {
    const auto cid1 = random.Generate();
    const auto cid2 = random.Generate();
    const auto cid3 = random.Generate();

    CID::Map<std::string> map;
    map[cid1] = "hello";
    map[cid2] = "there";
    CHECK_EQ(map[cid1], "hello");
    CHECK_EQ(map[cid2], "there");
    CHECK_NE(map[cid2], "hello");
    CHECK_NE(map[cid1], "there");

    // Remove the two CIDs.
    CHECK_EQ(map.erase(cid1), 1);
    CHECK_EQ(map.erase(cid2), 1);

    // Make sure they were removed.
    CHECK_EQ(map.find(cid1), map.end());
    CHECK_EQ(map.find(cid2), map.end());

    // The cid3 is not in the map, so it should not be found.
    CHECK_EQ(map.find(cid3), map.end());
  }
  {
    ngtcp2_cid cid_;
    uint8_t data[] = {1, 2, 3, 4, 5};
    ngtcp2_cid_init(&cid_, data, 5);
    const CID cid(cid_);
    // This variation of the constructor copies the cid_, so if we
    // modify the original data it doesn't change in the CID.
    cid_.data[0] = 9;
    CHECK_EQ(cid.length(), 5);
    CHECK_EQ(cid.ToString(), "0102030405");
  }
  {
    ngtcp2_cid cid_;
    uint8_t data[] = {1, 2, 3, 4, 5};
    ngtcp2_cid_init(&cid_, data, 5);
    const CID cid(&cid_);
    // This variation of the constructor wraps the cid_, so if we
    // modify the original data it does change in the CID... not
    // that this is something we would normally do.
    cid_.data[0] = 9;
    CHECK_EQ(cid.length(), 5);
    CHECK_EQ(cid.ToString(), "0902030405");
  }
  {
    // Generate a bunch to ensure that the pool is regenerated.
    for (int n = 0; n < 5000; n++) {
      CHECK(random.Generate());
    }
  }
  {
    ngtcp2_cid cid_;
    // Generate a bunch to ensure that the pool is regenerated.
    for (int n = 0; n < 5000; n++) {
      CHECK(random.GenerateInto(&cid_, 10));
      CHECK_EQ(cid_.datalen, 10);
    }
  }
}
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL
