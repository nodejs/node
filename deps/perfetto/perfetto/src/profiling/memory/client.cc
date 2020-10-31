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

#include "src/profiling/memory/client.h"

#include <inttypes.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineMips.h>
#include <unwindstack/MachineMips64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsGetLocal.h>

#include <algorithm>
#include <atomic>
#include <new>

#include "perfetto/base/logging.h"
#include "perfetto/base/thread_utils.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_socket.h"
#include "perfetto/ext/base/utils.h"
#include "src/profiling/memory/sampler.h"
#include "src/profiling/memory/scoped_spinlock.h"
#include "src/profiling/memory/wire_protocol.h"

namespace perfetto {
namespace profiling {
namespace {

const char kSingleByte[1] = {'x'};
constexpr std::chrono::seconds kLockTimeout{1};
constexpr auto kResendBackoffUs = 100;

inline bool IsMainThread() {
  return getpid() == base::GetThreadId();
}

// The implementation of pthread_getattr_np for the main thread uses malloc,
// so we cannot use it in GetStackBase, which we use inside of RecordMalloc
// (which is called from malloc). We would re-enter malloc if we used it.
//
// This is why we find the stack base for the main-thread when constructing
// the client and remember it.
char* FindMainThreadStack() {
  base::ScopedFstream maps(fopen("/proc/self/maps", "r"));
  if (!maps) {
    return nullptr;
  }
  while (!feof(*maps)) {
    char line[1024];
    char* data = fgets(line, sizeof(line), *maps);
    if (data != nullptr && strstr(data, "[stack]")) {
      char* sep = strstr(data, "-");
      if (sep == nullptr)
        continue;
      sep++;
      return reinterpret_cast<char*>(strtoll(sep, nullptr, 16));
    }
  }
  return nullptr;
}

int UnsetDumpable(int) {
  prctl(PR_SET_DUMPABLE, 0);
  return 0;
}

constexpr uint64_t kInfiniteTries = 0;

uint64_t GetMaxTries(const ClientConfiguration& client_config) {
  if (!client_config.block_client)
    return 1u;
  if (client_config.block_client_timeout_us == 0)
    return kInfiniteTries;
  return std::min<uint64_t>(
      1ul, client_config.block_client_timeout_us / kResendBackoffUs);
}

}  // namespace

const char* GetThreadStackBase() {
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) != 0)
    return nullptr;
  base::ScopedResource<pthread_attr_t*, pthread_attr_destroy, nullptr> cleanup(
      &attr);

  char* stackaddr;
  size_t stacksize;
  if (pthread_attr_getstack(&attr, reinterpret_cast<void**>(&stackaddr),
                            &stacksize) != 0)
    return nullptr;
  return stackaddr + stacksize;
}

