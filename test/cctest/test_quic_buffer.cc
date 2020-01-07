#include "quic/node_quic_buffer-inl.h"
#include "util-inl.h"
#include "uv.h"

#include "gtest/gtest.h"
#include <memory>
#include <vector>

using node::quic::QuicBuffer;
using node::quic::QuicBufferChunk;

class TestBuffer {
 public:
  explicit TestBuffer(size_t size, int val = 0) {
    buf_.AllocateSufficientStorage(size);
    buf_.SetLength(size);
    memset(*buf_, val, size);
  }

  ~TestBuffer() {
    CHECK_EQ(true, done_);
  }

  uv_buf_t ToUVBuf() {
    return uv_buf_init(*buf_, buf_.length());
  }

  void Done() {
    CHECK_EQ(false, done_);
    done_ = true;
  }

 private:
  node::MaybeStackBuffer<char> buf_;
  bool done_ = false;
};

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
  CHECK_EQ(0, buffer.length());

  // We have to move the read head forward in order to consume
  buffer.Seek(1);
  buffer.Consume(100);
  CHECK_EQ(0, buffer.length());
  CHECK_EQ(true, done);
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
  CHECK_EQ(true, done);
  CHECK_EQ(0, buffer.length());
}

TEST(QuicBuffer, Multiple) {
  TestBuffer buf1(100);
  TestBuffer buf2(50, 1);

  auto cb = [](int status, TestBuffer* test_buffer) {
    test_buffer->Done();
  };

  QuicBuffer buffer;
  {
    uv_buf_t b = buf1.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf1); });
  }
  {
    uv_buf_t b = buf2.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf2); });
  }

  buffer.Seek(2);

  buffer.Consume(25);
  CHECK_EQ(125, buffer.length());

  buffer.Consume(100);
  CHECK_EQ(25, buffer.length());

  buffer.Consume(25);
  CHECK_EQ(0, buffer.length());
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
    CHECK_EQ(0, status);
    delete[] ptr;
  });
  buffer.Seek(node::arraysize(bufs));

  buffer.Consume(25);
  CHECK_EQ(75, buffer.length());
  buffer.Consume(25);
  CHECK_EQ(50, buffer.length());
  buffer.Consume(25);
  CHECK_EQ(25, buffer.length());
  buffer.Consume(25);
  CHECK_EQ(0, buffer.length());

  // The callback was only called once tho
  CHECK_EQ(1, count);
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
    CHECK_EQ(UV_ECANCELED, status);
    delete[] ptr;
  });

  buffer.Seek(1);
  buffer.Consume(25);
  CHECK_EQ(75, buffer.length());
  buffer.Cancel();
  CHECK_EQ(0, buffer.length());

  // The callback was only called once tho
  CHECK_EQ(1, count);
}

TEST(QuicBuffer, Multiple3) {
  TestBuffer buf1(100);
  TestBuffer buf2(50, 1);
  TestBuffer buf3(50, 2);

  auto cb = [](int status, TestBuffer* test_buffer) {
    test_buffer->Done();
  };

  QuicBuffer buffer;
  {
    uv_buf_t b = buf1.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf1); });
  }
  {
    uv_buf_t b = buf2.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf2); });
  }
  CHECK_EQ(150, buffer.length());

  buffer.Seek(2);

  buffer.Consume(25);
  CHECK_EQ(125, buffer.length());

  buffer.Consume(100);
  CHECK_EQ(25, buffer.length());

  {
    uv_buf_t b = buf2.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf3); });
  }

  CHECK_EQ(75, buffer.length());

  buffer.Seek(1);
  buffer.Consume(75);
  CHECK_EQ(0, buffer.length());
}

TEST(QuicBuffer, Move) {
  QuicBuffer buffer1;
  QuicBuffer buffer2;

  char data[100];
  memset(&data, 0, node::arraysize(data));
  uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

  buffer1.Push(&buf, 1);

  CHECK_EQ(100, buffer1.length());

  buffer2 = std::move(buffer1);
  CHECK_EQ(0, buffer1.length());
  CHECK_EQ(100, buffer2.length());
}

TEST(QuicBuffer, Append) {
  QuicBuffer buffer1;
  QuicBuffer buffer2;

  {
    char data[100];
    memset(&data, 0, node::arraysize(data));
    uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

    buffer1.Push(&buf, 1);
  }

  {
    char data[100];
    memset(&data, 1, node::arraysize(data));
    uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

    buffer2.Push(&buf, 1);
  }

  CHECK_EQ(100, buffer1.length());
  CHECK_EQ(100, buffer2.length());

  buffer2 += std::move(buffer1);

  CHECK_EQ(0, buffer1.length());
  CHECK_EQ(200, buffer2.length());
}

TEST(QuicBuffer, DISABLED_QuicBufferChunk) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(100);

  QuicBuffer buffer;
  buffer.Push(std::move(chunk));
  CHECK_EQ(100, buffer.length());

  std::vector<uv_buf_t> vec;
  size_t len = buffer.DrainInto(&vec);
  buffer.Seek(len);

  buffer.Consume(50);
  CHECK_EQ(50, buffer.length());

  buffer.Consume(50);
  CHECK_EQ(0, buffer.length());
}

TEST(QuicBuffer, DISABLED_Head) {
  std::unique_ptr<QuicBufferChunk> chunk =
      std::make_unique<QuicBufferChunk>(100);
  memset(chunk->out(), 0, 100);
  QuicBuffer buffer;
  buffer.Push(std::move(chunk));

  // buffer.Head() returns the current read head
  {
    uv_buf_t buf = buffer.head();
    CHECK_EQ(100, buf.len);
    CHECK_EQ(0, buf.base[0]);
  }

  buffer.Consume(50);

  {
    uv_buf_t buf = buffer.head();
    CHECK_EQ(100, buf.len);
    CHECK_EQ(0, buf.base[0]);
  }

  // Seeking the head to the end will
  // result in an empty head
  buffer.Seek(1);
  {
    uv_buf_t buf = buffer.head();
    CHECK_EQ(0, buf.len);
    CHECK_EQ(nullptr, buf.base);
  }
  // But the buffer will still have unconsumed data
  CHECK_EQ(50, buffer.length());

  buffer.Consume(100);
  CHECK_EQ(0, buffer.length());
}
