// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_DEBUG_H_
#define V8_DEBUG_H_

#include "allocation.h"
#include "arguments.h"
#include "assembler.h"
#include "debug-agent.h"
#include "execution.h"
#include "factory.h"
#include "flags.h"
#include "frames-inl.h"
#include "hashmap.h"
#include "platform.h"
#include "string-stream.h"
#include "v8threads.h"

#ifdef ENABLE_DEBUGGER_SUPPORT
#include "../include/v8-debug.h"

namespace v8 {
namespace internal {


// Forward declarations.
class EnterDebugger;


// Step actions. NOTE: These values are in macros.py as well.
enum StepAction {
  StepNone = -1,  // Stepping not prepared.
  StepOut = 0,   // Step out of the current function.
  StepNext = 1,  // Step to the next statement in the current function.
  StepIn = 2,    // Step into new functions invoked or the next statement
                 // in the current function.
  StepMin = 3,   // Perform a minimum step in the current function.
  StepInMin = 4  // Step into new functions invoked or perform a minimum step
                 // in the current function.
};


// Type of exception break. NOTE: These values are in macros.py as well.
enum ExceptionBreakType {
  BreakException = 0,
  BreakUncaughtException = 1
};


// Type of exception break. NOTE: These values are in macros.py as well.
enum BreakLocatorType {
  ALL_BREAK_LOCATIONS = 0,
  SOURCE_BREAK_LOCATIONS = 1
};


// Class for iterating through the break points in a function and changing
// them.
class BreakLocationIterator {
 public:
  explicit BreakLocationIterator(Handle<DebugInfo> debug_info,
                                 BreakLocatorType type);
  virtual ~BreakLocationIterator();

  void Next();
  void Next(int count);
  void FindBreakLocationFromAddress(Address pc);
  void FindBreakLocationFromPosition(int position);
  void Reset();
  bool Done() const;
  void SetBreakPoint(Handle<Object> break_point_object);
  void ClearBreakPoint(Handle<Object> break_point_object);
  void SetOneShot();
  void ClearOneShot();
  void PrepareStepIn();
  bool IsExit() const;
  bool HasBreakPoint();
  bool IsDebugBreak();
  Object* BreakPointObjects();
  void ClearAllDebugBreak();


  inline int code_position() {
    return static_cast<int>(pc() - debug_info_->code()->entry());
  }
  inline int break_point() { return break_point_; }
  inline int position() { return position_; }
  inline int statement_position() { return statement_position_; }
  inline Address pc() { return reloc_iterator_->rinfo()->pc(); }
  inline Code* code() { return debug_info_->code(); }
  inline RelocInfo* rinfo() { return reloc_iterator_->rinfo(); }
  inline RelocInfo::Mode rmode() const {
    return reloc_iterator_->rinfo()->rmode();
  }
  inline RelocInfo* original_rinfo() {
    return reloc_iterator_original_->rinfo();
  }
  inline RelocInfo::Mode original_rmode() const {
    return reloc_iterator_original_->rinfo()->rmode();
  }

  bool IsDebuggerStatement();

 protected:
  bool RinfoDone() const;
  void RinfoNext();

  BreakLocatorType type_;
  int break_point_;
  int position_;
  int statement_position_;
  Handle<DebugInfo> debug_info_;
  RelocIterator* reloc_iterator_;
  RelocIterator* reloc_iterator_original_;

 private:
  void SetDebugBreak();
  void ClearDebugBreak();

  void SetDebugBreakAtIC();
  void ClearDebugBreakAtIC();

  bool IsDebugBreakAtReturn();
  void SetDebugBreakAtReturn();
  void ClearDebugBreakAtReturn();

  bool IsDebugBreakSlot();
  bool IsDebugBreakAtSlot();
  void SetDebugBreakAtSlot();
  void ClearDebugBreakAtSlot();

  DISALLOW_COPY_AND_ASSIGN(BreakLocationIterator);
};


// Cache of all script objects in the heap. When a script is added a weak handle
// to it is created and that weak handle is stored in the cache. The weak handle
// callback takes care of removing the script from the cache. The key used in
// the cache is the script id.
class ScriptCache : private HashMap {
 public:
  ScriptCache() : HashMap(ScriptMatch), collected_scripts_(10) {}
  virtual ~ScriptCache() { Clear(); }