// static
base::Optional<base::UnixSocketRaw> Client::ConnectToHeapprofd(
    const std::string& sock_name) {
  auto sock = base::UnixSocketRaw::CreateMayFail(base::SockFamily::kUnix,
                                                 base::SockType::kStream);
  if (!sock || !sock.Connect(sock_name)) {
    PERFETTO_PLOG("Failed to connect to %s", sock_name.c_str());
    return base::nullopt;
  }
  if (!sock.SetTxTimeout(kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set send timeout for %s", sock_name.c_str());
    return base::nullopt;
  }
  if (!sock.SetRxTimeout(kClientSockTimeoutMs)) {
    PERFETTO_PLOG("Failed to set receive timeout for %s", sock_name.c_str());
    return base::nullopt;
  }
  return std::move(sock);
}

// static
std::shared_ptr<Client> Client::CreateAndHandshake(
    base::UnixSocketRaw sock,
    UnhookedAllocator<Client> unhooked_allocator) {
  if (!sock) {
    PERFETTO_DFATAL_OR_ELOG("Socket not connected.");
    return nullptr;
  }

  PERFETTO_DCHECK(sock.IsBlocking());

  // We might be running in a process that is not dumpable (such as app
  // processes on user builds), in which case the /proc/self/mem will be chown'd
  // to root:root, and will not be accessible even to the process itself (see
  // man 5 proc). In such situations, temporarily mark the process dumpable to
  // be able to open the files, unsetting dumpability immediately afterwards.
  int orig_dumpable = prctl(PR_GET_DUMPABLE);

  enum { kNop, kDoUnset };
  base::ScopedResource<int, UnsetDumpable, kNop, false> unset_dumpable(kNop);
  if (orig_dumpable == 0) {
    unset_dumpable.reset(kDoUnset);
    prctl(PR_SET_DUMPABLE, 1);
  }

  size_t num_send_fds = kHandshakeSize;

  base::ScopedFile maps(base::OpenFile("/proc/self/maps", O_RDONLY));
  if (!maps) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc/self/maps");
    return nullptr;
  }
  base::ScopedFile mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  if (!mem) {
    PERFETTO_DFATAL_OR_ELOG("Failed to open /proc/self/mem");
    return nullptr;
  }

  base::ScopedFile page_idle(base::OpenFile("/proc/self/page_idle", O_RDWR));
  if (!page_idle) {
    PERFETTO_DLOG("Failed to open /proc/self/page_idle. Continuing.");
    num_send_fds = kHandshakeSize - 1;
  }

  // Restore original dumpability value if we overrode it.
  unset_dumpable.reset();

  int fds[kHandshakeSize];
  fds[kHandshakeMaps] = *maps;
  fds[kHandshakeMem] = *mem;
  fds[kHandshakePageIdle] = *page_idle;

  // Send an empty record to transfer fds for /proc/self/maps and
  // /proc/self/mem.
  if (sock.Send(kSingleByte, sizeof(kSingleByte), fds, num_send_fds) !=
      sizeof(kSingleByte)) {
    PERFETTO_DFATAL_OR_ELOG("Failed to send file descriptors.");
    return nullptr;
  }

  ClientConfiguration client_config;
  base::ScopedFile shmem_fd;
  size_t recv = 0;
  while (recv < sizeof(client_config)) {
    size_t num_fds = 0;
    base::ScopedFile* fd = nullptr;
    if (!shmem_fd) {
      num_fds = 1;
      fd = &shmem_fd;
    }
    ssize_t rd = sock.Receive(reinterpret_cast<char*>(&client_config) + recv,
                              sizeof(client_config) - recv, fd, num_fds);
    if (rd == -1) {
      PERFETTO_PLOG("Failed to receive ClientConfiguration.");
      return nullptr;
    }
    if (rd == 0) {
      PERFETTO_LOG("Server disconnected while sending ClientConfiguration.");
      return nullptr;
    }
    recv += static_cast<size_t>(rd);
  }

  if (!shmem_fd) {
    PERFETTO_DFATAL_OR_ELOG("Did not receive shmem fd.");
    return nullptr;
  }

  auto shmem = SharedRingBuffer::Attach(std::move(shmem_fd));
  if (!shmem || !shmem->is_valid()) {
    PERFETTO_DFATAL_OR_ELOG("Failed to attach to shmem.");
    return nullptr;
  }

  PERFETTO_DCHECK(client_config.interval >= 1);
  sock.SetBlocking(false);
  Sampler sampler{client_config.interval};
  // note: the shared_ptr will retain a copy of the unhooked_allocator
  return std::allocate_shared<Client>(unhooked_allocator, std::move(sock),
                                      client_config, std::move(shmem.value()),
                                      std::move(sampler), getpid(),
                                      FindMainThreadStack());
}

