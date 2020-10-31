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

#ifndef SRC_TRACING_IPC_POSIX_SHARED_MEMORY_H_
#define SRC_TRACING_IPC_POSIX_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/tracing/core/shared_memory.h"

namespace perfetto {

// Implements the SharedMemory and its factory for the posix-based transport.
class PosixSharedMemory : public SharedMemory {
 public:
  class Factory : public SharedMemory::Factory {
   public:
    ~Factory() override;
    std::unique_ptr<SharedMemory> CreateSharedMemory(size_t) override;
  };

  // Create a brand new SHM region.
  static std::unique_ptr<PosixSharedMemory> Create(size_t size);

  // Mmaps a file descriptor to an existing SHM region. If
  // |require_seals_if_supported| is true and the system supports
  // memfd_create(), the FD is required to be a sealed memfd with F_SEAL_SEAL,
  // F_SEAL_GROW, and F_SEAL_SHRINK seals set (otherwise, nullptr is returned).
  // May also return nullptr if mapping fails for another reason (e.g. OOM).
  static std::unique_ptr<PosixSharedMemory> AttachToFd(
      base::ScopedFile,
      bool require_seals_if_supported = true);

  ~PosixSharedMemory() override;

  int fd() const override { return fd_.get(); }

  // SharedMemory implementation.
  void* start() const override { return start_; }
  size_t size() const override { return size_; }

 private:
  static std::unique_ptr<PosixSharedMemory> MapFD(base::ScopedFile, size_t);

  PosixSharedMemory(void* start, size_t size, base::ScopedFile);
  PosixSharedMemory(const PosixSharedMemory&) = delete;
  PosixSharedMemory& operator=(const PosixSharedMemory&) = delete;

  void* const start_;
  const size_t size_;
  base::ScopedFile fd_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_IPC_POSIX_SHARED_MEMORY_H_