  // Add script to the cache.
  void Add(Handle<Script> script);

  // Return the scripts in the cache.
  Handle<FixedArray> GetScripts();

  // Generate debugger events for collected scripts.
  void ProcessCollectedScripts();

 private:
  // Calculate the hash value from the key (script id).
  static uint32_t Hash(int key) {
    return ComputeIntegerHash(key, v8::internal::kZeroHashSeed);
  }

  // Scripts match if their keys (script id) match.
  static bool ScriptMatch(void* key1, void* key2) { return key1 == key2; }

  // Clear the cache releasing all the weak handles.
  void Clear();

  // Weak handle callback for scripts in the cache.
  static void HandleWeakScript(v8::Persistent<v8::Value> obj, void* data);

  // List used during GC to temporarily store id's of collected scripts.
  List<int> collected_scripts_;
};


// Linked list holding debug info objects. The debug info objects are kept as
// weak handles to avoid a debug info object to keep a function alive.
class DebugInfoListNode {
 public:
  explicit DebugInfoListNode(DebugInfo* debug_info);
  virtual ~DebugInfoListNode();

  DebugInfoListNode* next() { return next_; }
  void set_next(DebugInfoListNode* next) { next_ = next; }
  Handle<DebugInfo> debug_info() { return debug_info_; }

 private:
  // Global (weak) handle to the debug info object.
  Handle<DebugInfo> debug_info_;

  // Next pointer for linked list.
  DebugInfoListNode* next_;
};

// This class contains the debugger support. The main purpose is to handle
// setting break points in the code.
//
// This class controls the debug info for all functions which currently have
// active breakpoints in them. This debug info is held in the heap root object
// debug_info which is a FixedArray. Each entry in this list is of class
// DebugInfo.
class Debug {
 public:
  void SetUp(bool create_heap_objects);
  bool Load();
  void Unload();
  bool IsLoaded() { return !debug_context_.is_null(); }
  bool InDebugger() { return thread_local_.debugger_entry_ != NULL; }
  void PreemptionWhileInDebugger();
  void Iterate(ObjectVisitor* v);

  Object* Break(Arguments args);
  void SetBreakPoint(Handle<SharedFunctionInfo> shared,
                     Handle<Object> break_point_object,
                     int* source_position);
  void ClearBreakPoint(Handle<Object> break_point_object);
  void ClearAllBreakPoints();
  void FloodWithOneShot(Handle<SharedFunctionInfo> shared);
  void FloodHandlerWithOneShot();
  void ChangeBreakOnException(ExceptionBreakType type, bool enable);
  bool IsBreakOnException(ExceptionBreakType type);
  void PrepareStep(StepAction step_action, int step_count);
  void ClearStepping();
  bool StepNextContinue(BreakLocationIterator* break_location_iterator,
                        JavaScriptFrame* frame);
  static Handle<DebugInfo> GetDebugInfo(Handle<SharedFunctionInfo> shared);
  static bool HasDebugInfo(Handle<SharedFunctionInfo> shared);

  void PrepareForBreakPoints();

  // Returns whether the operation succeeded.
  bool EnsureDebugInfo(Handle<SharedFunctionInfo> shared);

  // Returns true if the current stub call is patched to call the debugger.
  static bool IsDebugBreak(Address addr);
  // Returns true if the current return statement has been patched to be
  // a debugger breakpoint.
  static bool IsDebugBreakAtReturn(RelocInfo* rinfo);

  // Check whether a code stub with the specified major key is a possible break
  // point location.
  static bool IsSourceBreakStub(Code* code);
  static bool IsBreakStub(Code* code);

  // Find the builtin to use for invoking the debug break
  static Handle<Code> FindDebugBreak(Handle<Code> code, RelocInfo::Mode mode);

  static Handle<Object> GetSourceBreakLocations(
      Handle<SharedFunctionInfo> shared);

  // Getter for the debug_context.
  inline Handle<Context> debug_context() { return debug_context_; }

  // Check whether a global object is the debug global object.
  bool IsDebugGlobal(GlobalObject* global);

