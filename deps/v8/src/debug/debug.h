// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_H_
#define V8_DEBUG_DEBUG_H_

#include "src/allocation.h"
#include "src/arguments.h"
#include "src/assembler.h"
#include "src/base/atomicops.h"
#include "src/base/platform/platform.h"
#include "src/debug/liveedit.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/flags.h"
#include "src/frames.h"
#include "src/hashmap.h"
#include "src/runtime/runtime.h"
#include "src/string-stream.h"
#include "src/v8threads.h"

#include "include/v8-debug.h"

namespace v8 {
namespace internal {


// Forward declarations.
class DebugScope;


// Step actions. NOTE: These values are in macros.py as well.
enum StepAction : int8_t {
  StepNone = -1,  // Stepping not prepared.
  StepOut = 0,    // Step out of the current function.
  StepNext = 1,   // Step to the next statement in the current function.
  StepIn = 2,     // Step into new functions invoked or the next statement
                  // in the current function.
  StepFrame = 3   // Step into a new frame or return to previous frame.
};


// Type of exception break. NOTE: These values are in macros.py as well.
enum ExceptionBreakType {
  BreakException = 0,
  BreakUncaughtException = 1
};


// Type of exception break.
enum BreakLocatorType { ALL_BREAK_LOCATIONS, CALLS_AND_RETURNS };


// The different types of breakpoint position alignments.
// Must match Debug.BreakPositionAlignment in debug.js
enum BreakPositionAlignment {
  STATEMENT_ALIGNED = 0,
  BREAK_POSITION_ALIGNED = 1
};


class BreakLocation {
 public:
  // Find the break point at the supplied address, or the closest one before
  // the address.
  static BreakLocation FromAddress(Handle<DebugInfo> debug_info, Address pc);

  static void FromAddressSameStatement(Handle<DebugInfo> debug_info, Address pc,
                                       List<BreakLocation>* result_out);

  static BreakLocation FromPosition(Handle<DebugInfo> debug_info, int position,
                                    BreakPositionAlignment alignment);

  bool IsDebugBreak() const;

  inline bool IsReturn() const {
    return RelocInfo::IsDebugBreakSlotAtReturn(rmode_);
  }
  inline bool IsCall() const {
    return RelocInfo::IsDebugBreakSlotAtCall(rmode_);
  }
  inline bool HasBreakPoint() const {
    return debug_info_->HasBreakPoint(pc_offset_);
  }

  Handle<Object> BreakPointObjects() const;

  void SetBreakPoint(Handle<Object> break_point_object);
  void ClearBreakPoint(Handle<Object> break_point_object);

  void SetOneShot();
  void ClearOneShot();


  inline RelocInfo rinfo() const {
    return RelocInfo(debug_info_->GetIsolate(), pc(), rmode(), data_, code());
  }

  inline int position() const { return position_; }
  inline int statement_position() const { return statement_position_; }

  inline Address pc() const { return code()->entry() + pc_offset_; }

  inline RelocInfo::Mode rmode() const { return rmode_; }

  inline Code* code() const { return debug_info_->code(); }

 private:
  BreakLocation(Handle<DebugInfo> debug_info, RelocInfo* rinfo, int position,
                int statement_position);

  class Iterator {
   public:
    Iterator(Handle<DebugInfo> debug_info, BreakLocatorType type);

    BreakLocation GetBreakLocation() {
      return BreakLocation(debug_info_, rinfo(), position(),
                           statement_position());
    }

    inline bool Done() const { return reloc_iterator_.done(); }
    void Next();

    void SkipTo(int count) {
      while (count-- > 0) Next();
    }

    inline RelocInfo::Mode rmode() { return reloc_iterator_.rinfo()->rmode(); }
    inline RelocInfo* rinfo() { return reloc_iterator_.rinfo(); }
    inline Address pc() { return rinfo()->pc(); }
    int break_index() const { return break_index_; }
    inline int position() const { return position_; }
    inline int statement_position() const { return statement_position_; }

   private:
    static int GetModeMask(BreakLocatorType type);

    Handle<DebugInfo> debug_info_;
    RelocIterator reloc_iterator_;
    int break_index_;
    int position_;
    int statement_position_;

    DisallowHeapAllocation no_gc_;

    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  friend class Debug;

  static int BreakIndexFromAddress(Handle<DebugInfo> debug_info, Address pc);

  void SetDebugBreak();
  void ClearDebugBreak();

  inline bool IsDebuggerStatement() const {
    return RelocInfo::IsDebuggerStatement(rmode_);
  }
  inline bool IsDebugBreakSlot() const {
    return RelocInfo::IsDebugBreakSlot(rmode_);
  }

