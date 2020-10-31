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

#ifndef INCLUDE_PERFETTO_EXT_IPC_HOST_H_
#define INCLUDE_PERFETTO_EXT_IPC_HOST_H_

#include <memory>

#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/ipc/basic_types.h"

namespace perfetto {

namespace base {
class TaskRunner;
}  // namespace base

namespace ipc {

class Service;

// The host-side of the IPC layer. This class acts as a registry and request
// dispatcher. It listen on the UnixSocket |socket_name| for incoming requests
// (coming Client instances) and dispatches their requests to the various
// Services exposed.
class Host {
 public:
  // Creates an instance and starts listening on the given |socket_name|.
  // Returns nullptr if listening on the socket fails.
  static std::unique_ptr<Host> CreateInstance(const char* socket_name,
                                              base::TaskRunner*);

  // Like the above but takes a file descriptor to a pre-bound unix socket.
  // Returns nullptr if listening on the socket fails.
  static std::unique_ptr<Host> CreateInstance(base::ScopedFile socket_fd,
                                              base::TaskRunner*);

  virtual ~Host();

  // Registers a new service and makes it available to remote IPC peers.
  // All the exposed Service instances will be destroyed when destroying the
  // Host instance if ExposeService succeeds and returns true, or immediately
  // after the call in case of failure.
  // Returns true if the register has been successfully registered, false in
  // case of errors (e.g., another service with the same name is already
  // registered).
  virtual bool ExposeService(std::unique_ptr<Service>) = 0;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_IPC_HOST_H_