  // Check whether this frame is just about to return.
  bool IsBreakAtReturn(JavaScriptFrame* frame);

  // Fast check to see if any break points are active.
  inline bool has_break_points() { return has_break_points_; }

  void NewBreak(StackFrame::Id break_frame_id);
  void SetBreak(StackFrame::Id break_frame_id, int break_id);
  StackFrame::Id break_frame_id() {
    return thread_local_.break_frame_id_;
  }
  int break_id() { return thread_local_.break_id_; }

  bool StepInActive() { return thread_local_.step_into_fp_ != 0; }
  void HandleStepIn(Handle<JSFunction> function,
                    Handle<Object> holder,
                    Address fp,
                    bool is_constructor);
  Address step_in_fp() { return thread_local_.step_into_fp_; }
  Address* step_in_fp_addr() { return &thread_local_.step_into_fp_; }

  bool StepOutActive() { return thread_local_.step_out_fp_ != 0; }
  Address step_out_fp() { return thread_local_.step_out_fp_; }

  EnterDebugger* debugger_entry() {
    return thread_local_.debugger_entry_;
  }
  void set_debugger_entry(EnterDebugger* entry) {
    thread_local_.debugger_entry_ = entry;
  }

  // Check whether any of the specified interrupts are pending.
  bool is_interrupt_pending(InterruptFlag what) {
    return (thread_local_.pending_interrupts_ & what) != 0;
  }

  // Set specified interrupts as pending.
  void set_interrupts_pending(InterruptFlag what) {
    thread_local_.pending_interrupts_ |= what;
  }

  // Clear specified interrupts from pending.
  void clear_interrupt_pending(InterruptFlag what) {
    thread_local_.pending_interrupts_ &= ~static_cast<int>(what);
  }

  // Getter and setter for the disable break state.
  bool disable_break() { return disable_break_; }
  void set_disable_break(bool disable_break) {
    disable_break_ = disable_break;
  }

  // Getters for the current exception break state.
  bool break_on_exception() { return break_on_exception_; }
  bool break_on_uncaught_exception() {
    return break_on_uncaught_exception_;
  }

  enum AddressId {
    k_after_break_target_address,
    k_debug_break_return_address,
    k_debug_break_slot_address,
    k_restarter_frame_function_pointer
  };

  // Support for setting the address to jump to when returning from break point.
  Address* after_break_target_address() {
    return reinterpret_cast<Address*>(&thread_local_.after_break_target_);
  }
  Address* restarter_frame_function_pointer_address() {
    Object*** address = &thread_local_.restarter_frame_function_pointer_;
    return reinterpret_cast<Address*>(address);
  }

  // Support for saving/restoring registers when handling debug break calls.
  Object** register_address(int r) {
    return &registers_[r];
  }

  // Access to the debug break on return code.
  Code* debug_break_return() { return debug_break_return_; }
  Code** debug_break_return_address() {
    return &debug_break_return_;
  }

  // Access to the debug break in debug break slot code.
  Code* debug_break_slot() { return debug_break_slot_; }
  Code** debug_break_slot_address() {
    return &debug_break_slot_;
  }

  static const int kEstimatedNofDebugInfoEntries = 16;
  static const int kEstimatedNofBreakPointsInFunction = 16;

  // Passed to MakeWeak.
  static void HandleWeakDebugInfo(v8::Persistent<v8::Value> obj, void* data);

  friend class Debugger;
  friend Handle<FixedArray> GetDebuggedFunctions();  // In test-debug.cc
  friend void CheckDebuggerUnloaded(bool check_functions);  // In test-debug.cc

  // Threading support.
  char* ArchiveDebug(char* to);
  char* RestoreDebug(char* from);
  static int ArchiveSpacePerThread();
  void FreeThreadResources() { }

  // Mirror cache handling.
  void ClearMirrorCache();

  // Script cache handling.
  void CreateScriptCache();
  void DestroyScriptCache();
  void AddScriptToScriptCache(Handle<Script> script);
  Handle<FixedArray> GetLoadedScripts();

  // Garbage collection notifications.
  void AfterGarbageCollection();