  Handle<DebugInfo> debug_info_;
  int pc_offset_;
  RelocInfo::Mode rmode_;
  intptr_t data_;
  int position_;
  int statement_position_;
};


// Linked list holding debug info objects. The debug info objects are kept as
// weak handles to avoid a debug info object to keep a function alive.
class DebugInfoListNode {
 public:
  explicit DebugInfoListNode(DebugInfo* debug_info);
  ~DebugInfoListNode();

  DebugInfoListNode* next() { return next_; }
  void set_next(DebugInfoListNode* next) { next_ = next; }
  Handle<DebugInfo> debug_info() { return Handle<DebugInfo>(debug_info_); }

 private:
  // Global (weak) handle to the debug info object.
  DebugInfo** debug_info_;

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
  virtual v8::Local<v8::Object> GetExecutionState() const;
  virtual v8::Local<v8::Object> GetEventData() const;
  virtual v8::Local<v8::String> GetJSON() const;
  virtual v8::Local<v8::Context> GetEventContext() const;
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
  virtual v8::Local<v8::Object> GetExecutionState() const;
  virtual v8::Local<v8::Object> GetEventData() const;
  virtual v8::Local<v8::Context> GetEventContext() const;
  virtual v8::Local<v8::Value> GetCallbackData() const;
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


class DebugFeatureTracker {
 public:
  enum Feature {
    kActive = 1,
    kBreakPoint = 2,
    kStepping = 3,
    kHeapSnapshot = 4,
    kAllocationTracking = 5,
    kProfiler = 6,
    kLiveEdit = 7,
  };

  explicit DebugFeatureTracker(Isolate* isolate)
      : isolate_(isolate), bitfield_(0) {}
  void Track(Feature feature);

 private:
  Isolate* isolate_;
  uint32_t bitfield_;
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

  void OnThrow(Handle<Object> exception);
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
  MUST_USE_RESULT MaybeHandle<Object> Call(Handle<Object> fun,
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
  void FloodWithOneShot(Handle<JSFunction> function,
                        BreakLocatorType type = ALL_BREAK_LOCATIONS);
  void ChangeBreakOnException(ExceptionBreakType type, bool enable);
  bool IsBreakOnException(ExceptionBreakType type);

  // Stepping handling.
  void PrepareStep(StepAction step_action);
  void PrepareStepIn(Handle<JSFunction> function);
  void PrepareStepOnThrow();
  void ClearStepping();
  void ClearStepOut();
  void EnableStepIn();

  void GetStepinPositions(JavaScriptFrame* frame, StackFrame::Id frame_id,
                          List<int>* results_out);

  bool PrepareFunctionForBreakPoints(Handle<SharedFunctionInfo> shared);

  // Returns whether the operation succeeded. Compilation can only be triggered
  // if a valid closure is passed as the second argument, otherwise the shared
  // function needs to be compiled already.
  bool EnsureDebugInfo(Handle<SharedFunctionInfo> shared,
                       Handle<JSFunction> function);
  void CreateDebugInfo(Handle<SharedFunctionInfo> shared);
  static Handle<DebugInfo> GetDebugInfo(Handle<SharedFunctionInfo> shared);

  template <typename C>
  bool CompileToRevealInnerFunctions(C* compilable);

  // This function is used in FunctionNameUsing* tests.
  Handle<Object> FindSharedFunctionInfoInScript(Handle<Script> script,
                                                int position);

  static Handle<Object> GetSourceBreakLocations(
      Handle<SharedFunctionInfo> shared,
      BreakPositionAlignment position_aligment);

  // Check whether a global object is the debug global object.
  bool IsDebugGlobal(JSGlobalObject* global);

  // Check whether this frame is just about to return.
  bool IsBreakAtReturn(JavaScriptFrame* frame);

  // Support for LiveEdit
  void FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                             LiveEdit::FrameDropMode mode);

  // Threading support.
  char* ArchiveDebug(char* to);
  char* RestoreDebug(char* from);
  static int ArchiveSpacePerThread();
  void FreeThreadResources() { }

  // Record function from which eval was called.
  static void RecordEvalCaller(Handle<Script> script);

  bool CheckExecutionState(int id) {
    return is_active() && !debug_context().is_null() && break_id() != 0 &&
           break_id() == id;
  }

  // Flags and states.
  DebugScope* debugger_entry() {
    return reinterpret_cast<DebugScope*>(
        base::NoBarrier_Load(&thread_local_.current_debug_scope_));
  }
  inline Handle<Context> debug_context() { return debug_context_; }

  void set_live_edit_enabled(bool v) { live_edit_enabled_ = v; }
  bool live_edit_enabled() const {
    return FLAG_enable_liveedit && live_edit_enabled_;
  }

  inline bool is_active() const { return is_active_; }
  inline bool is_loaded() const { return !debug_context_.is_null(); }
  inline bool in_debug_scope() const {
    return !!base::NoBarrier_Load(&thread_local_.current_debug_scope_);
  }
  void set_break_points_active(bool v) { break_points_active_ = v; }
  bool break_points_active() const { return break_points_active_; }

