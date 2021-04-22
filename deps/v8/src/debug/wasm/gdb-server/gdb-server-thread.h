// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_THREAD_H_
#define V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_THREAD_H_

#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/debug/wasm/gdb-server/target.h"
#include "src/debug/wasm/gdb-server/transport.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class GdbServer;

// class GdbServerThread spawns a thread where all communication with a debugger
// happens.
class GdbServerThread : public v8::base::Thread {
 public:
  explicit GdbServerThread(GdbServer* gdb_server);
  GdbServerThread(const GdbServerThread&) = delete;
  GdbServerThread& operator=(const GdbServerThread&) = delete;

  // base::Thread
  void Run() override;

  // Starts the GDB-server thread and waits Run() method is called on the new
  // thread and the initialization completes.
  bool StartAndInitialize();

  // Stops the GDB-server thread when the V8 process shuts down; gracefully
  // closes any active debugging session.
  void Stop();

  Target& GetTarget() { return *target_; }

 private:
  void CleanupThread();

  GdbServer* gdb_server_;

  // Used to block the caller on StartAndInitialize() waiting for the new thread
  // to have completed its initialization.
  // (Note that Thread::StartSynchronously() wouldn't work in this case because
  // it returns as soon as the new thread starts, but before Run() is called).
  base::Semaphore start_semaphore_;

  base::Mutex mutex_;
  // Protected by {mutex_}:
  std::unique_ptr<TransportBase> transport_;
  std::unique_ptr<Target> target_;
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_THREAD_H_
