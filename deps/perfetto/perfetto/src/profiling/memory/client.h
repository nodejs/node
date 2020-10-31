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

#ifndef SRC_PROFILING_MEMORY_CLIENT_H_
#define SRC_PROFILING_MEMORY_CLIENT_H_

#include <stddef.h>
#include <sys/types.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "perfetto/base/compiler.h"
#include "perfetto/ext/base/unix_socket.h"
#include "src/profiling/memory/sampler.h"
#include "src/profiling/memory/shared_ring_buffer.h"
#include "src/profiling/memory/unhooked_allocator.h"
#include "src/profiling/memory/wire_protocol.h"

namespace perfetto {
namespace profiling {

const char* GetThreadStackBase();

constexpr uint32_t kClientSockTimeoutMs = 1000;

// Profiling client, used to sample and record the malloc/free family of calls,
// and communicate the necessary state to a separate profiling daemon process.
//
// Created and owned by the malloc hooks.
//
// Methods of this class are thread-safe unless otherwise stated, in which case
// the caller needs to synchronize calls behind a mutex or similar.
//
// Implementation warning: this class should not use any heap, as otherwise its
// destruction would enter the possibly-hooked |free|, which can reference the
// Client itself. If avoiding the heap is not possible, then look at using
// UnhookedAllocator.
class Client {
 public:
  // Returns a client that is ready for sampling allocations, using the given
  // socket (which should already be connected to heapprofd).
  //
  // Returns a shared_ptr since that is how the client will ultimately be used,
  // and to take advantage of std::allocate_shared putting the object & the
  // control block in one block of memory.
  static std::shared_ptr<Client> CreateAndHandshake(
      base::UnixSocketRaw sock,
      UnhookedAllocator<Client> unhooked_allocator);

  static base::Optional<base::UnixSocketRaw> ConnectToHeapprofd(
      const std::string& sock_name);

  bool RecordMalloc(uint64_t sample_size,
                    uint64_t alloc_size,
                    uint64_t alloc_address) PERFETTO_WARN_UNUSED_RESULT;

  // Add address to buffer of deallocations. Flushes the buffer if necessary.
  bool RecordFree(uint64_t alloc_address) PERFETTO_WARN_UNUSED_RESULT;

  // Returns the number of bytes to assign to an allocation with the given
  // |alloc_size|, based on the current sampling rate. A return value of zero
  // means that the allocation should not be recorded. Not idempotent, each
  // invocation mutates the sampler state.
  //
  // Not thread-safe.
  size_t GetSampleSizeLocked(size_t alloc_size) {
    return sampler_.SampleSize(alloc_size);
  }

  // Public for std::allocate_shared. Use CreateAndHandshake() to create
  // instances instead.
  Client(base::UnixSocketRaw sock,
         ClientConfiguration client_config,
         SharedRingBuffer shmem,
         Sampler sampler,
         pid_t pid_at_creation,
         const char* main_thread_stack_base);

  ~Client();

  ClientConfiguration client_config_for_testing() { return client_config_; }

 private:
  const char* GetStackBase();
  // Flush the contents of free_batch_. Must hold free_batch_lock_.
  bool FlushFreesLocked() PERFETTO_WARN_UNUSED_RESULT;
  bool SendControlSocketByte() PERFETTO_WARN_UNUSED_RESULT;
  bool SendWireMessageWithRetriesIfBlocking(const WireMessage&)
      PERFETTO_WARN_UNUSED_RESULT;

  bool IsPostFork();

  // This is only valid for non-blocking sockets. This is when
  // client_config_.block_client is true.
  bool IsConnected();

  ClientConfiguration client_config_;
  uint64_t max_shmem_tries_;
  // sampler_ operations are not thread-safe.
  Sampler sampler_;
  base::UnixSocketRaw sock_;

  // Protected by free_batch_lock_.
  FreeBatch free_batch_;
  std::timed_mutex free_batch_lock_;

  const char* main_thread_stack_base_{nullptr};
  std::atomic<uint64_t> sequence_number_{0};
  SharedRingBuffer shmem_;

  // Used to detect (during the slow path) the situation where the process has
  // forked during profiling, and is performing malloc operations in the child.
  // In this scenario, we want to stop profiling in the child, as otherwise
  // it'll proceed to write to the same shared buffer & control socket (with
  // duplicate sequence ids).
  const pid_t pid_at_creation_;
  bool detected_fork_ = false;
  bool postfork_return_value_ = false;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_CLIENT_H_
