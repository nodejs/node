#include "node_quic_buffer.h"
#include "util-inl.h"
#include "uv.h"

#include "gtest/gtest.h"
#include <memory>
#include <vector>

using node::quic::QuicBuffer;

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
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());

  // We have to move the read head forward in order to consume
  buffer.SeekHead(1);
  buffer.Consume(100);
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());
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

  buffer.SeekHead();
  buffer.Consume(150);  // Consume more than what was buffered
  CHECK_EQ(true, done);
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());
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

  buffer.SeekHead(2);

  buffer.Consume(25);
  CHECK_EQ(125, buffer.Length());
  CHECK_EQ(2, buffer.Size());

  buffer.Consume(100);
  CHECK_EQ(25, buffer.Length());
  CHECK_EQ(1, buffer.Size());

  buffer.Consume(25);
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());
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
  buffer.SeekHead(node::arraysize(bufs));

  buffer.Consume(25);
  CHECK_EQ(2, buffer.Size());
  CHECK_EQ(75, buffer.Length());
  buffer.Consume(25);
  CHECK_EQ(1, buffer.Size());
  CHECK_EQ(50, buffer.Length());
  buffer.Consume(25);
  CHECK_EQ(1, buffer.Size());
  CHECK_EQ(25, buffer.Length());
  buffer.Consume(25);
  CHECK_EQ(0, buffer.Size());
  CHECK_EQ(0, buffer.Length());

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

  buffer.SeekHead();
  buffer.Consume(25);
  CHECK_EQ(2, buffer.Size());
  CHECK_EQ(75, buffer.Length());
  buffer.Cancel();
  CHECK_EQ(0, buffer.Size());
  CHECK_EQ(0, buffer.Length());

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
  CHECK_EQ(150, buffer.Length());
  CHECK_EQ(2, buffer.Size());

  buffer.SeekHead(2);

  buffer.Consume(25);
  CHECK_EQ(125, buffer.Length());
  CHECK_EQ(2, buffer.Size());

  buffer.Consume(100);
  CHECK_EQ(25, buffer.Length());
  CHECK_EQ(1, buffer.Size());

  {
    uv_buf_t b = buf2.ToUVBuf();
    buffer.Push(&b, 1, [&](int status) { cb(status, &buf3); });
  }

  CHECK_EQ(75, buffer.Length());
  CHECK_EQ(2, buffer.Size());

  buffer.SeekHead();
  buffer.Consume(75);
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());
}

TEST(QuicBuffer, Move) {
  QuicBuffer buffer1;
  QuicBuffer buffer2;

  char data[100];
  memset(&data, 0, node::arraysize(data));
  uv_buf_t buf = uv_buf_init(data, node::arraysize(data));

  buffer1.Push(&buf, 1);

  CHECK_EQ(100, buffer1.Length());
  CHECK_EQ(1, buffer1.Size());

  buffer2 = std::move(buffer1);
  CHECK_EQ(0, buffer1.Length());
  CHECK_EQ(0, buffer1.Size());
  CHECK_EQ(100, buffer2.Length());
  CHECK_EQ(1, buffer2.Size());
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

  CHECK_EQ(100, buffer1.Length());
  CHECK_EQ(1, buffer1.Size());
  CHECK_EQ(100, buffer2.Length());
  CHECK_EQ(1, buffer2.Size());

  buffer2 += std::move(buffer1);

  CHECK_EQ(0, buffer1.Length());
  CHECK_EQ(0, buffer1.Size());
  CHECK_EQ(200, buffer2.Length());
  CHECK_EQ(2, buffer2.Size());
}

TEST(QuicBuffer, MallocedBuffer) {
  uint8_t* data = node::Malloc<uint8_t>(100);
  int count = 0;
  auto cb = [&](int status) {
    count++;
  };

  QuicBuffer buffer;
  buffer.Push(node::MallocedBuffer<uint8_t>(data, 100), cb);
  CHECK_EQ(1, buffer.Size());
  CHECK_EQ(100, buffer.Length());

  std::vector<uv_buf_t> vec;
  size_t len = buffer.DrainInto(&vec);
  CHECK_EQ(1, vec.size());
  buffer.SeekHead(len);

  buffer.Consume(50);
  CHECK_EQ(1, buffer.Size());
  CHECK_EQ(50, buffer.Length());
  CHECK_EQ(0, count);

  buffer.Consume(50);
  CHECK_EQ(0, buffer.Size());
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(1, count);
}

TEST(QuicBuffer, Head) {
  uint8_t* data = node::Malloc<uint8_t>(100);
  memset(data, 0, 100);
  QuicBuffer buffer;
  buffer.Push(node::MallocedBuffer<uint8_t>(data, 100));
  CHECK_EQ(1, buffer.Size());

  // buffer.Head() returns the current read head
  {
    uv_buf_t buf = buffer.Head();
    CHECK_EQ(100, buf.len);
    CHECK_EQ(0, buf.base[0]);
  }

  buffer.Consume(50);
  CHECK_EQ(1, buffer.Size());

  {
    uv_buf_t buf = buffer.Head();
    CHECK_EQ(100, buf.len);
    CHECK_EQ(0, buf.base[0]);
  }

  // Seeking the head to the end will
  // result in an empty head
  buffer.SeekHead();
  {
    uv_buf_t buf = buffer.Head();
    CHECK_EQ(0, buf.len);
    CHECK_EQ(nullptr, buf.base);
  }
  // But the buffer will still have unconsumed data
  CHECK_EQ(50, buffer.Length());
  CHECK_EQ(1, buffer.Size());

  buffer.Consume(100);
  CHECK_EQ(0, buffer.Length());
  CHECK_EQ(0, buffer.Size());
}
