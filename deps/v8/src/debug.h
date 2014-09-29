// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_H_
#define V8_DEBUG_H_

#include "src/allocation.h"
#include "src/arguments.h"
#include "src/assembler.h"
#include "src/base/platform/platform.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/flags.h"
#include "src/frames-inl.h"
#include "src/hashmap.h"
#include "src/liveedit.h"
#include "src/string-stream.h"
#include "src/v8threads.h"

#include "include/v8-debug.h"

namespace v8 {
namespace internal {


// Forward declarations.
class DebugScope;


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


// The different types of breakpoint position alignments.
// Must match Debug.BreakPositionAlignment in debug-debugger.js
enum BreakPositionAlignment {
  STATEMENT_ALIGNED = 0,
  BREAK_POSITION_ALIGNED = 1
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
  void FindBreakLocationFromPosition(int position,
      BreakPositionAlignment alignment);
  void Reset();
  bool Done() const;
  void SetBreakPoint(Handle<Object> break_point_object);
  void ClearBreakPoint(Handle<Object> break_point_object);
  void SetOneShot();
  void ClearOneShot();
  bool IsStepInLocation(Isolate* isolate);
  void PrepareStepIn(Isolate* isolate);
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
  explicit ScriptCache(Isolate* isolate);
  virtual ~ScriptCache() { Clear(); }

  // Add script to the cache.
  void Add(Handle<Script> script);

  // Return the scripts in the cache.
  Handle<FixedArray> GetScripts();

 private:
  // Calculate the hash value from the key (script id).
  static uint32_t Hash(int key) {
    return ComputeIntegerHash(key, v8::internal::kZeroHashSeed);
  }

  // Clear the cache releasing all the weak handles.
  void Clear();

  // Weak handle callback for scripts in the cache.
  static void HandleWeakScript(
      const v8::WeakCallbackData<v8::Value, void>& data);

  Isolate* isolate_;
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
  virtual v8::Isolate* GetIsolate() const;

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


// LockingCommandMessageQueue is a thread-safe circular buffer of CommandMessage
// messages.  The message data is not managed by LockingCommandMessageQueue.
// Pointers to the data are passed in and out. Implemented by adding a
// Mutex to CommandMessageQueue.  Includes logging of all puts and gets.
class LockingCommandMessageQueue BASE_EMBEDDED {
 public:
  LockingCommandMessageQueue(Logger* logger, int size);
  bool IsEmpty() const;
  CommandMessage Get();
  void Put(const CommandMessage& message);
  void Clear();
 private:
  Logger* logger_;
  CommandMessageQueue queue_;
  mutable base::Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(LockingCommandMessageQueue);
};


class PromiseOnStack {
 public:
  PromiseOnStack(Isolate* isolate, PromiseOnStack* prev,
                 Handle<JSObject> getter);
  ~PromiseOnStack();
  StackHandler* handler() { return handler_; }
  Handle<JSObject> promise() { return promise_; }
  PromiseOnStack* prev() { return prev_; }
 private:
  Isolate* isolate_;
  StackHandler* handler_;
  Handle<JSObject> promise_;
  PromiseOnStack* prev_;
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
  // Debug event triggers.
  void OnDebugBreak(Handle<Object> break_points_hit, bool auto_continue);

  void OnThrow(Handle<Object> exception, bool uncaught);
  void OnPromiseReject(Handle<JSObject> promise, Handle<Object> value);
  void OnCompileError(Handle<Script> script);
  void OnBeforeCompile(Handle<Script> script);
  void OnAfterCompile(Handle<Script> script);
  void OnPromiseEvent(Handle<JSObject> data);
  void OnAsyncTaskEvent(Handle<JSObject> data);

  // API facing.
  void SetEventListener(Handle<Object> callback, Handle<Object> data);
  void SetMessageHandler(v8::Debug::MessageHandler handler);
  void EnqueueCommandMessage(Vector<const uint16_t> command,
                             v8::Debug::ClientData* client_data = NULL);
  // Enqueue a debugger command to the command queue for event listeners.
  void EnqueueDebugCommand(v8::Debug::ClientData* client_data = NULL);
  MUST_USE_RESULT MaybeHandle<Object> Call(Handle<JSFunction> fun,
                                           Handle<Object> data);
  Handle<Context> GetDebugContext();
  void HandleDebugBreak();
  void ProcessDebugMessages(bool debug_command_only);

  // Internal logic
  bool Load();
  void Break(Arguments args, JavaScriptFrame*);
  void SetAfterBreakTarget(JavaScriptFrame* frame);

