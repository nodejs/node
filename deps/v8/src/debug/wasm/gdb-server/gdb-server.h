// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_
#define V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_

#include <map>
#include <memory>
#include "src/debug/wasm/gdb-server/gdb-server-thread.h"
#include "src/debug/wasm/gdb-server/wasm-module-debug.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class TaskRunner;

// class GdbServer acts as a manager for the GDB-remote stub. It is instantiated
// as soon as the first Wasm module is loaded in the Wasm engine and spawns a
// separate thread to accept connections and exchange messages with a debugger.
// It will contain the logic to serve debugger queries and access the state of
// the Wasm engine.
class GdbServer {
 public:
  GdbServer(const GdbServer&) = delete;
  GdbServer& operator=(const GdbServer&) = delete;

  // Factory method: creates and returns a GdbServer. Spawns a "GDB-remote"
  // thread that will be used to communicate with the debugger.
  // May return null on failure.
  // This should be called once, the first time a Wasm module is loaded in the
  // Wasm engine.
  static std::unique_ptr<GdbServer> Create();

  // Stops the "GDB-remote" thread and waits for it to complete. This should be
  // called once, when the Wasm engine shuts down.
  ~GdbServer();

  // Queries the set of the Wasm modules currently loaded. Each module is
  // identified by a unique integer module id.
  struct WasmModuleInfo {
    uint32_t module_id;
    std::string module_name;
  };
  std::vector<WasmModuleInfo> GetLoadedModules(
      bool clear_module_list_changed_flag = false);

  bool HasModuleListChanged() const { return has_module_list_changed_; }

  // Queries the value of the {index} global value in the Wasm module identified
  // by {frame_index}.
  //
  bool GetWasmGlobal(uint32_t frame_index, uint32_t index, uint8_t* buffer,
                     uint32_t buffer_size, uint32_t* size);

  // Queries the value of the {index} local value in the {frame_index}th stack
  // frame in the Wasm module identified by {frame_index}.
  //
  bool GetWasmLocal(uint32_t frame_index, uint32_t index, uint8_t* buffer,
                    uint32_t buffer_size, uint32_t* size);

  // Queries the value of the {index} value in the operand stack.
  //
  bool GetWasmStackValue(uint32_t frame_index, uint32_t index, uint8_t* buffer,
                         uint32_t buffer_size, uint32_t* size);

  // Reads {size} bytes, starting from {offset}, from the Memory instance
  // associated to the Wasm module identified by {module_id}.
  // Returns the number of bytes copied to {buffer}, or 0 is case of error.
  // Note: only one Memory for Module is currently supported.
  //
  uint32_t GetWasmMemory(uint32_t module_id, uint32_t offset, uint8_t* buffer,
                         uint32_t size);

  // Reads {size} bytes, starting from {offset}, from the first Data segment
  // in the Wasm module identified by {module_id}.
  // Returns the number of bytes copied to {buffer}, or 0 is case of error.
  // Note: only one Memory for Module is currently supported.
  //
  uint32_t GetWasmData(uint32_t module_id, uint32_t offset, uint8_t* buffer,
                       uint32_t size);

  // Reads {size} bytes, starting from the low dword of {address}, from the Code
  // space of th Wasm module identified by high dword of {address}.
  // Returns the number of bytes copied to {buffer}, or 0 is case of error.
  uint32_t GetWasmModuleBytes(wasm_addr_t address, uint8_t* buffer,
                              uint32_t size);

  // Inserts a breakpoint at the offset {offset} of the Wasm module identified
  // by {wasm_module_id}.
  // Returns true if the breakpoint was successfully added.
  bool AddBreakpoint(uint32_t wasm_module_id, uint32_t offset);

  // Removes a breakpoint at the offset {offset} of the Wasm module identified
  // by {wasm_module_id}.
  // Returns true if the breakpoint was successfully removed.
  bool RemoveBreakpoint(uint32_t wasm_module_id, uint32_t offset);

  // Returns the current call stack as a vector of program counters.
  std::vector<wasm_addr_t> GetWasmCallStack() const;

