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

#include "src/profiling/memory/unwinding.h"

#include <sys/types.h>
#include <unistd.h>

#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsMips.h>
#include <unwindstack/RegsMips64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>
#include <unwindstack/Unwinder.h>
#include <unwindstack/UserArm.h>
#include <unwindstack/UserArm64.h>
#include <unwindstack/UserMips.h>
#include <unwindstack/UserMips64.h>
#include <unwindstack/UserX86.h>
#include <unwindstack/UserX86_64.h>

#include <procinfo/process_map.h>

#include "perfetto/base/logging.h"
#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/thread_task_runner.h"

#include "src/profiling/memory/wire_protocol.h"

namespace perfetto {
namespace profiling {
namespace {

constexpr base::TimeMillis kMapsReparseInterval{500};

constexpr size_t kMaxFrames = 1000;

// We assume average ~300us per unwind. If we handle up to 1000 unwinds, this
// makes sure other tasks get to be run at least every 300ms if the unwinding
// saturates this thread.
constexpr size_t kUnwindBatchSize = 1000;

#pragma GCC diagnostic push
// We do not care about deterministic destructor order.
#pragma GCC diagnostic ignored "-Wglobal-constructors"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
static std::vector<std::string> kSkipMaps{"heapprofd_client.so"};
#pragma GCC diagnostic pop

size_t GetRegsSize(unwindstack::Regs* regs) {
  if (regs->Is32Bit())
    return sizeof(uint32_t) * regs->total_regs();
  return sizeof(uint64_t) * regs->total_regs();
}

void ReadFromRawData(unwindstack::Regs* regs, void* raw_data) {
  memcpy(regs->RawData(), raw_data, GetRegsSize(regs));
}

}  // namespace

std::unique_ptr<unwindstack::Regs> CreateRegsFromRawData(
    unwindstack::ArchEnum arch,
    void* raw_data) {
  std::unique_ptr<unwindstack::Regs> ret;
  switch (arch) {
    case unwindstack::ARCH_X86:
      ret.reset(new unwindstack::RegsX86());
      break;
    case unwindstack::ARCH_X86_64:
      ret.reset(new unwindstack::RegsX86_64());
      break;
    case unwindstack::ARCH_ARM:
      ret.reset(new unwindstack::RegsArm());
      break;
    case unwindstack::ARCH_ARM64:
      ret.reset(new unwindstack::RegsArm64());
      break;
    case unwindstack::ARCH_MIPS:
      ret.reset(new unwindstack::RegsMips());
      break;
    case unwindstack::ARCH_MIPS64:
      ret.reset(new unwindstack::RegsMips64());
      break;
    case unwindstack::ARCH_UNKNOWN:
      break;
  }
  if (ret)
    ReadFromRawData(ret.get(), raw_data);
  return ret;
}

bool DoUnwind(WireMessage* msg, UnwindingMetadata* metadata, AllocRecord* out) {
  AllocMetadata* alloc_metadata = msg->alloc_header;
  std::unique_ptr<unwindstack::Regs> regs(CreateRegsFromRawData(
      alloc_metadata->arch, alloc_metadata->register_data));
  if (regs == nullptr) {
    PERFETTO_DLOG("Unable to construct unwindstack::Regs");
    unwindstack::FrameData frame_data{};
    frame_data.function_name = "ERROR READING REGISTERS";
    frame_data.map_name = "ERROR";

    out->frames.emplace_back(frame_data, "");
    out->error = true;
    return false;
  }
  uint8_t* stack = reinterpret_cast<uint8_t*>(msg->payload);
  std::shared_ptr<unwindstack::Memory> mems =
      std::make_shared<StackOverlayMemory>(metadata->fd_mem,
                                           alloc_metadata->stack_pointer, stack,
                                           msg->payload_size);

  unwindstack::Unwinder unwinder(kMaxFrames, &metadata->fd_maps, regs.get(),
                                 mems);
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  unwinder.SetJitDebug(metadata->jit_debug.get(), regs->Arch());
  unwinder.SetDexFiles(metadata->dex_files.get(), regs->Arch());
#endif
  // Suppress incorrect "variable may be uninitialized" error for if condition
  // after this loop. error_code = LastErrorCode gets run at least once.
  unwindstack::ErrorCode error_code = unwindstack::ERROR_NONE;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (attempt > 0) {
      if (metadata->last_maps_reparse_time + kMapsReparseInterval >
          base::GetWallTimeMs()) {
        PERFETTO_DLOG("Skipping reparse due to rate limit.");
        break;
      }
      PERFETTO_DLOG("Reparsing maps");
      metadata->ReparseMaps();
      metadata->last_maps_reparse_time = base::GetWallTimeMs();
      // Regs got invalidated by libuwindstack's speculative jump.
      // Reset.
      ReadFromRawData(regs.get(), alloc_metadata->register_data);
      out->reparsed_map = true;
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
      unwinder.SetJitDebug(metadata->jit_debug.get(), regs->Arch());
      unwinder.SetDexFiles(metadata->dex_files.get(), regs->Arch());
#endif
    }
    unwinder.Unwind(&kSkipMaps, /*map_suffixes_to_ignore=*/nullptr);
    error_code = unwinder.LastErrorCode();
    if (error_code != unwindstack::ERROR_INVALID_MAP)
      break;
  }
  std::vector<unwindstack::FrameData> frames = unwinder.ConsumeFrames();
  for (unwindstack::FrameData& fd : frames) {
    out->frames.emplace_back(metadata->AnnotateFrame(std::move(fd)));
  }

