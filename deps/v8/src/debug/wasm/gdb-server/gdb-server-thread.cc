// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/gdb-server-thread.h"

#include "src/debug/wasm/gdb-server/gdb-server.h"
#include "src/debug/wasm/gdb-server/session.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

GdbServerThread::GdbServerThread(GdbServer* gdb_server)
    : Thread(v8::base::Thread::Options("GdbServerThread")),
      gdb_server_(gdb_server),
      start_semaphore_(0) {}

bool GdbServerThread::StartAndInitialize() {
  // Executed in the Isolate thread.
  if (!Start()) {
    return false;
  }

  // We need to make sure that {Stop} is never called before the thread has
  // completely initialized {transport_} and {target_}. Otherwise there could be
  // a race condition where in the main thread {Stop} might get called before
  // the transport is created, and then in the GDBServer thread we may have time
  // to setup the transport and block on accept() before the main thread blocks
  // on joining the thread.
  // The small performance hit caused by this Wait should be negligeable because
  // this operation happensat most once per process and only when the
  // --wasm-gdb-remote flag is set.
  start_semaphore_.Wait();
  return !!target_;
}

void GdbServerThread::CleanupThread() {
  // Executed in the GdbServer thread.
  v8::base::MutexGuard guard(&mutex_);

  target_ = nullptr;
  transport_ = nullptr;

#if _WIN32
  ::WSACleanup();
#endif
}

void GdbServerThread::Run() {
  // Executed in the GdbServer thread.
#ifdef _WIN32
  // Initialize Winsock
  WSADATA wsaData;
  int iResult = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0) {
    TRACE_GDB_REMOTE("GdbServerThread::Run: WSAStartup failed\n");
    return;
  }
#endif

  // If the default port is not available, try any port.
  SocketBinding socket_binding =
      SocketBinding::Bind(v8_flags.wasm_gdb_remote_port);
  if (!socket_binding.IsValid()) {
    socket_binding = SocketBinding::Bind(0);
  }
  if (!socket_binding.IsValid()) {
    TRACE_GDB_REMOTE("GdbServerThread::Run: Failed to bind any TCP port\n");
    return;
  }
  TRACE_GDB_REMOTE("gdb-remote(%d) : Connect GDB with 'target remote :%d\n",
                   __LINE__, socket_binding.GetBoundPort());

  transport_ = socket_binding.CreateTransport();
  target_ = std::make_unique<Target>(gdb_server_);

  // Here we have completed the initialization, and the thread that called
  // {StartAndInitialize} may resume execution.
  start_semaphore_.Signal();

  while (!target_->IsTerminated()) {
    // Wait for incoming connections.
    if (!transport_->AcceptConnection()) {
      continue;
    }

    // Create a new session for this connection
    Session session(transport_.get());
    TRACE_GDB_REMOTE("GdbServerThread: Connected\n");

    // Run this session for as long as it lasts
    target_->Run(&session);
  }
  CleanupThread();
}

void GdbServerThread::Stop() {
  // Executed in the Isolate thread.

  // Synchronized, becauses {Stop} might be called while {Run} is still
  // initializing {transport_} and {target_}. If this happens and the thread is
  // blocked waiting for an incoming connection or GdbServer for incoming
  // packets, it will unblocked when {transport_} is closed.
  v8::base::MutexGuard guard(&mutex_);

  if (target_) {
    target_->Terminate();
  }

  if (transport_) {
    transport_->Close();
  }
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
