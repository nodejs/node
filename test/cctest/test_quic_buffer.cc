#include "quic/node_quic_buffer-inl.h"
#include "node_bob-inl.h"
#include "util-inl.h"
#include "uv.h"

#include "gtest/gtest.h"
#include <memory>
#include <vector>

using node::quic::QuicBuffer;
using node::quic::QuicBufferChunk;
using node::bob::Status;
using node::bob::Options;
using node::bob::Done;
using ::testing::AssertionSuccess;
using ::testing::AssertionFailure;

::testing::AssertionResult IsEqual(size_t actual, int expected) {
  return (static_cast<size_t>(expected) == actual) ? AssertionSuccess() :
      AssertionFailure() << actual << " is not equal to " << expected;
}

TEST(QuicBuffer, Simple) {
  char data[100];
  memset(&data, 0, node::arraysize(data));
  uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

  bool done = false;

  QuicBuffer buffer;
  buffer.Push(&buf, 1, [&](int status) {
    EXPECT_EQ(0, status);
    done = true;
  });

  buffer.Consume(100);
  ASSERT_TRUE(IsEqual(buffer.length(), 0));

  // We have to move the read head forward in order to consume
  buffer.Seek(1);
  buffer.Consume(100);
  ASSERT_TRUE(done);
  ASSERT_TRUE(IsEqual(buffer.length(), 0));
}

TEST(QuicBuffer, ConsumeMore) {
  char data[100];
  memset(&data, 0, node::arraysize(data));
  uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

  bool done = false;

  QuicBuffer buffer;
  buffer.Push(&buf, 1, [&](int status) {
    EXPECT_EQ(0, status);
    done = true;
  });

  buffer.Seek(1);
  buffer.Consume(150);  // Consume more than what was buffered
  ASSERT_TRUE(done);
  ASSERT_TRUE(IsEqual(buffer.length(), 0));
}

TEST(QuicBuffer, Multiple) {
  uv_buf_t bufs[] {
    uv_buf_init(const_cast<char*>("abcdefghijklmnopqrstuvwxyz"), 26),
    uv_buf_init(const_cast<char*>("zyxwvutsrqponmlkjihgfedcba"), 26)
  };

  QuicBuffer buf;
  bool done = false;
  buf.Push(bufs, 2, [&](int status) { done = true; });

  buf.Seek(2);
  ASSERT_TRUE(IsEqual(buf.remaining(), 50));
  ASSERT_TRUE(IsEqual(buf.length(), 52));

  buf.Consume(25);
  ASSERT_TRUE(IsEqual(buf.length(), 27));

  buf.Consume(25);
  ASSERT_TRUE(IsEqual(buf.length(), 2));

  buf.Consume(2);
  ASSERT_TRUE(IsEqual(buf.length(), 0));
}

TEST(QuicBuffer, Multiple2) {
  char* ptr = new char[100];
  memset(ptr, 0, 50);
  memset(ptr + 50, 1, 50);

  uv_buf_t bufs[] = {
    uv_buf_init(ptr, 50),
    uv_buf_init(ptr + 50, 50)
  };

  int count = 0;

  QuicBuffer buffer;
  buffer.Push(
      bufs, node::arraysize(bufs),
      [&](int status) {
    count++;
    ASSERT_EQ(0, status);
    delete[] ptr;
  });
  buffer.Seek(node::arraysize(bufs));

  buffer.Consume(25);
  ASSERT_TRUE(IsEqual(buffer.length(), 75));
  buffer.Consume(25);
  ASSERT_TRUE(IsEqual(buffer.length(), 50));
  buffer.Consume(25);
  ASSERT_TRUE(IsEqual(buffer.length(), 25));
  buffer.Consume(25);
  ASSERT_TRUE(IsEqual(buffer.length(), 0));

  // The callback was only called once tho
  ASSERT_EQ(1, count);
}

TEST(QuicBuffer, Cancel) {
  char* ptr = new char[100];
  memset(ptr, 0, 50);
  memset(ptr + 50, 1, 50);

  uv_buf_t bufs[] = {
    uv_buf_init(ptr, 50),
    uv_buf_init(ptr + 50, 50)
  };

  int count = 0;

  QuicBuffer buffer;
  buffer.Push(
      bufs, node::arraysize(bufs),
      [&](int status) {
    count++;
    ASSERT_EQ(UV_ECANCELED, status);
    delete[] ptr;
  });

  buffer.Seek(1);
  buffer.Consume(25);
  ASSERT_TRUE(IsEqual(buffer.length(), 75));
  buffer.Cancel();
  ASSERT_TRUE(IsEqual(buffer.length(), 0));

  // The callback was only called once tho
  ASSERT_EQ(1, count);
}

TEST(QuicBuffer, Move) {
  QuicBuffer buffer1;
  QuicBuffer buffer2;

  char data[100];
  memset(&data, 0, node::arraysize(data));
  uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

  buffer1.Push(&buf, 1);

  ASSERT_TRUE(IsEqual(buffer1.length(), 100));

  buffer2 = std::move(buffer1);
  ASSERT_TRUE(IsEqual(buffer1.length(), 0));
  ASSERT_TRUE(IsEqual(buffer2.length(), 100));
}

TEST(QuicBuffer, QuicBufferChunk) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(100);
  memset(chunk->out(), 1, 100);

  QuicBuffer buffer;
  buffer.Push(std::move(chunk));
  buffer.End();
  ASSERT_TRUE(IsEqual(buffer.length(), 100));

  auto next = [&](
      int status,
      const ngtcp2_vec* data,
      size_t count,
      Done done) {
    ASSERT_EQ(status, Status::STATUS_END);
    ASSERT_TRUE(IsEqual(count, 1));
    ASSERT_NE(data, nullptr);
    done(100);
  };

  ASSERT_TRUE(IsEqual(buffer.remaining(), 100));

  ngtcp2_vec data[2];
  size_t len = sizeof(data) / sizeof(ngtcp2_vec);
  buffer.Pull(next, Options::OPTIONS_SYNC | Options::OPTIONS_END, data, len);

  ASSERT_TRUE(IsEqual(buffer.remaining(), 0));

  buffer.Consume(50);
  ASSERT_TRUE(IsEqual(buffer.length(), 50));

  buffer.Consume(50);
  ASSERT_TRUE(IsEqual(buffer.length(), 0));
}
