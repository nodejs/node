/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/profiling/perf/proc_descriptors.h"

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

namespace perfetto {

ProcDescriptorDelegate::~ProcDescriptorDelegate() {}

ProcDescriptorGetter::~ProcDescriptorGetter() {}

// DirectDescriptorGetter:

DirectDescriptorGetter::~DirectDescriptorGetter() {}

void DirectDescriptorGetter::SetDelegate(ProcDescriptorDelegate* delegate) {
  delegate_ = delegate;
}

void DirectDescriptorGetter::GetDescriptorsForPid(pid_t pid) {
  char buf[128] = {};
  snprintf(buf, sizeof(buf), "/proc/%d/maps", pid);
  auto maps_fd = base::ScopedFile{open(buf, O_RDONLY | O_CLOEXEC)};
  if (!maps_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      PERFETTO_PLOG("Failed to open [%s]", buf);

    return;
  }

  snprintf(buf, sizeof(buf), "/proc/%d/mem", pid);
  auto mem_fd = base::ScopedFile{open(buf, O_RDONLY | O_CLOEXEC)};
  if (!mem_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      PERFETTO_PLOG("Failed to open [%s]", buf);

    return;
  }

  delegate_->OnProcDescriptors(pid, std::move(maps_fd), std::move(mem_fd));
}

// AndroidRemoteDescriptorGetter:

AndroidRemoteDescriptorGetter::~AndroidRemoteDescriptorGetter() = default;

void AndroidRemoteDescriptorGetter::SetDelegate(
    ProcDescriptorDelegate* delegate) {
  delegate_ = delegate;
}

#if !PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
void AndroidRemoteDescriptorGetter::GetDescriptorsForPid(pid_t) {
  PERFETTO_FATAL("Unexpected build type for AndroidRemoteDescriptorGetter");
}
#else
void AndroidRemoteDescriptorGetter::GetDescriptorsForPid(pid_t pid) {
  constexpr static int kPerfProfilerSignalValue = 1;
  constexpr static int kProfilerSignal = __SIGRTMIN + 4;

  PERFETTO_DLOG("Sending signal to pid [%d]", pid);
  union sigval signal_value;
  signal_value.sival_int = kPerfProfilerSignalValue;
  if (sigqueue(pid, kProfilerSignal, signal_value) != 0 && errno != ESRCH) {
    PERFETTO_DPLOG("Failed sigqueue(%d)", pid);
  }
}
#endif

void AndroidRemoteDescriptorGetter::OnNewIncomingConnection(
    base::UnixSocket*,
    std::unique_ptr<base::UnixSocket> new_connection) {
  PERFETTO_DLOG("remote fds: new connection from pid [%d]",
                static_cast<int>(new_connection->peer_pid()));

  active_connections_.emplace(new_connection.get(), std::move(new_connection));
}

void AndroidRemoteDescriptorGetter::OnDisconnect(base::UnixSocket* self) {
  PERFETTO_DLOG("remote fds: disconnect from pid [%d]",
                static_cast<int>(self->peer_pid()));

  auto it = active_connections_.find(self);
  PERFETTO_CHECK(it != active_connections_.end());
  active_connections_.erase(it);
}

// Note: this callback will fire twice for a given connection. Once for the file
// descriptors, and once during the disconnect (with 0 bytes available in the
// socket).
void AndroidRemoteDescriptorGetter::OnDataAvailable(base::UnixSocket* self) {
  // Expect two file descriptors (maps, followed by mem).
  base::ScopedFile fds[2];
  char buf[1];
  size_t received_bytes =
      self->Receive(buf, sizeof(buf), fds, base::ArraySize(fds));

  PERFETTO_DLOG("remote fds: received %zu bytes", received_bytes);
  if (!received_bytes)
    return;

  delegate_->OnProcDescriptors(self->peer_pid(), std::move(fds[0]),
                               std::move(fds[1]));
}

}  // namespace perfetto