  // Scripts handling.
  Handle<FixedArray> GetLoadedScripts();

  // Break point handling.
  bool SetBreakPoint(Handle<JSFunction> function,
                     Handle<Object> break_point_object,
                     int* source_position);
  bool SetBreakPointForScript(Handle<Script> script,
                              Handle<Object> break_point_object,
                              int* source_position,
                              BreakPositionAlignment alignment);
  void ClearBreakPoint(Handle<Object> break_point_object);
  void ClearAllBreakPoints();
  void FloodWithOneShot(Handle<JSFunction> function);
  void FloodBoundFunctionWithOneShot(Handle<JSFunction> function);
  void FloodHandlerWithOneShot();
  void ChangeBreakOnException(ExceptionBreakType type, bool enable);
  bool IsBreakOnException(ExceptionBreakType type);

  // Stepping handling.
  void PrepareStep(StepAction step_action,
                   int step_count,
                   StackFrame::Id frame_id);
  void ClearStepping();
  void ClearStepOut();
  bool IsStepping() { return thread_local_.step_count_ > 0; }
  bool StepNextContinue(BreakLocationIterator* break_location_iterator,
                        JavaScriptFrame* frame);
  bool StepInActive() { return thread_local_.step_into_fp_ != 0; }
  void HandleStepIn(Handle<JSFunction> function,
                    Handle<Object> holder,
                    Address fp,
                    bool is_constructor);
  bool StepOutActive() { return thread_local_.step_out_fp_ != 0; }

  // Purge all code objects that have no debug break slots.
  void PrepareForBreakPoints();

  // Returns whether the operation succeeded. Compilation can only be triggered
  // if a valid closure is passed as the second argument, otherwise the shared
  // function needs to be compiled already.
  bool EnsureDebugInfo(Handle<SharedFunctionInfo> shared,
                       Handle<JSFunction> function);
  static Handle<DebugInfo> GetDebugInfo(Handle<SharedFunctionInfo> shared);
  static bool HasDebugInfo(Handle<SharedFunctionInfo> shared);

  // This function is used in FunctionNameUsing* tests.
  Object* FindSharedFunctionInfoInScript(Handle<Script> script, int position);

  // Returns true if the current stub call is patched to call the debugger.
  static bool IsDebugBreak(Address addr);
  // Returns true if the current return statement has been patched to be
  // a debugger breakpoint.
  static bool IsDebugBreakAtReturn(RelocInfo* rinfo);

  static Handle<Object> GetSourceBreakLocations(
      Handle<SharedFunctionInfo> shared,
      BreakPositionAlignment position_aligment);

  // Check whether a global object is the debug global object.
  bool IsDebugGlobal(GlobalObject* global);

  // Check whether this frame is just about to return.
  bool IsBreakAtReturn(JavaScriptFrame* frame);

  // Promise handling.
  // Push and pop a promise and the current try-catch handler.
  void PushPromise(Handle<JSObject> promise);
  void PopPromise();

  // Support for LiveEdit
  void FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                             LiveEdit::FrameDropMode mode,
                             Object** restarter_frame_function_pointer);

  // Passed to MakeWeak.
  static void HandleWeakDebugInfo(
      const v8::WeakCallbackData<v8::Value, void>& data);

  // Threading support.
  char* ArchiveDebug(char* to);
  char* RestoreDebug(char* from);
  static int ArchiveSpacePerThread();
  void FreeThreadResources() { }

  // Record function from which eval was called.
  static void RecordEvalCaller(Handle<Script> script);

  // Flags and states.
  DebugScope* debugger_entry() { return thread_local_.current_debug_scope_; }
  inline Handle<Context> debug_context() { return debug_context_; }
  void set_live_edit_enabled(bool v) { live_edit_enabled_ = v; }
  bool live_edit_enabled() const {
    return FLAG_enable_liveedit && live_edit_enabled_ ;
  }

  inline bool is_active() const { return is_active_; }
  inline bool is_loaded() const { return !debug_context_.is_null(); }
  inline bool has_break_points() const { return has_break_points_; }
  inline bool in_debug_scope() const {
    return thread_local_.current_debug_scope_ != NULL;
  }
  void set_disable_break(bool v) { break_disabled_ = v; }

  StackFrame::Id break_frame_id() { return thread_local_.break_frame_id_; }
  int break_id() { return thread_local_.break_id_; }

  // Support for embedding into generated code.
  Address is_active_address() {
    return reinterpret_cast<Address>(&is_active_);
  }

