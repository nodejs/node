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

#include "src/profiling/memory/shared_ring_buffer.h"

#include <atomic>
#include <type_traits>

#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/temp_file.h"
#include "src/profiling/memory/scoped_spinlock.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <linux/memfd.h>
#include <sys/syscall.h>
#endif

namespace perfetto {
namespace profiling {

namespace {

constexpr auto kMetaPageSize = base::kPageSize;
constexpr auto kAlignment = 8;  // 64 bits to use aligned memcpy().
constexpr auto kHeaderSize = kAlignment;
constexpr auto kGuardSize = base::kPageSize * 1024 * 16;  // 64 MB.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
constexpr auto kFDSeals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;
#endif

}  // namespace


SharedRingBuffer::SharedRingBuffer(CreateFlag, size_t size) {
  size_t size_with_meta = size + kMetaPageSize;
  base::ScopedFile fd;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  bool is_memfd = false;
  fd.reset(static_cast<int>(syscall(__NR_memfd_create, "heapprofd_ringbuf",
                                    MFD_CLOEXEC | MFD_ALLOW_SEALING)));
  is_memfd = !!fd;

  if (!fd) {
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
    // In-tree builds only allow mem_fd, so we can inspect the seals to verify
    // the fd is appropriately sealed.
    PERFETTO_ELOG("memfd_create() failed");
    return;
#else
    PERFETTO_DPLOG("memfd_create() failed");
#endif
  }
#endif

  if (!fd)
    fd = base::TempFile::CreateUnlinked().ReleaseFD();

  PERFETTO_CHECK(fd);
  int res = ftruncate(fd.get(), static_cast<off_t>(size_with_meta));
  PERFETTO_CHECK(res == 0);
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  if (is_memfd) {
    res = fcntl(*fd, F_ADD_SEALS, kFDSeals);
    if (res != 0) {
      PERFETTO_PLOG("Failed to seal FD.");
      return;
    }
  }
#endif
  Initialize(std::move(fd));
  if (!is_valid())
    return;

  new (meta_) MetadataPage();
}

SharedRingBuffer::~SharedRingBuffer() {
  static_assert(std::is_trivially_constructible<MetadataPage>::value,
                "MetadataPage must be trivially constructible");
  static_assert(std::is_trivially_destructible<MetadataPage>::value,
                "MetadataPage must be trivially destructible");

  if (is_valid()) {
    size_t outer_size = kMetaPageSize + size_ * 2 + kGuardSize;
    munmap(meta_, outer_size);
  }

  // This is work-around for code like the following:
  // https://android.googlesource.com/platform/libcore/+/4ecb71f94378716f88703b9f7548b5d24839262f/ojluni/src/main/native/UNIXProcess_md.c#427
  // They fork, close all fds by iterating over /proc/self/fd using opendir.
  // Unfortunately closedir calls free, which detects the fork, and then tries
  // to destruct the Client that holds this SharedRingBuffer.
  //
  // ScopedResource crashes on failure to close, so we explicitly ignore
  // failures here.
  int fd = mem_fd_.release();
  if (fd != -1)
    close(fd);
}

void SharedRingBuffer::Initialize(base::ScopedFile mem_fd) {
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  int seals = fcntl(*mem_fd, F_GET_SEALS);
  if (seals == -1) {
    PERFETTO_PLOG("Failed to get seals of FD.");
    return;
  }
  if ((seals & kFDSeals) != kFDSeals) {
    PERFETTO_ELOG("FD not properly sealed. Expected %x, got %x", kFDSeals,
                  seals);
    return;
  }
#endif

  struct stat stat_buf = {};
  int res = fstat(*mem_fd, &stat_buf);
  if (res != 0 || stat_buf.st_size == 0) {
    PERFETTO_PLOG("Could not attach to fd.");
    return;
  }
  auto size_with_meta = static_cast<size_t>(stat_buf.st_size);
  auto size = size_with_meta - kMetaPageSize;

  // |size_with_meta| must be a power of two number of pages + 1 page (for
  // metadata).
  if (size_with_meta < 2 * base::kPageSize || size % base::kPageSize ||
      (size & (size - 1))) {
    PERFETTO_ELOG("SharedRingBuffer size is invalid (%zu)", size_with_meta);
    return;
  }

  // First of all reserve the whole virtual region to fit the buffer twice
  // + metadata page + red zone at the end.
  size_t outer_size = kMetaPageSize + size * 2 + kGuardSize;
  uint8_t* region = reinterpret_cast<uint8_t*>(
      mmap(nullptr, outer_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  if (region == MAP_FAILED) {
    PERFETTO_PLOG("mmap(PROT_NONE) failed");
    return;
  }

  // Map first the whole buffer (including the initial metadata page) @ off=0.
  void* reg1 = mmap(region, size_with_meta, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED, *mem_fd, 0);

  // Then map again the buffer, skipping the metadata page. The final result is:
  // [ METADATA ] [ RING BUFFER SHMEM ] [ RING BUFFER SHMEM ]
  void* reg2 = mmap(region + size_with_meta, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_FIXED, *mem_fd,
                    /*offset=*/kMetaPageSize);

  if (reg1 != region || reg2 != region + size_with_meta) {
    PERFETTO_PLOG("mmap(MAP_SHARED) failed");
    munmap(region, outer_size);
    return;
  }
  size_ = size;
  meta_ = reinterpret_cast<MetadataPage*>(region);
  mem_ = region + kMetaPageSize;
  mem_fd_ = std::move(mem_fd);
}

SharedRingBuffer::Buffer SharedRingBuffer::BeginWrite(
    const ScopedSpinlock& spinlock,
    size_t size) {
  PERFETTO_DCHECK(spinlock.locked());
  Buffer result;

  base::Optional<PointerPositions> opt_pos = GetPointerPositions();
  if (!opt_pos) {
    meta_->stats.num_writes_corrupt++;
    errno = EBADF;
    return result;
  }
  auto pos = opt_pos.value();

  const uint64_t size_with_header =
      base::AlignUp<kAlignment>(size + kHeaderSize);

  // size_with_header < size is for catching overflow of size_with_header.
  if (PERFETTO_UNLIKELY(size_with_header < size)) {
    errno = EINVAL;
    return result;
  }

  if (size_with_header > write_avail(pos)) {
    meta_->stats.num_writes_overflow++;
    errno = EAGAIN;
    return result;
  }

  uint8_t* wr_ptr = at(pos.write_pos);

  result.size = size;
  result.data = wr_ptr + kHeaderSize;
  meta_->stats.bytes_written += size;
  meta_->stats.num_writes_succeeded++;

  // We can make this a relaxed store, as this gets picked up by the acquire
  // load in GetPointerPositions (and the release store below).
  reinterpret_cast<std::atomic<uint32_t>*>(wr_ptr)->store(
      0, std::memory_order_relaxed);

  // This needs to happen after the store above, so the reader never observes an
  // incorrect byte count. This is matched by the acquire load in
  // GetPointerPositions.
  meta_->write_pos.fetch_add(size_with_header, std::memory_order_release);
  return result;
}

void SharedRingBuffer::EndWrite(Buffer buf) {
  if (!buf)
    return;
  uint8_t* wr_ptr = buf.data - kHeaderSize;
  PERFETTO_DCHECK(reinterpret_cast<uintptr_t>(wr_ptr) % kAlignment == 0);

  // This needs to release to make sure the reader sees the payload written
  // between the BeginWrite and EndWrite calls.
  //
  // This is matched by the acquire load in BeginRead where it reads the
  // record's size.
  reinterpret_cast<std::atomic<uint32_t>*>(wr_ptr)->store(
      static_cast<uint32_t>(buf.size), std::memory_order_release);
}

SharedRingBuffer::Buffer SharedRingBuffer::BeginRead() {
  base::Optional<PointerPositions> opt_pos = GetPointerPositions();
  if (!opt_pos) {
    meta_->stats.num_reads_corrupt++;
    errno = EBADF;
    return Buffer();
  }
  auto pos = opt_pos.value();

  size_t avail_read = read_avail(pos);

  if (avail_read < kHeaderSize) {
    meta_->stats.num_reads_nodata++;
    errno = EAGAIN;
    return Buffer();  // No data
  }

  uint8_t* rd_ptr = at(pos.read_pos);
  PERFETTO_DCHECK(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
  const size_t size = reinterpret_cast<std::atomic<uint32_t>*>(rd_ptr)->load(
      std::memory_order_acquire);
  if (size == 0) {
    meta_->stats.num_reads_nodata++;
    errno = EAGAIN;
    return Buffer();
  }
  const size_t size_with_header = base::AlignUp<kAlignment>(size + kHeaderSize);

  if (size_with_header > avail_read) {
    PERFETTO_ELOG(
        "Corrupted header detected, size=%zu"
        ", read_avail=%zu, rd=%" PRIu64 ", wr=%" PRIu64,
        size, avail_read, pos.read_pos, pos.write_pos);
    meta_->stats.num_reads_corrupt++;
    errno = EBADF;
    return Buffer();
  }

  rd_ptr += kHeaderSize;
  PERFETTO_DCHECK(reinterpret_cast<uintptr_t>(rd_ptr) % kAlignment == 0);
  return Buffer(rd_ptr, size);
}

void SharedRingBuffer::EndRead(Buffer buf) {
  if (!buf)
    return;
  size_t size_with_header = base::AlignUp<kAlignment>(buf.size + kHeaderSize);
  meta_->read_pos.fetch_add(size_with_header, std::memory_order_relaxed);
  meta_->stats.num_reads_succeeded++;
}

bool SharedRingBuffer::IsCorrupt(const PointerPositions& pos) {
  if (pos.write_pos < pos.read_pos || pos.write_pos - pos.read_pos > size_ ||
      pos.write_pos % kAlignment || pos.read_pos % kAlignment) {
    PERFETTO_ELOG("Ring buffer corrupted, rd=%" PRIu64 ", wr=%" PRIu64
                  ", size=%zu",
                  pos.read_pos, pos.write_pos, size_);
    return true;
  }
  return false;
}

SharedRingBuffer::SharedRingBuffer(SharedRingBuffer&& other) noexcept {
  *this = std::move(other);
}

SharedRingBuffer& SharedRingBuffer::operator=(SharedRingBuffer&& other) {
  mem_fd_ = std::move(other.mem_fd_);
  std::tie(meta_, mem_, size_) = std::tie(other.meta_, other.mem_, other.size_);
  std::tie(other.meta_, other.mem_, other.size_) =
      std::make_tuple(nullptr, nullptr, 0);
  return *this;
}

// static
base::Optional<SharedRingBuffer> SharedRingBuffer::Create(size_t size) {
  auto buf = SharedRingBuffer(CreateFlag(), size);
  if (!buf.is_valid())
    return base::nullopt;
  return base::make_optional(std::move(buf));
}

// static
base::Optional<SharedRingBuffer> SharedRingBuffer::Attach(
    base::ScopedFile mem_fd) {
  auto buf = SharedRingBuffer(AttachFlag(), std::move(mem_fd));
  if (!buf.is_valid())
    return base::nullopt;
  return base::make_optional(std::move(buf));
}

}  // namespace profiling
}  // namespace perfetto
