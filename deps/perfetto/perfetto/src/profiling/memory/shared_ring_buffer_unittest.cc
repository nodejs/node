/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/shared_ring_buffer.h"

#include <array>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include "perfetto/ext/base/optional.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

std::string ToString(const SharedRingBuffer::Buffer& buf_and_size) {
  return std::string(reinterpret_cast<const char*>(&buf_and_size.data[0]),
                     buf_and_size.size);
}

bool TryWrite(SharedRingBuffer* wr, const char* src, size_t size) {
  SharedRingBuffer::Buffer buf;
  {
    auto lock = wr->AcquireLock(ScopedSpinlock::Mode::Try);
    if (!lock.locked())
      return false;
    buf = wr->BeginWrite(lock, size);
  }
  if (!buf)
    return false;
  memcpy(buf.data, src, size);
  wr->EndWrite(std::move(buf));
  return true;
}

void StructuredTest(SharedRingBuffer* wr, SharedRingBuffer* rd) {
  ASSERT_TRUE(wr);
  ASSERT_TRUE(wr->is_valid());
  ASSERT_TRUE(wr->size() == rd->size());
  const size_t buf_size = wr->size();

  // Test small writes.
  ASSERT_TRUE(TryWrite(wr, "foo", 4));
  ASSERT_TRUE(TryWrite(wr, "bar", 4));

  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(buf_and_size.size, 4u);
    ASSERT_STREQ(reinterpret_cast<const char*>(&buf_and_size.data[0]), "foo");
    rd->EndRead(std::move(buf_and_size));
  }
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(buf_and_size.size, 4u);
    ASSERT_STREQ(reinterpret_cast<const char*>(&buf_and_size.data[0]), "bar");
    rd->EndRead(std::move(buf_and_size));
  }

  for (int i = 0; i < 3; i++) {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(buf_and_size.data, nullptr);
    ASSERT_EQ(buf_and_size.size, 0u);
  }

  // Test extremely large writes (fill the buffer)
  for (int i = 0; i < 3; i++) {
    // TryWrite precisely |buf_size| bytes (minus the size header itself).
    std::string data(buf_size - sizeof(uint64_t), '.' + static_cast<char>(i));
    ASSERT_TRUE(TryWrite(wr, data.data(), data.size()));
    ASSERT_FALSE(TryWrite(wr, data.data(), data.size()));
    ASSERT_FALSE(TryWrite(wr, "?", 1));

    // And read it back
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), data);
    rd->EndRead(std::move(buf_and_size));
  }

  // Test large writes that wrap.
  std::string data(buf_size / 4 * 3 - sizeof(uint64_t), '!');
  ASSERT_TRUE(TryWrite(wr, data.data(), data.size()));
  ASSERT_FALSE(TryWrite(wr, data.data(), data.size()));
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), data);
    rd->EndRead(std::move(buf_and_size));
  }
  data = std::string(base::kPageSize - sizeof(uint64_t), '#');
  for (int i = 0; i < 4; i++)
    ASSERT_TRUE(TryWrite(wr, data.data(), data.size()));

  for (int i = 0; i < 4; i++) {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(buf_and_size.size, data.size());
    ASSERT_EQ(ToString(buf_and_size), data);
    rd->EndRead(std::move(buf_and_size));
  }

  // Test misaligned writes.
  ASSERT_TRUE(TryWrite(wr, "1", 1));
  ASSERT_TRUE(TryWrite(wr, "22", 2));
  ASSERT_TRUE(TryWrite(wr, "333", 3));
  ASSERT_TRUE(TryWrite(wr, "55555", 5));
  ASSERT_TRUE(TryWrite(wr, "7777777", 7));
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), "1");
    rd->EndRead(std::move(buf_and_size));
  }
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), "22");
    rd->EndRead(std::move(buf_and_size));
  }
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), "333");
    rd->EndRead(std::move(buf_and_size));
  }
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), "55555");
    rd->EndRead(std::move(buf_and_size));
  }
  {
    auto buf_and_size = rd->BeginRead();
    ASSERT_EQ(ToString(buf_and_size), "7777777");
    rd->EndRead(std::move(buf_and_size));
  }
}

TEST(SharedRingBufferTest, ReadShutdown) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> wr = SharedRingBuffer::Create(kBufSize);
  ASSERT_TRUE(wr);
  SharedRingBuffer rd =
      *SharedRingBuffer::Attach(base::ScopedFile(dup(wr->fd())));
  auto buf = rd.BeginRead();
  wr = base::nullopt;
  rd.EndRead(std::move(buf));
}