Client::Client(base::UnixSocketRaw sock,
               ClientConfiguration client_config,
               SharedRingBuffer shmem,
               Sampler sampler,
               pid_t pid_at_creation,
               const char* main_thread_stack_base)
    : client_config_(client_config),
      max_shmem_tries_(GetMaxTries(client_config_)),
      sampler_(std::move(sampler)),
      sock_(std::move(sock)),
      main_thread_stack_base_(main_thread_stack_base),
      shmem_(std::move(shmem)),
      pid_at_creation_(pid_at_creation) {}

Client::~Client() {
  // This is work-around for code like the following:
  // https://android.googlesource.com/platform/libcore/+/4ecb71f94378716f88703b9f7548b5d24839262f/ojluni/src/main/native/UNIXProcess_md.c#427
  // They fork, close all fds by iterating over /proc/self/fd using opendir.
  // Unfortunately closedir calls free, which detects the fork, and then tries
  // to destruct this Client.
  //
  // ScopedResource crashes on failure to close, so we explicitly ignore
  // failures here.
  int fd = sock_.ReleaseFd().release();
  if (fd != -1)
    close(fd);
}

const char* Client::GetStackBase() {
  if (IsMainThread()) {
    if (!main_thread_stack_base_)
      // Because pthread_attr_getstack reads and parses /proc/self/maps and
      // /proc/self/stat, we have to cache the result here.
      main_thread_stack_base_ = GetThreadStackBase();
    return main_thread_stack_base_;
  }
  return GetThreadStackBase();
}

bool Client::IsPostFork() {
  if (PERFETTO_UNLIKELY(getpid() != pid_at_creation_)) {
    // Only print the message once, even if we do not shut down the client.
    if (!detected_fork_) {
      detected_fork_ = true;
      const char* vfork_detected = "";

      // We use the fact that vfork does not update Bionic's TID cache, so
      // we will have a mismatch between the actual TID (from the syscall)
      // and the cached one.
      //
      // What we really want to check is if we are sharing virtual memory space
      // with the original process. This would be
      // syscall(__NR_kcmp, syscall(__NR_getpid), pid_at_creation_,
      //         KCMP_VM, 0, 0),
      //  but that is not compiled into our kernels and disallowed by seccomp.
      if (!client_config_.disable_vfork_detection &&
          syscall(__NR_gettid) != base::GetThreadId()) {
        postfork_return_value_ = true;
        vfork_detected = " (vfork detected)";
      } else {
        postfork_return_value_ = client_config_.disable_fork_teardown;
      }
      const char* action =
          postfork_return_value_ ? "Not shutting down" : "Shutting down";
      const char* force =
          postfork_return_value_ ? " (fork teardown disabled)" : "";
      PERFETTO_LOG(
          "Detected post-fork child situation. Not profiling the child. "
          "%s client%s%s",
          action, force, vfork_detected);
    }
    return true;
  }
  return false;
}

// The stack grows towards numerically smaller addresses, so the stack layout
// of main calling malloc is as follows.
//
//               +------------+
//               |SendWireMsg |
// stacktop +--> +------------+ 0x1000
//               |RecordMalloc|    +
//               +------------+    |
//               | malloc     |    |
//               +------------+    |
//               |  main      |    v
// stackbase +-> +------------+ 0xffff
bool Client::RecordMalloc(uint64_t sample_size,
                          uint64_t alloc_size,
                          uint64_t alloc_address) {
  if (PERFETTO_UNLIKELY(IsPostFork())) {
    return postfork_return_value_;
  }

  AllocMetadata metadata;
  const char* stackbase = GetStackBase();
  const char* stacktop = reinterpret_cast<char*>(__builtin_frame_address(0));
  unwindstack::AsmGetRegs(metadata.register_data);

  if (PERFETTO_UNLIKELY(stackbase < stacktop)) {
    PERFETTO_DFATAL_OR_ELOG("Stackbase >= stacktop.");
    return false;
  }

  uint64_t stack_size = static_cast<uint64_t>(stackbase - stacktop);
  metadata.sample_size = sample_size;
  metadata.alloc_size = alloc_size;
  metadata.alloc_address = alloc_address;
  metadata.stack_pointer = reinterpret_cast<uint64_t>(stacktop);
  metadata.stack_pointer_offset = sizeof(AllocMetadata);
  metadata.arch = unwindstack::Regs::CurrentArch();
  metadata.sequence_number =
      1 + sequence_number_.fetch_add(1, std::memory_order_acq_rel);

  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
    metadata.clock_monotonic_coarse_timestamp =
        static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
  } else {
    metadata.clock_monotonic_coarse_timestamp = 0;
  }

  WireMessage msg{};
  msg.record_type = RecordType::Malloc;
  msg.alloc_header = &metadata;
  msg.payload = const_cast<char*>(stacktop);
  msg.payload_size = static_cast<size_t>(stack_size);

  if (!SendWireMessageWithRetriesIfBlocking(msg))
    return false;

  return SendControlSocketByte();
}