  // Manage the set of Isolates for this GdbServer.
  void AddIsolate(Isolate* isolate);
  void RemoveIsolate(Isolate* isolate);

  // Requests that the thread suspend execution at the next Wasm instruction.
  void Suspend();

  // Handle stepping in wasm functions via the wasm interpreter.
  void PrepareStep();

  // Called when the target debuggee can resume execution (for example after
  // having been suspended on a breakpoint). Terminates the task runner leaving
  // all pending tasks in the queue.
  void QuitMessageLoopOnPause();

 private:
  GdbServer();

  // When the target debuggee is suspended for a breakpoint or exception, blocks
  // the main (isolate) thread and enters in a message loop. Here it waits on a
  // queue of Task objects that are posted by the GDB-stub thread and that
  // represent queries received from the debugger via the GDB-remote protocol.
  void RunMessageLoopOnPause();

  // Post a task to run a callback in the isolate thread.
  template <typename Callback>
  auto RunSyncTask(Callback&& callback) const;

  void AddWasmModule(uint32_t module_id, Local<debug::WasmScript> wasm_script);

  // Given a Wasm module id, retrieves the corresponding debugging WasmScript
  // object.
  bool GetModuleDebugHandler(uint32_t module_id,
                             WasmModuleDebug** wasm_module_debug);

  // Returns the debugging target.
  Target& GetTarget() const;

  // Class DebugDelegate implements the debug::DebugDelegate interface to
  // receive notifications when debug events happen in a given isolate, like a
  // script being loaded, a breakpoint being hit, an exception being thrown.
  class DebugDelegate : public debug::DebugDelegate {
   public:
    DebugDelegate(Isolate* isolate, GdbServer* gdb_server);
    ~DebugDelegate();

    // debug::DebugDelegate
    void ScriptCompiled(Local<debug::Script> script, bool is_live_edited,
                        bool has_compile_error) override;
    void BreakProgramRequested(
        Local<v8::Context> paused_context,
        const std::vector<debug::BreakpointId>& inspector_break_points_hit,
        v8::debug::BreakReasons break_reasons) override;
    void ExceptionThrown(Local<v8::Context> paused_context,
                         Local<Value> exception, Local<Value> promise,
                         bool is_uncaught,
                         debug::ExceptionType exception_type) override;
    bool IsFunctionBlackboxed(Local<debug::Script> script,
                              const debug::Location& start,
                              const debug::Location& end) override;

   private:
    // Calculates module_id as:
    // +--------------------+------------------- +
    // | DebugDelegate::id_ |    Script::Id()    |
    // +--------------------+------------------- +
    //  <----- 16 bit -----> <----- 16 bit ----->
    uint32_t GetModuleId(uint32_t script_id) const {
      DCHECK_LT(script_id, 0x10000);
      DCHECK_LT(id_, 0x10000);
      return id_ << 16 | script_id;
    }

    Isolate* isolate_;
    uint32_t id_;
    GdbServer* gdb_server_;

    static std::atomic<uint32_t> id_s;
  };

  // The GDB-stub thread where all the communication with the debugger happens.
  std::unique_ptr<GdbServerThread> thread_;

  // Used to transform the queries that arrive in the GDB-stub thread into
  // tasks executed in the main (isolate) thread.
  std::unique_ptr<TaskRunner> task_runner_;

  std::atomic<bool> has_module_list_changed_;

  //////////////////////////////////////////////////////////////////////////////
  // Always accessed in the isolate thread.

  // Set of breakpoints currently defines in Wasm code.
  typedef std::map<uint64_t, int> BreakpointsMap;
  BreakpointsMap breakpoints_;

  typedef std::map<uint32_t, WasmModuleDebug> ScriptsMap;
  ScriptsMap scripts_;

  typedef std::map<Isolate*, std::unique_ptr<DebugDelegate>>
      IsolateDebugDelegateMap;
  IsolateDebugDelegateMap isolate_delegates_;

  // End of fields always accessed in the isolate thread.
  //////////////////////////////////////////////////////////////////////////////
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_
