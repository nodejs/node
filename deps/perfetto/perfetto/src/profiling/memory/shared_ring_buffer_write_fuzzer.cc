/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <stddef.h>
#include <stdint.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/temp_file.h"
#include "src/profiling/memory/shared_ring_buffer.h"

namespace perfetto {
namespace profiling {
namespace {

struct FuzzingInputHeader {
  size_t write_size;
  SharedRingBuffer::MetadataPage metadata_page;
};

size_t RoundToPow2(size_t v) {
  uint64_t x = static_cast<uint64_t>(v);
  if (x < 2)
    return 2;

  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  x++;
  return static_cast<size_t>(x);
}

int FuzzRingBufferWrite(const uint8_t* data, size_t size) {
  if (size <= sizeof(FuzzingInputHeader))
    return 0;

  auto fd = base::TempFile::CreateUnlinked().ReleaseFD();
  PERFETTO_CHECK(fd);

  // Prefill shared buffer with fuzzer input, then attempt to write.
  // TODO(fmayer): Do we actually need to fill the buffer with payload, or
  // should we only fuzz the metadata?
  size_t payload_size = size - sizeof(FuzzingInputHeader);
  const uint8_t* payload = data + sizeof(FuzzingInputHeader);
  size_t payload_size_pages =
      (payload_size + base::kPageSize - 1) / base::kPageSize;
  // Upsize test buffer to be 2^n data pages (precondition of the impl) + 1 page
  // for the metadata.
  size_t total_size_pages = 1 + RoundToPow2(payload_size_pages);

  // Clear spinlock field, as otherwise we will fail acquiring the lock below.
  FuzzingInputHeader header = {};
  memcpy(&header, data, sizeof(header));
  SharedRingBuffer::MetadataPage& metadata_page = header.metadata_page;
  metadata_page.spinlock = 0;

  PERFETTO_CHECK(ftruncate(*fd, static_cast<off_t>(total_size_pages *
                                                   base::kPageSize)) == 0);
  PERFETTO_CHECK(base::WriteAll(*fd, &metadata_page, sizeof(metadata_page)) !=
                 -1);
  PERFETTO_CHECK(lseek(*fd, base::kPageSize, SEEK_SET) != -1);
  PERFETTO_CHECK(base::WriteAll(*fd, payload, payload_size) != -1);

  auto buf = SharedRingBuffer::Attach(std::move(fd));
  PERFETTO_CHECK(!!buf);

  SharedRingBuffer::Buffer write_buf;
  {
    auto lock = buf->AcquireLock(ScopedSpinlock::Mode::Try);
    PERFETTO_CHECK(lock.locked());
    write_buf = buf->BeginWrite(lock, header.write_size);
  }
  if (!write_buf)
    return 0;

  memset(write_buf.data, '\0', write_buf.size);
  buf->EndWrite(std::move(write_buf));
  return 0;
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return perfetto::profiling::FuzzRingBufferWrite(data, size);
}