bool Client::SendWireMessageWithRetriesIfBlocking(const WireMessage& msg) {
  for (uint64_t i = 0;
       max_shmem_tries_ == kInfiniteTries || i < max_shmem_tries_; ++i) {
    if (PERFETTO_LIKELY(SendWireMessage(&shmem_, msg)))
      return true;
    // retry if in blocking mode and still connected
    if (client_config_.block_client && base::IsAgain(errno) && IsConnected()) {
      usleep(kResendBackoffUs);
    } else {
      break;
    }
  }
  PERFETTO_PLOG("Failed to write to shared ring buffer. Disconnecting.");
  return false;
}

bool Client::RecordFree(const uint64_t alloc_address) {
  uint64_t sequence_number =
      1 + sequence_number_.fetch_add(1, std::memory_order_acq_rel);

  std::unique_lock<std::timed_mutex> l(free_batch_lock_, kLockTimeout);
  if (!l.owns_lock())
    return false;
  if (free_batch_.num_entries == kFreeBatchSize) {
    if (!FlushFreesLocked())
      return false;
    // Flushed the contents of the buffer, reset it for reuse.
    free_batch_.num_entries = 0;
  }
  FreeBatchEntry& current_entry =
      free_batch_.entries[free_batch_.num_entries++];
  current_entry.sequence_number = sequence_number;
  current_entry.addr = alloc_address;
  return true;
}

bool Client::FlushFreesLocked() {
  if (PERFETTO_UNLIKELY(IsPostFork())) {
    return postfork_return_value_;
  }

  WireMessage msg = {};
  msg.record_type = RecordType::Free;
  msg.free_header = &free_batch_;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
    free_batch_.clock_monotonic_coarse_timestamp =
        static_cast<uint64_t>(base::FromPosixTimespec(ts).count());
  } else {
    free_batch_.clock_monotonic_coarse_timestamp = 0;
  }

  if (!SendWireMessageWithRetriesIfBlocking(msg))
    return false;
  return SendControlSocketByte();
}

bool Client::IsConnected() {
  PERFETTO_DCHECK(!sock_.IsBlocking());
  char buf[1];
  ssize_t recv_bytes = sock_.Receive(buf, sizeof(buf), nullptr, 0);
  if (recv_bytes == 0)
    return false;
  // This is not supposed to happen because currently heapprofd does not send
  // data to the client. Here for generality's sake.
  if (recv_bytes > 0)
    return true;
  return base::IsAgain(errno);
}

bool Client::SendControlSocketByte() {
  // If base::IsAgain(errno), the socket buffer is full, so the service will
  // pick up the notification even without adding another byte.
  // In other error cases (usually EPIPE) we want to disconnect, because that
  // is how the service signals the tracing session was torn down.
  if (sock_.Send(kSingleByte, sizeof(kSingleByte)) == -1 &&
      !base::IsAgain(errno)) {
    PERFETTO_PLOG("Failed to send control socket byte.");
    return false;
  }
  return true;
}

}  // namespace profiling
}  // namespace perfetto
