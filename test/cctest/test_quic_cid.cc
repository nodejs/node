#include "quic/node_quic_util-inl.h"
#include "node_sockaddr-inl.h"
#include "util-inl.h"
#include "ngtcp2/ngtcp2.h"
#include "gtest/gtest.h"
#include <memory>
#include <vector>

using node::quic::QuicCID;

TEST(QuicCID, Simple) {
  ngtcp2_cid cid1;
  ngtcp2_cid cid2;
  uint8_t data1[3] = { 'a', 'b', 'c' };
  uint8_t data2[4] = { 1, 2, 3, 4 };
  ngtcp2_cid_init(&cid1, data1, 3);
  ngtcp2_cid_init(&cid2, data2, 4);

  QuicCID qcid1(cid1);
  CHECK(qcid1);
  CHECK_EQ(qcid1.length(), 3);
  CHECK_EQ(qcid1.ToString(), "616263");

  QuicCID qcid2(cid2);
  qcid1 = qcid2;
  CHECK_EQ(qcid1.ToString(), qcid2.ToString());

  qcid1.set_length(5);
  memset(qcid1.data(), 1, 5);
  CHECK_EQ(qcid1.ToString(), "0101010101");
}