  StackFrame::Id break_frame_id() { return thread_local_.break_frame_id_; }
  int break_id() { return thread_local_.break_id_; }

  // Support for embedding into generated code.
  Address is_active_address() {
    return reinterpret_cast<Address>(&is_active_);
  }

  Address after_break_target_address() {
    return reinterpret_cast<Address>(&after_break_target_);
  }

  Address step_in_enabled_address() {
    return reinterpret_cast<Address>(&thread_local_.step_in_enabled_);
  }

  StepAction last_step_action() { return thread_local_.last_step_action_; }

  DebugFeatureTracker* feature_tracker() { return &feature_tracker_; }

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
  inline bool break_disabled() const {
    return break_disabled_ || in_debug_event_listener_;
  }

  void OnException(Handle<Object> exception, Handle<Object> promise);

  // Constructors for debug event objects.
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

  MaybeHandle<Object> PromiseHasUserDefinedRejectHandler(
      Handle<JSObject> promise);

  void CallEventCallback(v8::DebugEvent event,
                         Handle<Object> exec_state,
                         Handle<Object> event_data,
                         v8::Debug::ClientData* client_data);
  void ProcessCompileEvent(v8::DebugEvent event, Handle<Script> script);
  void ProcessDebugEvent(v8::DebugEvent event,
                         Handle<JSObject> event_data,
                         bool auto_continue);
  void NotifyMessageHandler(v8::DebugEvent event,
                            Handle<JSObject> exec_state,
                            Handle<JSObject> event_data,
                            bool auto_continue);
  void InvokeMessageHandler(MessageImpl message);

  void ClearOneShot();
  void ActivateStepOut(StackFrame* frame);
  void RemoveDebugInfoAndClearFromShared(Handle<DebugInfo> debug_info);
  Handle<Object> CheckBreakPoints(Handle<Object> break_point);
  bool CheckBreakPoint(Handle<Object> break_point_object);
  MaybeHandle<Object> CallFunction(const char* name, int argc,
                                   Handle<Object> args[]);

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

  bool is_active_;
  bool is_suppressed_;
  bool live_edit_enabled_;
  bool break_disabled_;
  bool break_points_active_;
  bool in_debug_event_listener_;
  bool break_on_exception_;
  bool break_on_uncaught_exception_;

  DebugInfoListNode* debug_info_list_;  // List of active debug info objects.

  // Storage location for jump when exiting debug break calls.
  // Note that this address is not GC safe.  It should be computed immediately
  // before returning to the DebugBreakCallHelper.
  Address after_break_target_;

  // Used to collect histogram data on debugger feature usage.
  DebugFeatureTracker feature_tracker_;

  // Per-thread data.
  class ThreadLocal {
   public:
    // Top debugger entry.
    base::AtomicWord current_debug_scope_;

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

    // Frame pointer from last step next or step frame action.
    Address last_fp_;

    // Frame pointer of the target frame we want to arrive at.
    Address target_fp_;

    // Whether functions are flooded on entry for step-in and step-frame.
    // If we stepped out to the embedder, disable flooding to spill stepping
    // to the next call that the embedder makes.
    bool step_in_enabled_;

    // Stores the way how LiveEdit has patched the stack. It is used when
    // debugger returns control back to user script.
    LiveEdit::FrameDropMode frame_drop_mode_;
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
      : debug_(debug),
        previous_break_disabled_(debug->break_disabled_),
        previous_in_debug_event_listener_(debug->in_debug_event_listener_) {
    debug_->break_disabled_ = disable_break;
    debug_->in_debug_event_listener_ = disable_break;
  }
  ~DisableBreak() {
    debug_->break_disabled_ = previous_break_disabled_;
    debug_->in_debug_event_listener_ = previous_in_debug_event_listener_;
  }

 private:
  Debug* debug_;
  bool previous_break_disabled_;
  bool previous_in_debug_event_listener_;
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
  enum DebugBreakCallHelperMode {
    SAVE_RESULT_REGISTER,
    IGNORE_RESULT_REGISTER
  };

  static void GenerateDebugBreakStub(MacroAssembler* masm,
                                     DebugBreakCallHelperMode mode);

  // FrameDropper is a code replacement for a JavaScript frame with possibly
  // several frames above.
  // There is no calling conventions here, because it never actually gets
  // called, it only gets returned to.
  static void GenerateFrameDropperLiveEdit(MacroAssembler* masm);


  static void GenerateSlot(MacroAssembler* masm, RelocInfo::Mode mode);

  static void PatchDebugBreakSlot(Isolate* isolate, Address pc,
                                  Handle<Code> code);
  static void ClearDebugBreakSlot(Isolate* isolate, Address pc);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_H_