  Address after_break_target_address() {
    return reinterpret_cast<Address>(&after_break_target_);
  }

  Address restarter_frame_function_pointer_address() {
    Object*** address = &thread_local_.restarter_frame_function_pointer_;
    return reinterpret_cast<Address>(address);
  }

  Address step_in_fp_addr() {
    return reinterpret_cast<Address>(&thread_local_.step_into_fp_);
  }

 private:
  explicit Debug(Isolate* isolate);

  void UpdateState();
  void Unload();
  void SetNextBreakId() {
    thread_local_.break_id_ = ++thread_local_.break_count_;
  }

  // Check whether there are commands in the command queue.
  inline bool has_commands() const { return !command_queue_.IsEmpty(); }
  inline bool ignore_events() const { return is_suppressed_ || !is_active_; }

  void OnException(Handle<Object> exception, bool uncaught,
                   Handle<Object> promise);

  // Constructors for debug event objects.
  MUST_USE_RESULT MaybeHandle<Object> MakeJSObject(
      const char* constructor_name,
      int argc,
      Handle<Object> argv[]);
  MUST_USE_RESULT MaybeHandle<Object> MakeExecutionState();
  MUST_USE_RESULT MaybeHandle<Object> MakeBreakEvent(
      Handle<Object> break_points_hit);
  MUST_USE_RESULT MaybeHandle<Object> MakeExceptionEvent(
      Handle<Object> exception,
      bool uncaught,
      Handle<Object> promise);
  MUST_USE_RESULT MaybeHandle<Object> MakeCompileEvent(
      Handle<Script> script, v8::DebugEvent type);
  MUST_USE_RESULT MaybeHandle<Object> MakePromiseEvent(
      Handle<JSObject> promise_event);
  MUST_USE_RESULT MaybeHandle<Object> MakeAsyncTaskEvent(
      Handle<JSObject> task_event);

  // Mirror cache handling.
  void ClearMirrorCache();

  // Returns a promise if the pushed try-catch handler matches the current one.
  Handle<Object> GetPromiseOnStackOnThrow();
  bool PromiseHasRejectHandler(Handle<JSObject> promise);

  void CallEventCallback(v8::DebugEvent event,
                         Handle<Object> exec_state,
                         Handle<Object> event_data,
                         v8::Debug::ClientData* client_data);
  void ProcessDebugEvent(v8::DebugEvent event,
                         Handle<JSObject> event_data,
                         bool auto_continue);
  void NotifyMessageHandler(v8::DebugEvent event,
                            Handle<JSObject> exec_state,
                            Handle<JSObject> event_data,
                            bool auto_continue);
  void InvokeMessageHandler(MessageImpl message);

  static bool CompileDebuggerScript(Isolate* isolate, int index);
  void ClearOneShot();
  void ActivateStepIn(StackFrame* frame);
  void ClearStepIn();
  void ActivateStepOut(StackFrame* frame);
  void ClearStepNext();
  // Returns whether the compile succeeded.
  void RemoveDebugInfo(Handle<DebugInfo> debug_info);
  Handle<Object> CheckBreakPoints(Handle<Object> break_point);
  bool CheckBreakPoint(Handle<Object> break_point_object);

  inline void AssertDebugContext() {
    DCHECK(isolate_->context() == *debug_context());
    DCHECK(in_debug_scope());
  }

  void ThreadInit();

  // Global handles.
  Handle<Context> debug_context_;
  Handle<Object> event_listener_;
  Handle<Object> event_listener_data_;

  v8::Debug::MessageHandler message_handler_;

  static const int kQueueInitialSize = 4;
  base::Semaphore command_received_;  // Signaled for each command received.
  LockingCommandMessageQueue command_queue_;
  LockingCommandMessageQueue event_command_queue_;

  bool is_active_;
  bool is_suppressed_;
  bool live_edit_enabled_;
  bool has_break_points_;
  bool break_disabled_;
  bool break_on_exception_;
  bool break_on_uncaught_exception_;

  ScriptCache* script_cache_;  // Cache of all scripts in the heap.
  DebugInfoListNode* debug_info_list_;  // List of active debug info objects.

  // Storage location for jump when exiting debug break calls.
  // Note that this address is not GC safe.  It should be computed immediately
  // before returning to the DebugBreakCallHelper.
  Address after_break_target_;

  // Per-thread data.
  class ThreadLocal {
   public:
    // Top debugger entry.
    DebugScope* current_debug_scope_;

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

