// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/gdb-server.h"

#include <inttypes.h>
#include <functional>
#include "src/api/api-inl.h"
#include "src/api/api.h"
#include "src/debug/debug.h"
#include "src/debug/wasm/gdb-server/gdb-server-thread.h"
#include "src/utils/locked-queue-inl.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

static const uint32_t kMaxWasmCallStack = 20;

// A TaskRunner is an object that runs posted tasks (in the form of closure
// objects). Tasks are queued and run, in order, in the thread where the
// TaskRunner::RunMessageLoop() is called.
class TaskRunner {
 public:
  // Class Task wraps a std::function with a semaphore to signal its completion.
  // This logic would be neatly implemented with std::packaged_tasks but we
  // cannot use <future> in V8.
  class Task {
   public:
    Task(base::Semaphore* ready_semaphore, std::function<void()> func)
        : ready_semaphore_(ready_semaphore), func_(func) {}

    void Run() {
      func_();
      ready_semaphore_->Signal();
    }

    // A semaphore object passed by the thread that posts a task.
    // The sender can Wait on this semaphore to block until the task has
    // completed execution in the TaskRunner thread.
    base::Semaphore* ready_semaphore_;

    // The function to run.
    std::function<void()> func_;
  };

  TaskRunner()
      : process_queue_semaphore_(0),
        nested_loop_count_(0),
        is_terminated_(false) {}