  // Code generator routines.
  static void GenerateSlot(MacroAssembler* masm);
  static void GenerateLoadICDebugBreak(MacroAssembler* masm);
  static void GenerateStoreICDebugBreak(MacroAssembler* masm);
  static void GenerateKeyedLoadICDebugBreak(MacroAssembler* masm);
  static void GenerateKeyedStoreICDebugBreak(MacroAssembler* masm);
  static void GenerateReturnDebugBreak(MacroAssembler* masm);
  static void GenerateCallFunctionStubDebugBreak(MacroAssembler* masm);
  static void GenerateCallFunctionStubRecordDebugBreak(MacroAssembler* masm);
  static void GenerateCallConstructStubDebugBreak(MacroAssembler* masm);
  static void GenerateCallConstructStubRecordDebugBreak(MacroAssembler* masm);
  static void GenerateSlotDebugBreak(MacroAssembler* masm);
  static void GeneratePlainReturnLiveEdit(MacroAssembler* masm);

  // FrameDropper is a code replacement for a JavaScript frame with possibly
  // several frames above.
  // There is no calling conventions here, because it never actually gets
  // called, it only gets returned to.
  static void GenerateFrameDropperLiveEdit(MacroAssembler* masm);

  // Called from stub-cache.cc.
  static void GenerateCallICDebugBreak(MacroAssembler* masm);

  // Describes how exactly a frame has been dropped from stack.
  enum FrameDropMode {
    // No frame has been dropped.
    FRAMES_UNTOUCHED,
    // The top JS frame had been calling IC stub. IC stub mustn't be called now.
    FRAME_DROPPED_IN_IC_CALL,
    // The top JS frame had been calling debug break slot stub. Patch the
    // address this stub jumps to in the end.
    FRAME_DROPPED_IN_DEBUG_SLOT_CALL,
    // The top JS frame had been calling some C++ function. The return address
    // gets patched automatically.
    FRAME_DROPPED_IN_DIRECT_CALL,
    FRAME_DROPPED_IN_RETURN_CALL
  };

  void FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                                    FrameDropMode mode,
                                    Object** restarter_frame_function_pointer);

  // Initializes an artificial stack frame. The data it contains is used for:
  //  a. successful work of frame dropper code which eventually gets control,
  //  b. being compatible with regular stack structure for various stack
  //     iterators.
  // Returns address of stack allocated pointer to restarted function,
  // the value that is called 'restarter_frame_function_pointer'. The value
  // at this address (possibly updated by GC) may be used later when preparing
  // 'step in' operation.
  static Object** SetUpFrameDropperFrame(StackFrame* bottom_js_frame,
                                         Handle<Code> code);

  static const int kFrameDropperFrameSize;

  // Architecture-specific constant.
  static const bool kFrameDropperSupported;

 private:
  explicit Debug(Isolate* isolate);
  ~Debug();

  static bool CompileDebuggerScript(int index);
  void ClearOneShot();
  void ActivateStepIn(StackFrame* frame);
  void ClearStepIn();
  void ActivateStepOut(StackFrame* frame);
  void ClearStepOut();
  void ClearStepNext();
  // Returns whether the compile succeeded.
  void RemoveDebugInfo(Handle<DebugInfo> debug_info);
  void SetAfterBreakTarget(JavaScriptFrame* frame);
  Handle<Object> CheckBreakPoints(Handle<Object> break_point);
  bool CheckBreakPoint(Handle<Object> break_point_object);

  // Global handle to debug context where all the debugger JavaScript code is
  // loaded.
  Handle<Context> debug_context_;

  // Boolean state indicating whether any break points are set.
  bool has_break_points_;

  // Cache of all scripts in the heap.
  ScriptCache* script_cache_;

  // List of active debug info objects.
  DebugInfoListNode* debug_info_list_;

  bool disable_break_;
  bool break_on_exception_;
  bool break_on_uncaught_exception_;

  // Per-thread data.
  class ThreadLocal {
   public:
    // Counter for generating next break id.
    int break_count_;

    // Current break id.
    int break_id_;

    // Frame id for the frame of the current break.
    StackFrame::Id break_frame_id_;

    // Step action for last step performed.
    StepAction last_step_action_;

    // Source statement position from last step next action.
    int last_statement_position_;

    // Number of steps left to perform before debug event.
    int step_count_;