  if (error_code != unwindstack::ERROR_NONE) {
    PERFETTO_DLOG("Unwinding error %" PRIu8, error_code);
    unwindstack::FrameData frame_data{};
    frame_data.function_name =
        "ERROR " + StringifyLibUnwindstackError(error_code);
    frame_data.map_name = "ERROR";

    out->frames.emplace_back(std::move(frame_data), "");
    out->error = true;
  }
  return true;
}

void UnwindingWorker::OnDisconnect(base::UnixSocket* self) {
  // TODO(fmayer): Maybe try to drain shmem one last time.
  auto it = client_data_.find(self->peer_pid());
  if (it == client_data_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Disconnected unexpected socket.");
    return;
  }
  ClientData& client_data = it->second;
  SharedRingBuffer& shmem = client_data.shmem;

  SharedRingBuffer::Stats stats = {};
  {
    auto lock = shmem.AcquireLock(ScopedSpinlock::Mode::Try);
    if (lock.locked())
      stats = shmem.GetStats(lock);
    else
      PERFETTO_ELOG("Failed to log shmem to get stats.");
  }
  DataSourceInstanceID ds_id = client_data.data_source_instance_id;
  pid_t peer_pid = self->peer_pid();
  client_data_.erase(it);
  // The erase invalidates the self pointer.
  self = nullptr;
  delegate_->PostSocketDisconnected(ds_id, peer_pid, stats);
}

void UnwindingWorker::OnDataAvailable(base::UnixSocket* self) {
  // Drain buffer to clear the notification.
  char recv_buf[kUnwindBatchSize];
  self->Receive(recv_buf, sizeof(recv_buf));
  HandleUnwindBatch(self->peer_pid());
}

void UnwindingWorker::HandleUnwindBatch(pid_t peer_pid) {
  auto it = client_data_.find(peer_pid);
  if (it == client_data_.end()) {
    // This can happen if the client disconnected before the buffer was fully
    // handled.
    PERFETTO_DLOG("Unexpected data.");
    return;
  }

  ClientData& client_data = it->second;
  SharedRingBuffer& shmem = client_data.shmem;
  SharedRingBuffer::Buffer buf;

  size_t i;
  bool repost_task = false;
  for (i = 0; i < kUnwindBatchSize; ++i) {
    uint64_t reparses_before = client_data.metadata.reparses;
    buf = shmem.BeginRead();
    if (!buf)
      break;
    HandleBuffer(buf, &client_data.metadata,
                 client_data.data_source_instance_id,
                 client_data.sock->peer_pid(), delegate_);
    shmem.EndRead(std::move(buf));
    // Reparsing takes time, so process the rest in a new batch to avoid timing
    // out.
    if (reparses_before < client_data.metadata.reparses) {
      repost_task = true;
      break;
    }
  }

  // Always repost if we have gone through the whole batch.
  if (i == kUnwindBatchSize)
    repost_task = true;

  if (repost_task) {
    thread_task_runner_.get()->PostTask(
        [this, peer_pid] { HandleUnwindBatch(peer_pid); });
  }
}

// static
void UnwindingWorker::HandleBuffer(const SharedRingBuffer::Buffer& buf,
                                   UnwindingMetadata* unwinding_metadata,
                                   DataSourceInstanceID data_source_instance_id,
                                   pid_t peer_pid,
                                   Delegate* delegate) {
  WireMessage msg;
  // TODO(fmayer): standardise on char* or uint8_t*.
  // char* has stronger guarantees regarding aliasing.
  // see https://timsong-cpp.github.io/cppwp/n3337/basic.lval#10.8
  if (!ReceiveWireMessage(reinterpret_cast<char*>(buf.data), buf.size, &msg)) {
    PERFETTO_DFATAL_OR_ELOG("Failed to receive wire message.");
    return;
  }

  if (msg.record_type == RecordType::Malloc) {
    AllocRecord rec;
    rec.alloc_metadata = *msg.alloc_header;
    rec.pid = peer_pid;
    rec.data_source_instance_id = data_source_instance_id;
    auto start_time_us = base::GetWallTimeNs() / 1000;
    DoUnwind(&msg, unwinding_metadata, &rec);
    rec.unwinding_time_us = static_cast<uint64_t>(
        ((base::GetWallTimeNs() / 1000) - start_time_us).count());
    delegate->PostAllocRecord(std::move(rec));
  } else if (msg.record_type == RecordType::Free) {
    FreeRecord rec;
    rec.pid = peer_pid;
    rec.data_source_instance_id = data_source_instance_id;
    // We need to copy this, so we can return the memory to the shmem buffer.
    memcpy(&rec.free_batch, msg.free_header, sizeof(*msg.free_header));
    delegate->PostFreeRecord(std::move(rec));
  } else {
    PERFETTO_DFATAL_OR_ELOG("Invalid record type.");
  }
}

void UnwindingWorker::PostHandoffSocket(HandoffData handoff_data) {
  // Even with C++14, this cannot be moved, as std::function has to be
  // copyable, which HandoffData is not.
  HandoffData* raw_data = new HandoffData(std::move(handoff_data));
  // We do not need to use a WeakPtr here because the task runner will not
  // outlive its UnwindingWorker.
  thread_task_runner_.get()->PostTask([this, raw_data] {
    HandoffData data = std::move(*raw_data);
    delete raw_data;
    HandleHandoffSocket(std::move(data));
  });
}

void UnwindingWorker::HandleHandoffSocket(HandoffData handoff_data) {
  auto sock = base::UnixSocket::AdoptConnected(
      handoff_data.sock.ReleaseFd(), this, this->thread_task_runner_.get(),
      base::SockFamily::kUnix, base::SockType::kStream);
  pid_t peer_pid = sock->peer_pid();

  UnwindingMetadata metadata(std::move(handoff_data.maps_fd),
                             std::move(handoff_data.mem_fd));
  ClientData client_data{
      handoff_data.data_source_instance_id,
      std::move(sock),
      std::move(metadata),
      std::move(handoff_data.shmem),
      std::move(handoff_data.client_config),
  };
  client_data_.emplace(peer_pid, std::move(client_data));
}

void UnwindingWorker::PostDisconnectSocket(pid_t pid) {
  // We do not need to use a WeakPtr here because the task runner will not
  // outlive its UnwindingWorker.
  thread_task_runner_.get()->PostTask(
      [this, pid] { HandleDisconnectSocket(pid); });
}

void UnwindingWorker::HandleDisconnectSocket(pid_t pid) {
  auto it = client_data_.find(pid);
  if (it == client_data_.end()) {
    PERFETTO_DFATAL_OR_ELOG("Trying to disconnect unknown socket.");
    return;
  }
  ClientData& client_data = it->second;
  // Shutdown and call OnDisconnect handler.
  client_data.sock->Shutdown(/* notify= */ true);
}

UnwindingWorker::Delegate::~Delegate() = default;

}  // namespace profiling
}  // namespace perfetto