  TaskRunner(const TaskRunner&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;

  // Starts the task runner. All tasks posted are run, in order, in the thread
  // that calls this function.
  void Run() {
    is_terminated_ = false;
    int loop_number = ++nested_loop_count_;
    while (nested_loop_count_ == loop_number && !is_terminated_) {
      std::shared_ptr<Task> task = GetNext();
      if (task) {
        task->Run();
      }
    }
  }

  // Terminates the task runner. Tasks that are still pending in the queue are
  // not discarded and will be executed when the task runner is restarted.
  void Terminate() {
    DCHECK_LT(0, nested_loop_count_);
    --nested_loop_count_;

    is_terminated_ = true;
    process_queue_semaphore_.Signal();
  }

  // Posts a task to the task runner, to be executed in the task runner thread.
  template <typename Functor>
  auto Append(base::Semaphore* ready_semaphore, Functor&& task) {
    queue_.Enqueue(std::make_shared<Task>(ready_semaphore, task));
    process_queue_semaphore_.Signal();
  }

 private:
  std::shared_ptr<Task> GetNext() {
    while (!is_terminated_) {
      if (queue_.IsEmpty()) {
        process_queue_semaphore_.Wait();
      }

      std::shared_ptr<Task> task;
      if (queue_.Dequeue(&task)) {
        return task;
      }
    }
    return nullptr;
  }

  LockedQueue<std::shared_ptr<Task>> queue_;
  v8::base::Semaphore process_queue_semaphore_;
  int nested_loop_count_;
  std::atomic<bool> is_terminated_;
};

GdbServer::GdbServer() : has_module_list_changed_(false) {
  task_runner_ = std::make_unique<TaskRunner>();
}

template <typename Functor>
auto GdbServer::RunSyncTask(Functor&& callback) const {
  // Executed in the GDBServerThread.
  v8::base::Semaphore ready_semaphore(0);
  task_runner_->Append(&ready_semaphore, callback);
  ready_semaphore.Wait();
}

// static
std::unique_ptr<GdbServer> GdbServer::Create() {
  DCHECK(v8_flags.wasm_gdb_remote);

  std::unique_ptr<GdbServer> gdb_server(new GdbServer());

  // Spawns the GDB-stub thread where all the communication with the debugger
  // happens.
  gdb_server->thread_ = std::make_unique<GdbServerThread>(gdb_server.get());
  if (!gdb_server->thread_->StartAndInitialize()) {
    TRACE_GDB_REMOTE(
        "Cannot initialize thread, GDB-remote debugging will be disabled.\n");
    return nullptr;
  }
  return gdb_server;
}

GdbServer::~GdbServer() {
  // All Isolates have been deregistered.
  DCHECK(isolate_delegates_.empty());

  if (thread_) {
    // Waits for the GDB-stub thread to terminate.
    thread_->Stop();
    thread_->Join();
  }
}

void GdbServer::RunMessageLoopOnPause() { task_runner_->Run(); }

void GdbServer::QuitMessageLoopOnPause() { task_runner_->Terminate(); }

std::vector<GdbServer::WasmModuleInfo> GdbServer::GetLoadedModules(
    bool clear_module_list_changed_flag) {
  // Executed in the GDBServerThread.
  std::vector<GdbServer::WasmModuleInfo> modules;

  RunSyncTask([this, &modules, clear_module_list_changed_flag]() {
    // Executed in the isolate thread.
    for (const auto& pair : scripts_) {
      uint32_t module_id = pair.first;
      const WasmModuleDebug& module_debug = pair.second;
      modules.push_back({module_id, module_debug.GetModuleName()});
    }

    if (clear_module_list_changed_flag) has_module_list_changed_ = false;
  });
  return modules;
}

bool GdbServer::GetModuleDebugHandler(uint32_t module_id,
                                      WasmModuleDebug** wasm_module_debug) {
  // Always executed in the isolate thread.
  ScriptsMap::iterator scriptIterator = scripts_.find(module_id);
  if (scriptIterator != scripts_.end()) {
    *wasm_module_debug = &scriptIterator->second;
    return true;
  }
  wasm_module_debug = nullptr;
  return false;
}

bool GdbServer::GetWasmGlobal(uint32_t frame_index, uint32_t index,
                              uint8_t* buffer, uint32_t buffer_size,
                              uint32_t* size) {
  // Executed in the GDBServerThread.
  bool result = false;
  RunSyncTask([this, &result, frame_index, index, buffer, buffer_size, size]() {
    // Executed in the isolate thread.
    result = WasmModuleDebug::GetWasmGlobal(GetTarget().GetCurrentIsolate(),
                                            frame_index, index, buffer,
                                            buffer_size, size);
  });
  return result;
}

bool GdbServer::GetWasmLocal(uint32_t frame_index, uint32_t index,
                             uint8_t* buffer, uint32_t buffer_size,
                             uint32_t* size) {
  // Executed in the GDBServerThread.
  bool result = false;
  RunSyncTask([this, &result, frame_index, index, buffer, buffer_size, size]() {
    // Executed in the isolate thread.
    result = WasmModuleDebug::GetWasmLocal(GetTarget().GetCurrentIsolate(),
                                           frame_index, index, buffer,
                                           buffer_size, size);
  });
  return result;
}

bool GdbServer::GetWasmStackValue(uint32_t frame_index, uint32_t index,
                                  uint8_t* buffer, uint32_t buffer_size,
                                  uint32_t* size) {
  // Executed in the GDBServerThread.
  bool result = false;
  RunSyncTask([this, &result, frame_index, index, buffer, buffer_size, size]() {
    // Executed in the isolate thread.
    result = WasmModuleDebug::GetWasmStackValue(GetTarget().GetCurrentIsolate(),
                                                frame_index, index, buffer,
                                                buffer_size, size);
  });
  return result;
}

uint32_t GdbServer::GetWasmMemory(uint32_t module_id, uint32_t offset,
                                  uint8_t* buffer, uint32_t size) {
  // Executed in the GDBServerThread.
  uint32_t bytes_read = 0;
  RunSyncTask([this, &bytes_read, module_id, offset, buffer, size]() {
    // Executed in the isolate thread.
    WasmModuleDebug* module_debug = nullptr;
    if (GetModuleDebugHandler(module_id, &module_debug)) {
      bytes_read = module_debug->GetWasmMemory(GetTarget().GetCurrentIsolate(),
                                               offset, buffer, size);
    }
  });
  return bytes_read;
}

uint32_t GdbServer::GetWasmData(uint32_t module_id, uint32_t offset,
                                uint8_t* buffer, uint32_t size) {
  // Executed in the GDBServerThread.
  uint32_t bytes_read = 0;
  RunSyncTask([this, &bytes_read, module_id, offset, buffer, size]() {
    // Executed in the isolate thread.
    WasmModuleDebug* module_debug = nullptr;
    if (GetModuleDebugHandler(module_id, &module_debug)) {
      bytes_read = module_debug->GetWasmData(GetTarget().GetCurrentIsolate(),
                                             offset, buffer, size);
    }
  });
  return bytes_read;
}

uint32_t GdbServer::GetWasmModuleBytes(wasm_addr_t wasm_addr, uint8_t* buffer,
                                       uint32_t size) {
  // Executed in the GDBServerThread.
  uint32_t bytes_read = 0;
  RunSyncTask([this, &bytes_read, wasm_addr, buffer, size]() {
    // Executed in the isolate thread.
    WasmModuleDebug* module_debug;
    if (GetModuleDebugHandler(wasm_addr.ModuleId(), &module_debug)) {
      bytes_read = module_debug->GetWasmModuleBytes(wasm_addr, buffer, size);
    }
  });
  return bytes_read;
}

bool GdbServer::AddBreakpoint(uint32_t wasm_module_id, uint32_t offset) {
  // Executed in the GDBServerThread.
  bool result = false;
  RunSyncTask([this, &result, wasm_module_id, offset]() {
    // Executed in the isolate thread.
    WasmModuleDebug* module_debug;
    if (GetModuleDebugHandler(wasm_module_id, &module_debug)) {
      int breakpoint_id = 0;
      if (module_debug->AddBreakpoint(offset, &breakpoint_id)) {
        breakpoints_[wasm_addr_t(wasm_module_id, offset)] = breakpoint_id;
        result = true;
      }
    }
  });
  return result;
}

bool GdbServer::RemoveBreakpoint(uint32_t wasm_module_id, uint32_t offset) {
  // Executed in the GDBServerThread.
  bool result = false;
  RunSyncTask([this, &result, wasm_module_id, offset]() {
    // Executed in the isolate thread.
    BreakpointsMap::iterator it =
        breakpoints_.find(wasm_addr_t(wasm_module_id, offset));
    if (it != breakpoints_.end()) {
      int breakpoint_id = it->second;
      breakpoints_.erase(it);

      WasmModuleDebug* module_debug;
      if (GetModuleDebugHandler(wasm_module_id, &module_debug)) {
        module_debug->RemoveBreakpoint(offset, breakpoint_id);
        result = true;
      }
    }
  });
  return result;
}

std::vector<wasm_addr_t> GdbServer::GetWasmCallStack() const {
  // Executed in the GDBServerThread.
  std::vector<wasm_addr_t> result;
  RunSyncTask([this, &result]() {
    // Executed in the isolate thread.
    result = GetTarget().GetCallStack();
  });
  return result;
}

void GdbServer::AddIsolate(Isolate* isolate) {
  // Executed in the isolate thread.
  if (isolate_delegates_.find(isolate) == isolate_delegates_.end()) {
    isolate_delegates_[isolate] =
        std::make_unique<DebugDelegate>(isolate, this);
  }
}

void GdbServer::RemoveIsolate(Isolate* isolate) {
  // Executed in the isolate thread.
  auto it = isolate_delegates_.find(isolate);
  if (it != isolate_delegates_.end()) {
    for (auto it = scripts_.begin(); it != scripts_.end();) {
      if (it->second.GetIsolate() == isolate) {
        it = scripts_.erase(it);
        has_module_list_changed_ = true;
      } else {
        ++it;
      }
    }
    isolate_delegates_.erase(it);
  }
}

void GdbServer::Suspend() {
  // Executed in the GDBServerThread.
  auto it = isolate_delegates_.begin();
  if (it != isolate_delegates_.end()) {
    Isolate* isolate = it->first;
    v8::Isolate* v8Isolate = (v8::Isolate*)isolate;
    v8Isolate->RequestInterrupt(
        // Executed in the isolate thread.
        [](v8::Isolate* isolate, void*) {
          if (v8::debug::AllFramesOnStackAreBlackboxed(isolate)) {
            v8::debug::SetBreakOnNextFunctionCall(isolate);
          } else {
            v8::debug::BreakRightNow(isolate);
          }
        },
        this);
  }
}

void GdbServer::PrepareStep() {
  // Executed in the GDBServerThread.
  wasm_addr_t pc = GetTarget().GetCurrentPc();
  RunSyncTask([this, pc]() {
    // Executed in the isolate thread.
    WasmModuleDebug* module_debug;
    if (GetModuleDebugHandler(pc.ModuleId(), &module_debug)) {
      module_debug->PrepareStep();
    }
  });
}

void GdbServer::AddWasmModule(uint32_t module_id,
                              Local<debug::WasmScript> wasm_script) {
  // Executed in the isolate thread.
  DCHECK_EQ(Script::TYPE_WASM, Utils::OpenHandle(*wasm_script)->type());
  v8::Isolate* isolate = wasm_script->GetIsolate();
  scripts_.insert(
      std::make_pair(module_id, WasmModuleDebug(isolate, wasm_script)));
  has_module_list_changed_ = true;

  if (v8_flags.wasm_pause_waiting_for_debugger && scripts_.size() == 1) {
    TRACE_GDB_REMOTE("Paused, waiting for a debugger to attach...\n");
    Suspend();
  }
}

Target& GdbServer::GetTarget() const { return thread_->GetTarget(); }

// static
std::atomic<uint32_t> GdbServer::DebugDelegate::id_s;

GdbServer::DebugDelegate::DebugDelegate(Isolate* isolate, GdbServer* gdb_server)
    : isolate_(isolate), id_(id_s++), gdb_server_(gdb_server) {
  isolate_->SetCaptureStackTraceForUncaughtExceptions(
      true, kMaxWasmCallStack, v8::StackTrace::kOverview);

  // Register the delegate
  isolate_->debug()->SetDebugDelegate(this);
  v8::debug::EnterDebuggingForIsolate((v8::Isolate*)isolate_);
  v8::debug::ChangeBreakOnException((v8::Isolate*)isolate_,
                                    v8::debug::BreakOnUncaughtException);
}

GdbServer::DebugDelegate::~DebugDelegate() {
  // Deregister the delegate
  isolate_->debug()->SetDebugDelegate(nullptr);
}

void GdbServer::DebugDelegate::ScriptCompiled(Local<debug::Script> script,
                                              bool is_live_edited,
                                              bool has_compile_error) {
  // Executed in the isolate thread.
  if (script->IsWasm()) {
    DCHECK_EQ(reinterpret_cast<v8::Isolate*>(isolate_), script->GetIsolate());
    gdb_server_->AddWasmModule(GetModuleId(script->Id()),
                               script.As<debug::WasmScript>());
  }
}

void GdbServer::DebugDelegate::BreakProgramRequested(
    // Executed in the isolate thread.
    Local<v8::Context> paused_context,
    const std::vector<debug::BreakpointId>& inspector_break_points_hit,
    v8::debug::BreakReasons break_reasons) {
  gdb_server_->GetTarget().OnProgramBreak(
      isolate_, WasmModuleDebug::GetCallStack(id_, isolate_));
  gdb_server_->RunMessageLoopOnPause();
}

void GdbServer::DebugDelegate::ExceptionThrown(
    // Executed in the isolate thread.
    Local<v8::Context> paused_context, Local<Value> exception,
    Local<Value> promise, bool is_uncaught,
    debug::ExceptionType exception_type) {
  if (exception_type == v8::debug::kException && is_uncaught) {
    gdb_server_->GetTarget().OnException(
        isolate_, WasmModuleDebug::GetCallStack(id_, isolate_));
    gdb_server_->RunMessageLoopOnPause();
  }
}

bool GdbServer::DebugDelegate::IsFunctionBlackboxed(
    // Executed in the isolate thread.
    Local<debug::Script> script, const debug::Location& start,
    const debug::Location& end) {
  return false;
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