    // Frame pointer from last step next action.
    Address last_fp_;

    // Number of queued steps left to perform before debug event.
    int queued_step_count_;

    // Frame pointer for frame from which step in was performed.
    Address step_into_fp_;

    // Frame pointer for the frame where debugger should be called when current
    // step out action is completed.
    Address step_out_fp_;

    // Storage location for jump when exiting debug break calls.
    Address after_break_target_;

    // Stores the way how LiveEdit has patched the stack. It is used when
    // debugger returns control back to user script.
    FrameDropMode frame_drop_mode_;

    // Top debugger entry.
    EnterDebugger* debugger_entry_;

    // Pending interrupts scheduled while debugging.
    int pending_interrupts_;

    // When restarter frame is on stack, stores the address
    // of the pointer to function being restarted. Otherwise (most of the time)
    // stores NULL. This pointer is used with 'step in' implementation.
    Object** restarter_frame_function_pointer_;
  };

  // Storage location for registers when handling debug break calls
  JSCallerSavedBuffer registers_;
  ThreadLocal thread_local_;
  void ThreadInit();

  // Code to call for handling debug break on return.
  Code* debug_break_return_;

  // Code to call for handling debug break in debug break slots.
  Code* debug_break_slot_;

  Isolate* isolate_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(Debug);
};


DECLARE_RUNTIME_FUNCTION(Object*, Debug_Break);


// Message delivered to the message handler callback. This is either a debugger
// event or the response to a command.
class MessageImpl: public v8::Debug::Message {
 public:
  // Create a message object for a debug event.
  static MessageImpl NewEvent(DebugEvent event,
                              bool running,
                              Handle<JSObject> exec_state,
                              Handle<JSObject> event_data);

  // Create a message object for the response to a debug command.
  static MessageImpl NewResponse(DebugEvent event,
                                 bool running,
                                 Handle<JSObject> exec_state,
                                 Handle<JSObject> event_data,
                                 Handle<String> response_json,
                                 v8::Debug::ClientData* client_data);

  // Implementation of interface v8::Debug::Message.
  virtual bool IsEvent() const;
  virtual bool IsResponse() const;
  virtual DebugEvent GetEvent() const;
  virtual bool WillStartRunning() const;
  virtual v8::Handle<v8::Object> GetExecutionState() const;
  virtual v8::Handle<v8::Object> GetEventData() const;
  virtual v8::Handle<v8::String> GetJSON() const;
  virtual v8::Handle<v8::Context> GetEventContext() const;
  virtual v8::Debug::ClientData* GetClientData() const;

 private:
  MessageImpl(bool is_event,
              DebugEvent event,
              bool running,
              Handle<JSObject> exec_state,
              Handle<JSObject> event_data,
              Handle<String> response_json,
              v8::Debug::ClientData* client_data);

  bool is_event_;  // Does this message represent a debug event?
  DebugEvent event_;  // Debug event causing the break.
  bool running_;  // Will the VM start running after this event?
  Handle<JSObject> exec_state_;  // Current execution state.
  Handle<JSObject> event_data_;  // Data associated with the event.
  Handle<String> response_json_;  // Response JSON if message holds a response.
  v8::Debug::ClientData* client_data_;  // Client data passed with the request.
};


// Details of the debug event delivered to the debug event listener.
class EventDetailsImpl : public v8::Debug::EventDetails {
 public:
  EventDetailsImpl(DebugEvent event,
                   Handle<JSObject> exec_state,
                   Handle<JSObject> event_data,
                   Handle<Object> callback_data,
                   v8::Debug::ClientData* client_data);
  virtual DebugEvent GetEvent() const;
  virtual v8::Handle<v8::Object> GetExecutionState() const;
  virtual v8::Handle<v8::Object> GetEventData() const;
  virtual v8::Handle<v8::Context> GetEventContext() const;
  virtual v8::Handle<v8::Value> GetCallbackData() const;
  virtual v8::Debug::ClientData* GetClientData() const;
 private:
  DebugEvent event_;  // Debug event causing the break.
  Handle<JSObject> exec_state_;         // Current execution state.
  Handle<JSObject> event_data_;         // Data associated with the event.
  Handle<Object> callback_data_;        // User data passed with the callback
                                        // when it was registered.
  v8::Debug::ClientData* client_data_;  // Data passed to DebugBreakForCommand.
};


// Message send by user to v8 debugger or debugger output message.
// In addition to command text it may contain a pointer to some user data
// which are expected to be passed along with the command reponse to message
// handler.
class CommandMessage {
 public:
  static CommandMessage New(const Vector<uint16_t>& command,
                            v8::Debug::ClientData* data);
  CommandMessage();
  ~CommandMessage();