TEST(SharedRingBufferTest, WriteShutdown) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> rd = SharedRingBuffer::Create(kBufSize);
  ASSERT_TRUE(rd);
  SharedRingBuffer wr =
      *SharedRingBuffer::Attach(base::ScopedFile(dup(rd->fd())));
  SharedRingBuffer::Buffer buf;
  {
    auto lock = wr.AcquireLock(ScopedSpinlock::Mode::Blocking);
    buf = wr.BeginWrite(lock, 10);
  }
  rd = base::nullopt;
  memset(buf.data, 0, buf.size);
  wr.EndWrite(std::move(buf));
}

TEST(SharedRingBufferTest, SingleThreadSameInstance) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> buf = SharedRingBuffer::Create(kBufSize);
  StructuredTest(&*buf, &*buf);
}

TEST(SharedRingBufferTest, SingleThreadAttach) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> buf1 = SharedRingBuffer::Create(kBufSize);
  base::Optional<SharedRingBuffer> buf2 =
      SharedRingBuffer::Attach(base::ScopedFile(dup(buf1->fd())));
  StructuredTest(&*buf1, &*buf2);
}

TEST(SharedRingBufferTest, MultiThreadingTest) {
  constexpr auto kBufSize = base::kPageSize * 1024;  // 4 MB
  SharedRingBuffer rd = *SharedRingBuffer::Create(kBufSize);
  SharedRingBuffer wr =
      *SharedRingBuffer::Attach(base::ScopedFile(dup(rd.fd())));

  std::mutex mutex;
  std::unordered_map<std::string, int64_t> expected_contents;
  std::atomic<bool> writers_enabled{false};

  auto writer_thread_fn = [&wr, &expected_contents, &mutex,
                           &writers_enabled](size_t thread_id) {
    while (!writers_enabled.load()) {
    }
    std::minstd_rand0 rnd_engine(static_cast<uint32_t>(thread_id));
    std::uniform_int_distribution<size_t> dist(1, base::kPageSize * 8);
    for (int i = 0; i < 1000; i++) {
      size_t size = dist(rnd_engine);
      ASSERT_GT(size, 0u);
      std::string data;
      data.resize(size);
      std::generate(data.begin(), data.end(), rnd_engine);
      if (TryWrite(&wr, data.data(), data.size())) {
        std::lock_guard<std::mutex> lock(mutex);
        expected_contents[std::move(data)]++;
      } else {
        std::this_thread::yield();
      }
    }
  };

  auto reader_thread_fn = [&rd, &expected_contents, &mutex, &writers_enabled] {
    for (;;) {
      auto buf_and_size = rd.BeginRead();
      if (!buf_and_size) {
        if (!writers_enabled.load()) {
          // Failing to read after the writers are done means that there is no
          // data left in the ring buffer.
          return;
        } else {
          std::this_thread::yield();
          continue;
        }
      }
      ASSERT_GT(buf_and_size.size, 0u);
      std::string data = ToString(buf_and_size);
      std::lock_guard<std::mutex> lock(mutex);
      expected_contents[std::move(data)]--;
      rd.EndRead(std::move(buf_and_size));
    }
  };

  constexpr size_t kNumWriterThreads = 4;
  std::array<std::thread, kNumWriterThreads> writer_threads;
  for (size_t i = 0; i < kNumWriterThreads; i++)
    writer_threads[i] = std::thread(writer_thread_fn, i);

  writers_enabled.store(true);

  std::thread reader_thread(reader_thread_fn);

  for (size_t i = 0; i < kNumWriterThreads; i++)
    writer_threads[i].join();

  writers_enabled.store(false);

  reader_thread.join();
}

TEST(SharedRingBufferTest, InvalidSize) {
  constexpr auto kBufSize = base::kPageSize * 4 + 1;
  base::Optional<SharedRingBuffer> wr = SharedRingBuffer::Create(kBufSize);
  EXPECT_EQ(wr, base::nullopt);
}

TEST(SharedRingBufferTest, EmptyWrite) {
  constexpr auto kBufSize = base::kPageSize * 4;
  base::Optional<SharedRingBuffer> wr = SharedRingBuffer::Create(kBufSize);
  ASSERT_TRUE(wr);
  SharedRingBuffer::Buffer buf;
  {
    auto lock = wr->AcquireLock(ScopedSpinlock::Mode::Try);
    ASSERT_TRUE(lock.locked());
    buf = wr->BeginWrite(lock, 0);
  }
  EXPECT_TRUE(buf);
  wr->EndWrite(std::move(buf));
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
