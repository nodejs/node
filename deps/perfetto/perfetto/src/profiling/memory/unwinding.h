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

#ifndef SRC_PROFILING_MEMORY_UNWINDING_H_
#define SRC_PROFILING_MEMORY_UNWINDING_H_

#include <unwindstack/Regs.h>

#include "perfetto/base/time.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_task_runner.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "src/profiling/common/unwind_support.h"
#include "src/profiling/memory/bookkeeping.h"
#include "src/profiling/memory/unwound_messages.h"
#include "src/profiling/memory/wire_protocol.h"

namespace perfetto {
namespace profiling {

std::unique_ptr<unwindstack::Regs> CreateRegsFromRawData(
    unwindstack::ArchEnum arch,
    void* raw_data);

bool DoUnwind(WireMessage*, UnwindingMetadata* metadata, AllocRecord* out);

class UnwindingWorker : public base::UnixSocket::EventListener {
 public:
  class Delegate {
   public:
    virtual void PostAllocRecord(AllocRecord) = 0;
    virtual void PostFreeRecord(FreeRecord) = 0;
    virtual void PostSocketDisconnected(DataSourceInstanceID,
                                        pid_t pid,
                                        SharedRingBuffer::Stats stats) = 0;
    virtual ~Delegate();
  };

  struct HandoffData {
    DataSourceInstanceID data_source_instance_id;
    base::UnixSocketRaw sock;
    base::ScopedFile maps_fd;
    base::ScopedFile mem_fd;
    SharedRingBuffer shmem;
    ClientConfiguration client_config;
  };

  UnwindingWorker(Delegate* delegate, base::ThreadTaskRunner thread_task_runner)
      : delegate_(delegate),
        thread_task_runner_(std::move(thread_task_runner)) {}

  // Public API safe to call from other threads.
  void PostDisconnectSocket(pid_t pid);
  void PostHandoffSocket(HandoffData);

  // Implementation of UnixSocket::EventListener.
  // Do not call explicitly.
  void OnDisconnect(base::UnixSocket* self) override;
  void OnNewIncomingConnection(base::UnixSocket*,
                               std::unique_ptr<base::UnixSocket>) override {
    PERFETTO_DFATAL_OR_ELOG("This should not happen.");
  }
  void OnDataAvailable(base::UnixSocket* self) override;

 public:
  // static and public for testing/fuzzing
  static void HandleBuffer(const SharedRingBuffer::Buffer& buf,
                           UnwindingMetadata* unwinding_metadata,
                           DataSourceInstanceID data_source_instance_id,
                           pid_t peer_pid,
                           Delegate* delegate);

 private:
  void HandleHandoffSocket(HandoffData data);
  void HandleDisconnectSocket(pid_t pid);

  void HandleUnwindBatch(pid_t);

  struct ClientData {
    DataSourceInstanceID data_source_instance_id;
    std::unique_ptr<base::UnixSocket> sock;
    UnwindingMetadata metadata;
    SharedRingBuffer shmem;
    ClientConfiguration client_config;
  };

  std::map<pid_t, ClientData> client_data_;
  Delegate* delegate_;

  // Task runner with a dedicated thread. Keep last as instances this class are
  // currently (incorrectly) being destroyed on the main thread, instead of the
  // task thread. By destroying this task runner first, we ensure that the
  // UnwindingWorker is not active while the rest of its state is being
  // destroyed. Additionally this ensures that the destructing thread sees a
  // consistent view of the memory due to the ThreadTaskRunner's destructor
  // joining a thread.
  //
  // Additionally, keep the destructor defaulted, as its body would still race
  // against an active task thread.
  //
  // TODO(rsavitski): make the task thread own the object's lifetime (likely by
  // refactoring base::ThreadTaskRunner).
  base::ThreadTaskRunner thread_task_runner_;
};

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_UNWINDING_H_