  // Deletes user data and disposes of the text.
  void Dispose();
  Vector<uint16_t> text() const { return text_; }
  v8::Debug::ClientData* client_data() const { return client_data_; }
 private:
  CommandMessage(const Vector<uint16_t>& text,
                 v8::Debug::ClientData* data);

  Vector<uint16_t> text_;
  v8::Debug::ClientData* client_data_;
};

// A Queue of CommandMessage objects.  A thread-safe version is
// LockingCommandMessageQueue, based on this class.
class CommandMessageQueue BASE_EMBEDDED {
 public:
  explicit CommandMessageQueue(int size);
  ~CommandMessageQueue();
  bool IsEmpty() const { return start_ == end_; }
  CommandMessage Get();
  void Put(const CommandMessage& message);
  void Clear() { start_ = end_ = 0; }  // Queue is empty after Clear().
 private:
  // Doubles the size of the message queue, and copies the messages.
  void Expand();

  CommandMessage* messages_;
  int start_;
  int end_;
  int size_;  // The size of the queue buffer.  Queue can hold size-1 messages.
};


class MessageDispatchHelperThread;


// LockingCommandMessageQueue is a thread-safe circular buffer of CommandMessage
// messages.  The message data is not managed by LockingCommandMessageQueue.
// Pointers to the data are passed in and out. Implemented by adding a
// Mutex to CommandMessageQueue.  Includes logging of all puts and gets.
class LockingCommandMessageQueue BASE_EMBEDDED {
 public:
  LockingCommandMessageQueue(Logger* logger, int size);
  ~LockingCommandMessageQueue();
  bool IsEmpty() const;
  CommandMessage Get();
  void Put(const CommandMessage& message);
  void Clear();
 private:
  Logger* logger_;
  CommandMessageQueue queue_;
  Mutex* lock_;
  DISALLOW_COPY_AND_ASSIGN(LockingCommandMessageQueue);
};


class Debugger {
 public:
  ~Debugger();

  void DebugRequest(const uint16_t* json_request, int length);

  Handle<Object> MakeJSObject(Vector<const char> constructor_name,
                              int argc,
                              Handle<Object> argv[],
                              bool* caught_exception);
  Handle<Object> MakeExecutionState(bool* caught_exception);
  Handle<Object> MakeBreakEvent(Handle<Object> exec_state,
                                Handle<Object> break_points_hit,
                                bool* caught_exception);
  Handle<Object> MakeExceptionEvent(Handle<Object> exec_state,
                                    Handle<Object> exception,
                                    bool uncaught,
                                    bool* caught_exception);
  Handle<Object> MakeNewFunctionEvent(Handle<Object> func,
                                      bool* caught_exception);
  Handle<Object> MakeCompileEvent(Handle<Script> script,
                                  bool before,
                                  bool* caught_exception);
  Handle<Object> MakeScriptCollectedEvent(int id,
                                          bool* caught_exception);
  void OnDebugBreak(Handle<Object> break_points_hit, bool auto_continue);
  void OnException(Handle<Object> exception, bool uncaught);
  void OnBeforeCompile(Handle<Script> script);

  enum AfterCompileFlags {
    NO_AFTER_COMPILE_FLAGS,
    SEND_WHEN_DEBUGGING
  };
  void OnAfterCompile(Handle<Script> script,
                      AfterCompileFlags after_compile_flags);
  void OnNewFunction(Handle<JSFunction> fun);
  void OnScriptCollected(int id);
  void ProcessDebugEvent(v8::DebugEvent event,
                         Handle<JSObject> event_data,
                         bool auto_continue);
  void NotifyMessageHandler(v8::DebugEvent event,
                            Handle<JSObject> exec_state,
                            Handle<JSObject> event_data,
                            bool auto_continue);
  void SetEventListener(Handle<Object> callback, Handle<Object> data);
  void SetMessageHandler(v8::Debug::MessageHandler2 handler);
  void SetHostDispatchHandler(v8::Debug::HostDispatchHandler handler,
                              int period);
  void SetDebugMessageDispatchHandler(
      v8::Debug::DebugMessageDispatchHandler handler,
      bool provide_locker);