    // Stores the way how LiveEdit has patched the stack. It is used when
    // debugger returns control back to user script.
    LiveEdit::FrameDropMode frame_drop_mode_;

    // When restarter frame is on stack, stores the address
    // of the pointer to function being restarted. Otherwise (most of the time)
    // stores NULL. This pointer is used with 'step in' implementation.
    Object** restarter_frame_function_pointer_;

    // When a promise is being resolved, we may want to trigger a debug event
    // if we catch a throw.  For this purpose we remember the try-catch
    // handler address that would catch the exception.  We also hold onto a
    // closure that returns a promise if the exception is considered uncaught.
    // Due to the possibility of reentry we use a linked list.
    PromiseOnStack* promise_on_stack_;
  };

  // Storage location for registers when handling debug break calls
  ThreadLocal thread_local_;

  Isolate* isolate_;

  friend class Isolate;
  friend class DebugScope;
  friend class DisableBreak;
  friend class LiveEdit;
  friend class SuppressDebug;

  friend Handle<FixedArray> GetDebuggedFunctions();  // In test-debug.cc
  friend void CheckDebuggerUnloaded(bool check_functions);  // In test-debug.cc

  DISALLOW_COPY_AND_ASSIGN(Debug);
};


DECLARE_RUNTIME_FUNCTION(Debug_Break);


// This scope is used to load and enter the debug context and create a new
// break state.  Leaving the scope will restore the previous state.
// On failure to load, FailedToEnter returns true.
class DebugScope BASE_EMBEDDED {
 public:
  explicit DebugScope(Debug* debug);
  ~DebugScope();

  // Check whether loading was successful.
  inline bool failed() { return failed_; }

  // Get the active context from before entering the debugger.
  inline Handle<Context> GetContext() { return save_.context(); }

 private:
  Isolate* isolate() { return debug_->isolate_; }

  Debug* debug_;
  DebugScope* prev_;               // Previous scope if entered recursively.
  StackFrame::Id break_frame_id_;  // Previous break frame id.
  int break_id_;                   // Previous break id.
  bool failed_;                    // Did the debug context fail to load?
  SaveContext save_;               // Saves previous context.
  PostponeInterruptsScope no_termination_exceptons_;
};


// Stack allocated class for disabling break.
class DisableBreak BASE_EMBEDDED {
 public:
  explicit DisableBreak(Debug* debug, bool disable_break)
    : debug_(debug), old_state_(debug->break_disabled_) {
    debug_->break_disabled_ = disable_break;
  }
  ~DisableBreak() { debug_->break_disabled_ = old_state_; }

 private:
  Debug* debug_;
  bool old_state_;
  DISALLOW_COPY_AND_ASSIGN(DisableBreak);
};


class SuppressDebug BASE_EMBEDDED {
 public:
  explicit SuppressDebug(Debug* debug)
      : debug_(debug), old_state_(debug->is_suppressed_) {
    debug_->is_suppressed_ = true;
  }
  ~SuppressDebug() { debug_->is_suppressed_ = old_state_; }

 private:
  Debug* debug_;
  bool old_state_;
  DISALLOW_COPY_AND_ASSIGN(SuppressDebug);
};


// Code generator routines.
class DebugCodegen : public AllStatic {
 public:
  static void GenerateSlot(MacroAssembler* masm);
  static void GenerateCallICStubDebugBreak(MacroAssembler* masm);
  static void GenerateLoadICDebugBreak(MacroAssembler* masm);
  static void GenerateStoreICDebugBreak(MacroAssembler* masm);
  static void GenerateKeyedLoadICDebugBreak(MacroAssembler* masm);
  static void GenerateKeyedStoreICDebugBreak(MacroAssembler* masm);
  static void GenerateCompareNilICDebugBreak(MacroAssembler* masm);
  static void GenerateReturnDebugBreak(MacroAssembler* masm);
  static void GenerateCallFunctionStubDebugBreak(MacroAssembler* masm);
  static void GenerateCallConstructStubDebugBreak(MacroAssembler* masm);
  static void GenerateCallConstructStubRecordDebugBreak(MacroAssembler* masm);
  static void GenerateSlotDebugBreak(MacroAssembler* masm);
  static void GeneratePlainReturnLiveEdit(MacroAssembler* masm);

  // FrameDropper is a code replacement for a JavaScript frame with possibly
  // several frames above.
  // There is no calling conventions here, because it never actually gets
  // called, it only gets returned to.
  static void GenerateFrameDropperLiveEdit(MacroAssembler* masm);
};


} }  // namespace v8::internal

#endif  // V8_DEBUG_H_
