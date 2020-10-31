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

#ifndef SRC_TRACING_CORE_SHARED_MEMORY_ARBITER_IMPL_H_
#define SRC_TRACING_CORE_SHARED_MEMORY_ARBITER_IMPL_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "perfetto/ext/base/weak_ptr.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/shared_memory_abi.h"
#include "perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "perfetto/tracing/core/forward_decls.h"
#include "src/tracing/core/id_allocator.h"

namespace perfetto {

class PatchList;
class TraceWriter;
class TraceWriterImpl;

namespace base {
class TaskRunner;
}  // namespace base

// This class handles the shared memory buffer on the producer side. It is used
// to obtain thread-local chunks and to partition pages from several threads.
// There is one arbiter instance per Producer.
// This class is thread-safe and uses locks to do so. Data sources are supposed
// to interact with this sporadically, only when they run out of space on their
// current thread-local chunk.
//
// When the arbiter is created using CreateUnboundInstance(), the following
// state transitions are possible:
//
//   [ !fully_bound_, !endpoint_, 0 unbound buffer reservations ]
//       |     |
//       |     | CreateStartupTraceWriter(buf)
//       |     |  buffer reservations += buf
//       |     |
//       |     |             ----
//       |     |            |    | CreateStartupTraceWriter(buf)
//       |     |            |    |  buffer reservations += buf
//       |     V            |    V
//       |   [ !fully_bound_, !endpoint_, >=1 unbound buffer reservations ]
//       |                                                |
//       |                       BindToProducerEndpoint() |
//       |                                                |
//       | BindToProducerEndpoint()                       |
//       |                                                V
//       |   [ !fully_bound_, endpoint_, >=1 unbound buffer reservations ]
//       |   A    |    A                               |     A
//       |   |    |    |                               |     |
//       |   |     ----                                |     |
//       |   |    CreateStartupTraceWriter(buf)        |     |
//       |   |     buffer reservations += buf          |     |
//       |   |                                         |     |
//       |   | CreateStartupTraceWriter(buf)           |     |
//       |   |  where buf is not yet bound             |     |
//       |   |  buffer reservations += buf             |     | (yes)
//       |   |                                         |     |
//       |   |        BindStartupTargetBuffer(buf, id) |-----
//       |   |           buffer reservations -= buf    | reservations > 0?
//       |   |                                         |
//       |   |                                         | (no)
//       V   |                                         V
//       [ fully_bound_, endpoint_, 0 unbound buffer reservations ]
//          |    A
//          |    | CreateStartupTraceWriter(buf)
//          |    |  where buf is already bound
//           ----
class SharedMemoryArbiterImpl : public SharedMemoryArbiter {
 public:
  // See SharedMemoryArbiter::CreateInstance(). |start|, |size| define the
  // boundaries of the shared memory buffer. ProducerEndpoint and TaskRunner may
  // be |nullptr| if created unbound, see
  // SharedMemoryArbiter::CreateUnboundInstance().
  SharedMemoryArbiterImpl(void* start,
                          size_t size,
                          size_t page_size,
                          TracingService::ProducerEndpoint*,
                          base::TaskRunner*);

  // Returns a new Chunk to write tracing data. Depending on the provided
  // BufferExhaustedPolicy, this may return an invalid chunk if no valid free
  // chunk could be found in the SMB.
  SharedMemoryABI::Chunk GetNewChunk(const SharedMemoryABI::ChunkHeader&,
                                     BufferExhaustedPolicy,
                                     size_t size_hint = 0);

  // Puts back a Chunk that has been completed and sends a request to the
  // service to move it to the central tracing buffer. |target_buffer| is the
  // absolute trace buffer ID where the service should move the chunk onto (the
  // producer is just to copy back the same number received in the
  // DataSourceConfig upon the StartDataSource() reques).
  // PatchList is a pointer to the list of patches for previous chunks. The
  // first patched entries will be removed from the patched list and sent over
  // to the service in the same CommitData() IPC request.
  void ReturnCompletedChunk(SharedMemoryABI::Chunk,
                            MaybeUnboundBufferID target_buffer,
                            PatchList*);

  // Send a request to the service to apply completed patches from |patch_list|.
  // |writer_id| is the ID of the TraceWriter that calls this method,
  // |target_buffer| is the global trace buffer ID of its target buffer.
  void SendPatches(WriterID writer_id,
                   MaybeUnboundBufferID target_buffer,
                   PatchList* patch_list);

  // Forces a synchronous commit of the completed packets without waiting for
  // the next task.
  void FlushPendingCommitDataRequests(std::function<void()> callback = {});

  SharedMemoryABI* shmem_abi_for_testing() { return &shmem_abi_; }

  static void set_default_layout_for_testing(SharedMemoryABI::PageLayout l) {
    default_page_layout = l;
  }

