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

#ifndef SRC_PROFILING_PERF_PROC_DESCRIPTORS_H_
#define SRC_PROFILING_PERF_PROC_DESCRIPTORS_H_

#include <sys/types.h>

#include <map>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_socket.h"

namespace perfetto {

// Callback interface for receiving /proc/<pid>/ file descriptors (proc-fds)
// from |ProcDescriptorGetter|.
class ProcDescriptorDelegate {
 public:
  virtual void OnProcDescriptors(pid_t pid,
                                 base::ScopedFile maps_fd,
                                 base::ScopedFile mem_fd) = 0;

  virtual ~ProcDescriptorDelegate();
};

class ProcDescriptorGetter {
 public:
  virtual void GetDescriptorsForPid(pid_t pid) = 0;
  virtual void SetDelegate(ProcDescriptorDelegate* delegate) = 0;
  // TODO(b/151835887): attempt to remove the race condition in the Android
  // platform itself, and remove this best-effort workaround.
  virtual bool RequiresDelayedRequest() { return false; }

  virtual ~ProcDescriptorGetter();
};

// Directly opens /proc/<pid>/{maps,mem} files. Used when the daemon is running
// with sufficient privileges to do so.
class DirectDescriptorGetter : public ProcDescriptorGetter {
 public:
  void GetDescriptorsForPid(pid_t pid) override;
  void SetDelegate(ProcDescriptorDelegate* delegate) override;

  ~DirectDescriptorGetter() override;

 private:
  ProcDescriptorDelegate* delegate_ = nullptr;
};

// Implementation of |ProcDescriptorGetter| used when running as a system daemon
// on Android. Uses a socket inherited from |init| and platform signal handlers
// to obtain the proc-fds.
class AndroidRemoteDescriptorGetter : public ProcDescriptorGetter,
                                      public base::UnixSocket::EventListener {
 public:
  AndroidRemoteDescriptorGetter(int listening_raw_socket,
                                base::TaskRunner* task_runner) {
    listening_socket_ = base::UnixSocket::Listen(
        base::ScopedFile(listening_raw_socket), this, task_runner,
        base::SockFamily::kUnix, base::SockType::kStream);
  }

  // ProcDescriptorGetter impl:
  void GetDescriptorsForPid(pid_t pid) override;
  void SetDelegate(ProcDescriptorDelegate* delegate) override;
  bool RequiresDelayedRequest() override { return true; }

  // UnixSocket::EventListener impl:
  void OnNewIncomingConnection(
      base::UnixSocket*,
      std::unique_ptr<base::UnixSocket> new_connection) override;
  void OnDataAvailable(base::UnixSocket* self) override;
  void OnDisconnect(base::UnixSocket* self) override;

  ~AndroidRemoteDescriptorGetter() override;

 private:
  ProcDescriptorDelegate* delegate_ = nullptr;
  std::unique_ptr<base::UnixSocket> listening_socket_;
  // Holds onto connections until we receive the file descriptors (keyed by
  // their raw addresses, which the map keeps stable).
  std::map<base::UnixSocket*, std::unique_ptr<base::UnixSocket>>
      active_connections_;
};

}  // namespace perfetto

#endif  // SRC_PROFILING_PERF_PROC_DESCRIPTORS_H_