  // Invoke the message handler function.
  void InvokeMessageHandler(MessageImpl message);

  // Add a debugger command to the command queue.
  void ProcessCommand(Vector<const uint16_t> command,
                      v8::Debug::ClientData* client_data = NULL);

  // Check whether there are commands in the command queue.
  bool HasCommands();

  // Enqueue a debugger command to the command queue for event listeners.
  void EnqueueDebugCommand(v8::Debug::ClientData* client_data = NULL);

  Handle<Object> Call(Handle<JSFunction> fun,
                      Handle<Object> data,
                      bool* pending_exception);

  // Start the debugger agent listening on the provided port.
  bool StartAgent(const char* name, int port,
                  bool wait_for_connection = false);

  // Stop the debugger agent.
  void StopAgent();

  // Blocks until the agent has started listening for connections
  void WaitForAgent();

  void CallMessageDispatchHandler();

  Handle<Context> GetDebugContext();

  // Unload the debugger if possible. Only called when no debugger is currently
  // active.
  void UnloadDebugger();
  friend void ForceUnloadDebugger();  // In test-debug.cc

  inline bool EventActive(v8::DebugEvent event) {
    ScopedLock with(debugger_access_);

    // Check whether the message handler was been cleared.
    if (debugger_unload_pending_) {
      if (isolate_->debug()->debugger_entry() == NULL) {
        UnloadDebugger();
      }
    }

    if (((event == v8::BeforeCompile) || (event == v8::AfterCompile)) &&
        !FLAG_debug_compile_events) {
      return false;

    } else if ((event == v8::ScriptCollected) &&
               !FLAG_debug_script_collected_events) {
      return false;
    }

    // Currently argument event is not used.
    return !compiling_natives_ && Debugger::IsDebuggerActive();
  }

  void set_compiling_natives(bool compiling_natives) {
    compiling_natives_ = compiling_natives;
  }
  bool compiling_natives() const { return compiling_natives_; }
  void set_loading_debugger(bool v) { is_loading_debugger_ = v; }
  bool is_loading_debugger() const { return is_loading_debugger_; }
  void set_force_debugger_active(bool force_debugger_active) {
    force_debugger_active_ = force_debugger_active;
  }
  bool force_debugger_active() const { return force_debugger_active_; }

  bool IsDebuggerActive();

 private:
  explicit Debugger(Isolate* isolate);

  void CallEventCallback(v8::DebugEvent event,
                         Handle<Object> exec_state,
                         Handle<Object> event_data,
                         v8::Debug::ClientData* client_data);
  void CallCEventCallback(v8::DebugEvent event,
                          Handle<Object> exec_state,
                          Handle<Object> event_data,
                          v8::Debug::ClientData* client_data);
  void CallJSEventCallback(v8::DebugEvent event,
                           Handle<Object> exec_state,
                           Handle<Object> event_data);
  void ListenersChanged();

  Mutex* debugger_access_;  // Mutex guarding debugger variables.
  Handle<Object> event_listener_;  // Global handle to listener.
  Handle<Object> event_listener_data_;
  bool compiling_natives_;  // Are we compiling natives?
  bool is_loading_debugger_;  // Are we loading the debugger?
  bool never_unload_debugger_;  // Can we unload the debugger?
  bool force_debugger_active_;  // Activate debugger without event listeners.
  v8::Debug::MessageHandler2 message_handler_;
  bool debugger_unload_pending_;  // Was message handler cleared?
  v8::Debug::HostDispatchHandler host_dispatch_handler_;
  Mutex* dispatch_handler_access_;  // Mutex guarding dispatch handler.
  v8::Debug::DebugMessageDispatchHandler debug_message_dispatch_handler_;
  MessageDispatchHelperThread* message_dispatch_helper_thread_;
  int host_dispatch_micros_;