  // SharedMemoryArbiter implementation.
  // See include/perfetto/tracing/core/shared_memory_arbiter.h for comments.
  std::unique_ptr<TraceWriter> CreateTraceWriter(
      BufferID target_buffer,
      BufferExhaustedPolicy = BufferExhaustedPolicy::kDefault) override;
  std::unique_ptr<TraceWriter> CreateStartupTraceWriter(
      uint16_t target_buffer_reservation_id) override;
  void BindToProducerEndpoint(TracingService::ProducerEndpoint*,
                              base::TaskRunner*) override;
  void BindStartupTargetBuffer(uint16_t target_buffer_reservation_id,
                               BufferID target_buffer_id) override;
  void AbortStartupTracingForReservation(
      uint16_t target_buffer_reservation_id) override;
  void NotifyFlushComplete(FlushRequestID) override;

  base::TaskRunner* task_runner() const { return task_runner_; }
  size_t page_size() const { return shmem_abi_.page_size(); }
  size_t num_pages() const { return shmem_abi_.num_pages(); }

  base::WeakPtr<SharedMemoryArbiterImpl> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class TraceWriterImpl;
  friend class StartupTraceWriterTest;
  friend class SharedMemoryArbiterImplTest;

  struct TargetBufferReservation {
    bool resolved = false;
    BufferID target_buffer = kInvalidBufferId;
  };

  // Placeholder for the actual target buffer ID of a startup target buffer
  // reservation ID in |target_buffer_reservations_|.
  static constexpr BufferID kInvalidBufferId = 0;

  static SharedMemoryABI::PageLayout default_page_layout;

  SharedMemoryArbiterImpl(const SharedMemoryArbiterImpl&) = delete;
  SharedMemoryArbiterImpl& operator=(const SharedMemoryArbiterImpl&) = delete;

  void UpdateCommitDataRequest(SharedMemoryABI::Chunk chunk,
                               WriterID writer_id,
                               MaybeUnboundBufferID target_buffer,
                               PatchList* patch_list);

  std::unique_ptr<TraceWriter> CreateTraceWriterInternal(
      MaybeUnboundBufferID target_buffer,
      BufferExhaustedPolicy);

  // Called by the TraceWriter destructor.
  void ReleaseWriterID(WriterID);

  void BindStartupTargetBufferImpl(std::unique_lock<std::mutex> scoped_lock,
                                   uint16_t target_buffer_reservation_id,
                                   BufferID target_buffer_id);

  // If any flush callbacks were queued up while the arbiter or any target
  // buffer reservation was unbound, this wraps the pending callbacks into a new
  // std::function and returns it. Otherwise returns an invalid std::function.
  std::function<void()> TakePendingFlushCallbacksLocked();

  // Replace occurrences of target buffer reservation IDs in |commit_data_req_|
  // with their respective actual BufferIDs if they were already bound. Returns
  // true iff all occurrences were replaced.
  bool ReplaceCommitPlaceholderBufferIdsLocked();

  // Update and return |fully_bound_| based on the arbiter's |pending_writers_|
  // state.
  bool UpdateFullyBoundLocked();

  const bool initially_bound_;
  // Only accessed on |task_runner_| after the producer endpoint was bound.
  TracingService::ProducerEndpoint* producer_endpoint_ = nullptr;

  // --- Begin lock-protected members ---

  std::mutex lock_;

  base::TaskRunner* task_runner_ = nullptr;
  SharedMemoryABI shmem_abi_;
  size_t page_idx_ = 0;
  std::unique_ptr<CommitDataRequest> commit_data_req_;
  size_t bytes_pending_commit_ = 0;  // SUM(chunk.size() : commit_data_req_).
  IdAllocator<WriterID> active_writer_ids_;

  // Whether the arbiter itself and all startup target buffer reservations are
  // bound. Note that this can become false again later if a new target buffer
  // reservation is created by calling CreateStartupTraceWriter() with a new
  // reservation id.
  bool fully_bound_;

  // IDs of writers and their assigned target buffers that should be registered
  // with the service after the arbiter and/or their startup target buffer is
  // bound.
  std::map<WriterID, MaybeUnboundBufferID> pending_writers_;

  // Callbacks for flush requests issued while the arbiter or a target buffer
  // reservation was unbound.
  std::vector<std::function<void()>> pending_flush_callbacks_;

  // Stores target buffer reservations for writers created via
  // CreateStartupTraceWriter(). A bound reservation sets
  // TargetBufferReservation::resolved to true and is associated with the actual
  // BufferID supplied in BindStartupTargetBuffer().
  //
  // TODO(eseckler): Clean up entries from this map. This would probably require
  // a method in SharedMemoryArbiter that allows a producer to invalidate a
  // reservation ID.
  std::map<MaybeUnboundBufferID, TargetBufferReservation>
      target_buffer_reservations_;

  // --- End lock-protected members ---

  // Keep at the end.
  base::WeakPtrFactory<SharedMemoryArbiterImpl> weak_ptr_factory_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_CORE_SHARED_MEMORY_ARBITER_IMPL_H_
