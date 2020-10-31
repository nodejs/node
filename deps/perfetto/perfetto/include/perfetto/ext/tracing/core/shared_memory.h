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

#ifndef INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_H_
#define INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_H_

#include <stddef.h>

#include <memory>

#include "perfetto/base/export.h"

namespace perfetto {

// An abstract interface that models the shared memory region shared between
// Service and Producer. The concrete implementation of this is up to the
// transport layer. This can be as simple as a malloc()-ed buffer, if both
// Producer and Service are hosted in the same process, or some posix shared
// memory for the out-of-process case (see src/unix_rpc).
// Both this class and the Factory are subclassed by the transport layer, which
// will attach platform specific fields to it (e.g., a unix file descriptor).
class PERFETTO_EXPORT SharedMemory {
 public:
  class PERFETTO_EXPORT Factory {
   public:
    virtual ~Factory();
    virtual std::unique_ptr<SharedMemory> CreateSharedMemory(size_t) = 0;
  };

  // The transport layer is expected to tear down the resource associated to
  // this object region when destroyed.
  virtual ~SharedMemory();

  virtual void* start() const = 0;
  virtual size_t size() const = 0;
  virtual int fd() const = 0;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_TRACING_CORE_SHARED_MEMORY_H_