  DebuggerAgent* agent_;

  static const int kQueueInitialSize = 4;
  LockingCommandMessageQueue command_queue_;
  Semaphore* command_received_;  // Signaled for each command received.
  LockingCommandMessageQueue event_command_queue_;

  Isolate* isolate_;

  friend class EnterDebugger;
  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(Debugger);
};


// This class is used for entering the debugger. Create an instance in the stack
// to enter the debugger. This will set the current break state, make sure the
// debugger is loaded and switch to the debugger context. If the debugger for
// some reason could not be entered FailedToEnter will return true.
class EnterDebugger BASE_EMBEDDED {
 public:
  EnterDebugger();
  ~EnterDebugger();

  // Check whether the debugger could be entered.
  inline bool FailedToEnter() { return load_failed_; }

  // Check whether there are any JavaScript frames on the stack.
  inline bool HasJavaScriptFrames() { return has_js_frames_; }

  // Get the active context from before entering the debugger.
  inline Handle<Context> GetContext() { return save_.context(); }

 private:
  Isolate* isolate_;
  EnterDebugger* prev_;  // Previous debugger entry if entered recursively.
  JavaScriptFrameIterator it_;
  const bool has_js_frames_;  // Were there any JavaScript frames?
  StackFrame::Id break_frame_id_;  // Previous break frame id.
  int break_id_;  // Previous break id.
  bool load_failed_;  // Did the debugger fail to load?
  SaveContext save_;  // Saves previous context.
};


// Stack allocated class for disabling break.
class DisableBreak BASE_EMBEDDED {
 public:
  explicit DisableBreak(bool disable_break) : isolate_(Isolate::Current()) {
    prev_disable_break_ = isolate_->debug()->disable_break();
    isolate_->debug()->set_disable_break(disable_break);
  }
  ~DisableBreak() {
    ASSERT(Isolate::Current() == isolate_);
    isolate_->debug()->set_disable_break(prev_disable_break_);
  }

 private:
  Isolate* isolate_;
  // The previous state of the disable break used to restore the value when this
  // object is destructed.
  bool prev_disable_break_;
};


// Debug_Address encapsulates the Address pointers used in generating debug
// code.
class Debug_Address {
 public:
  explicit Debug_Address(Debug::AddressId id) : id_(id) { }

  static Debug_Address AfterBreakTarget() {
    return Debug_Address(Debug::k_after_break_target_address);
  }

  static Debug_Address DebugBreakReturn() {
    return Debug_Address(Debug::k_debug_break_return_address);
  }

  static Debug_Address RestarterFrameFunctionPointer() {
    return Debug_Address(Debug::k_restarter_frame_function_pointer);
  }

  Address address(Isolate* isolate) const {
    Debug* debug = isolate->debug();
    switch (id_) {
      case Debug::k_after_break_target_address:
        return reinterpret_cast<Address>(debug->after_break_target_address());
      case Debug::k_debug_break_return_address:
        return reinterpret_cast<Address>(debug->debug_break_return_address());
      case Debug::k_debug_break_slot_address:
        return reinterpret_cast<Address>(debug->debug_break_slot_address());
      case Debug::k_restarter_frame_function_pointer:
        return reinterpret_cast<Address>(
            debug->restarter_frame_function_pointer_address());
      default:
        UNREACHABLE();
        return NULL;
    }
  }

 private:
  Debug::AddressId id_;
};

// The optional thread that Debug Agent may use to temporary call V8 to process
// pending debug requests if debuggee is not running V8 at the moment.
// Techincally it does not call V8 itself, rather it asks embedding program
// to do this via v8::Debug::HostDispatchHandler
class MessageDispatchHelperThread: public Thread {
 public:
  explicit MessageDispatchHelperThread(Isolate* isolate);
  ~MessageDispatchHelperThread();

  void Schedule();

 private:
  void Run();

  Semaphore* const sem_;
  Mutex* const mutex_;
  bool already_signalled_;

  DISALLOW_COPY_AND_ASSIGN(MessageDispatchHelperThread);
};


} }  // namespace v8::internal

#endif  // ENABLE_DEBUGGER_SUPPORT

#endif  // V8_DEBUG_H_
