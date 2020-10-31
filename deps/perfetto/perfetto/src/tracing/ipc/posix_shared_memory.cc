/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "src/tracing/ipc/posix_shared_memory.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "perfetto/base/compiler.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/temp_file.h"
#include "src/tracing/ipc/memfd.h"

namespace perfetto {

namespace {
int kFileSeals = F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL;
}  // namespace

// static
std::unique_ptr<PosixSharedMemory> PosixSharedMemory::Create(size_t size) {
  base::ScopedFile fd =
      CreateMemfd("perfetto_shmem", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  bool is_memfd = !!fd;

  // In-tree builds only allow mem_fd, so we can inspect the seals to verify the
  // fd is appropriately sealed. We'll crash in the PERFETTO_CHECK(fd) below if
  // memfd_create failed.
#if !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  if (!fd) {
    // TODO: if this fails on Android we should fall back on ashmem.
    PERFETTO_DPLOG("memfd_create() failed");
    fd = base::TempFile::CreateUnlinked().ReleaseFD();
  }
#endif

  PERFETTO_CHECK(fd);
  int res = ftruncate(fd.get(), static_cast<off_t>(size));
  PERFETTO_CHECK(res == 0);

  if (is_memfd) {
    // When memfd is supported, file seals should be, too.
    res = fcntl(*fd, F_ADD_SEALS, kFileSeals);
    PERFETTO_DCHECK(res == 0);
  }

  return MapFD(std::move(fd), size);
}

// static
std::unique_ptr<PosixSharedMemory> PosixSharedMemory::AttachToFd(
    base::ScopedFile fd,
    bool require_seals_if_supported) {
  bool requires_seals = require_seals_if_supported;

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  // In-tree kernels all support memfd.
  PERFETTO_CHECK(HasMemfdSupport());
#else
  // In out-of-tree builds, we only require seals if the kernel supports memfd.
  if (requires_seals)
    requires_seals = HasMemfdSupport();
#endif

  if (requires_seals) {
    // If the system supports memfd, we require a sealed memfd.
    int res = fcntl(*fd, F_GET_SEALS);
    if (res == -1 || (res & kFileSeals) != kFileSeals) {
      PERFETTO_PLOG("Couldn't verify file seals on shmem FD");
      return nullptr;
    }
  }

  struct stat stat_buf = {};
  int res = fstat(fd.get(), &stat_buf);
  PERFETTO_CHECK(res == 0 && stat_buf.st_size > 0);
  return MapFD(std::move(fd), static_cast<size_t>(stat_buf.st_size));
}

// static
std::unique_ptr<PosixSharedMemory> PosixSharedMemory::MapFD(base::ScopedFile fd,
                                                            size_t size) {
  PERFETTO_DCHECK(fd);
  PERFETTO_DCHECK(size > 0);
  void* start =
      mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
  PERFETTO_CHECK(start != MAP_FAILED);
  return std::unique_ptr<PosixSharedMemory>(
      new PosixSharedMemory(start, size, std::move(fd)));
}

PosixSharedMemory::PosixSharedMemory(void* start,
                                     size_t size,
                                     base::ScopedFile fd)
    : start_(start), size_(size), fd_(std::move(fd)) {}

PosixSharedMemory::~PosixSharedMemory() {
  munmap(start(), size());
}

PosixSharedMemory::Factory::~Factory() {}

std::unique_ptr<SharedMemory> PosixSharedMemory::Factory::CreateSharedMemory(
    size_t size) {
  return PosixSharedMemory::Create(size);
}

}  // namespace perfetto
