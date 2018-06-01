// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_H_
#define V8_DEBUG_DEBUG_H_

#include <vector>

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/base/atomicops.h"
#include "src/base/hashmap.h"
#include "src/base/platform/platform.h"
#include "src/debug/debug-interface.h"
#include "src/debug/interface-types.h"
#include "src/execution.h"
#include "src/flags.h"
#include "src/frames.h"
#include "src/globals.h"
#include "src/heap/factory.h"
#include "src/objects/debug-objects.h"
#include "src/runtime/runtime.h"
#include "src/source-position-table.h"
#include "src/string-stream.h"
#include "src/v8threads.h"

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
  LastStepAction = StepIn
};

// Type of exception break. NOTE: These values are in macros.py as well.
enum ExceptionBreakType {
  BreakException = 0,
  BreakUncaughtException = 1
};

enum DebugBreakType {
  NOT_DEBUG_BREAK,
  DEBUGGER_STATEMENT,
  DEBUG_BREAK_SLOT,
  DEBUG_BREAK_SLOT_AT_CALL,
  DEBUG_BREAK_SLOT_AT_RETURN,
  DEBUG_BREAK_SLOT_AT_SUSPEND,
  DEBUG_BREAK_AT_ENTRY,
};

enum IgnoreBreakMode {
  kIgnoreIfAllFramesBlackboxed,
  kIgnoreIfTopFrameBlackboxed
};

class BreakLocation {
 public:
  static BreakLocation FromFrame(Handle<DebugInfo> debug_info,
                                 JavaScriptFrame* frame);

  static void AllAtCurrentStatement(Handle<DebugInfo> debug_info,
                                    JavaScriptFrame* frame,
                                    std::vector<BreakLocation>* result_out);

  inline bool IsSuspend() const { return type_ == DEBUG_BREAK_SLOT_AT_SUSPEND; }
  inline bool IsReturn() const { return type_ == DEBUG_BREAK_SLOT_AT_RETURN; }
  inline bool IsReturnOrSuspend() const {
    return type_ >= DEBUG_BREAK_SLOT_AT_RETURN;
  }
  inline bool IsCall() const { return type_ == DEBUG_BREAK_SLOT_AT_CALL; }
  inline bool IsDebugBreakSlot() const { return type_ >= DEBUG_BREAK_SLOT; }
  inline bool IsDebuggerStatement() const {
    return type_ == DEBUGGER_STATEMENT;
  }
  inline bool IsDebugBreakAtEntry() const {
    bool result = type_ == DEBUG_BREAK_AT_ENTRY;
    return result;
  }

  bool HasBreakPoint(Handle<DebugInfo> debug_info) const;

  inline int position() const { return position_; }

  debug::BreakLocationType type() const;

  JSGeneratorObject* GetGeneratorObjectForSuspendedFrame(
      JavaScriptFrame* frame) const;

 private:
  BreakLocation(Handle<AbstractCode> abstract_code, DebugBreakType type,
                int code_offset, int position, int generator_obj_reg_index)
      : abstract_code_(abstract_code),
        code_offset_(code_offset),
        type_(type),
        position_(position),
        generator_obj_reg_index_(generator_obj_reg_index) {
    DCHECK_NE(NOT_DEBUG_BREAK, type_);
  }

  BreakLocation(int position, DebugBreakType type)
      : code_offset_(0),
        type_(type),
        position_(position),
        generator_obj_reg_index_(0) {}

  static int BreakIndexFromCodeOffset(Handle<DebugInfo> debug_info,
                                      Handle<AbstractCode> abstract_code,
                                      int offset);

  void SetDebugBreak();
  void ClearDebugBreak();

  Handle<AbstractCode> abstract_code_;
  int code_offset_;
  DebugBreakType type_;
  int position_;
  int generator_obj_reg_index_;

  friend class BreakIterator;
};

class BreakIterator {
 public:
  explicit BreakIterator(Handle<DebugInfo> debug_info);

  BreakLocation GetBreakLocation();
  bool Done() const { return source_position_iterator_.done(); }
  void Next();

  void SkipToPosition(int position);
  void SkipTo(int count) {
    while (count-- > 0) Next();
  }

  int code_offset() { return source_position_iterator_.code_offset(); }
  int break_index() const { return break_index_; }
  inline int position() const { return position_; }
  inline int statement_position() const { return statement_position_; }

  void ClearDebugBreak();
  void SetDebugBreak();

 private:
  int BreakIndexFromPosition(int position);

  Isolate* isolate() { return debug_info_->GetIsolate(); }

  DebugBreakType GetDebugBreakType();

  Handle<DebugInfo> debug_info_;
  int break_index_;
  int position_;
  int statement_position_;
  SourcePositionTableIterator source_position_iterator_;
  DisallowHeapAllocation no_gc_;

  DISALLOW_COPY_AND_ASSIGN(BreakIterator);
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
  void OnDebugBreak(Handle<FixedArray> break_points_hit);

  void OnThrow(Handle<Object> exception);
  void OnPromiseReject(Handle<Object> promise, Handle<Object> value);
  void OnCompileError(Handle<Script> script);
  void OnAfterCompile(Handle<Script> script);

  V8_WARN_UNUSED_RESULT MaybeHandle<Object> Call(Handle<Object> fun,
                                                 Handle<Object> data);
  Handle<Context> GetDebugContext();
  void HandleDebugBreak(IgnoreBreakMode ignore_break_mode);

  // Internal logic
  bool Load();
  // The break target may not be the top-most frame, since we may be
  // breaking before entering a function that cannot contain break points.
  void Break(JavaScriptFrame* frame, Handle<JSFunction> break_target);

  // Scripts handling.
  Handle<FixedArray> GetLoadedScripts();

  // Break point handling.
  bool SetBreakPoint(Handle<JSFunction> function,
                     Handle<BreakPoint> break_point, int* source_position);
  void ClearBreakPoint(Handle<BreakPoint> break_point);
  void ChangeBreakOnException(ExceptionBreakType type, bool enable);
  bool IsBreakOnException(ExceptionBreakType type);

  bool SetBreakPointForScript(Handle<Script> script, Handle<String> condition,
                              int* source_position, int* id);
  bool SetBreakpointForFunction(Handle<JSFunction> function,
                                Handle<String> condition, int* id);
  void RemoveBreakpoint(int id);

  // Find breakpoints from the debug info and the break location and check
  // whether they are hit. Return an empty handle if not, or a FixedArray with
  // hit BreakPoint objects.
  MaybeHandle<FixedArray> GetHitBreakPoints(Handle<DebugInfo> debug_info,
                                            int position);

  // Stepping handling.
  void PrepareStep(StepAction step_action);
  void PrepareStepIn(Handle<JSFunction> function);
  void PrepareStepInSuspendedGenerator();
  void PrepareStepOnThrow();
  void ClearStepping();
  void ClearStepOut();

  void DeoptimizeFunction(Handle<SharedFunctionInfo> shared);
  void PrepareFunctionForDebugExecution(Handle<SharedFunctionInfo> shared);
  void InstallDebugBreakTrampoline();
  bool GetPossibleBreakpoints(Handle<Script> script, int start_position,
                              int end_position, bool restrict_to_function,
                              std::vector<BreakLocation>* locations);

  void RunPromiseHook(PromiseHookType hook_type, Handle<JSPromise> promise,
                      Handle<Object> parent);

  int NextAsyncTaskId(Handle<JSObject> promise);

  bool IsBlackboxed(Handle<SharedFunctionInfo> shared);

  bool CanBreakAtEntry(Handle<SharedFunctionInfo> shared);

  void SetDebugDelegate(debug::DebugDelegate* delegate, bool pass_ownership);

  // Returns whether the operation succeeded.
  bool EnsureBreakInfo(Handle<SharedFunctionInfo> shared);
  void CreateBreakInfo(Handle<SharedFunctionInfo> shared);
  Handle<DebugInfo> GetOrCreateDebugInfo(Handle<SharedFunctionInfo> shared);

  void InstallCoverageInfo(Handle<SharedFunctionInfo> shared,
                           Handle<CoverageInfo> coverage_info);
  void RemoveAllCoverageInfos();

  template <typename C>
  bool CompileToRevealInnerFunctions(C* compilable);

  // This function is used in FunctionNameUsing* tests.
  Handle<Object> FindSharedFunctionInfoInScript(Handle<Script> script,
                                                int position);

  static Handle<Object> GetSourceBreakLocations(
      Handle<SharedFunctionInfo> shared);

  // Check whether a global object is the debug global object.
  bool IsDebugGlobal(JSGlobalObject* global);

  // Check whether this frame is just about to return.
  bool IsBreakAtReturn(JavaScriptFrame* frame);

  // Support for LiveEdit
  void ScheduleFrameRestart(StackFrame* frame);

  bool AllFramesOnStackAreBlackboxed();

  // Set new script source, throw an exception if error occurred. When preview
  // is true: try to set source, throw exception if any without actual script
  // change. stack_changed is true if after editing script on pause stack is
  // changed and client should request stack trace again.
  bool SetScriptSource(Handle<Script> script, Handle<String> source,
                       bool preview, bool* stack_changed);

  // Threading support.
  char* ArchiveDebug(char* to);
  char* RestoreDebug(char* from);
  static int ArchiveSpacePerThread();
  void FreeThreadResources() { }
  void Iterate(RootVisitor* v);

  bool CheckExecutionState(int id) {
    return CheckExecutionState() && break_id() == id;
  }

  bool CheckExecutionState() {
    return is_active() && !debug_context().is_null() && break_id() != 0;
  }

  // Apply proper instrumentation depends on debug_execution_mode.
  void ApplyInstrumentation(Handle<SharedFunctionInfo> shared);

  void StartSideEffectCheckMode();
  void StopSideEffectCheckMode();

  void ApplySideEffectChecks(Handle<DebugInfo> debug_info);
  void ClearSideEffectChecks(Handle<DebugInfo> debug_info);

  bool PerformSideEffectCheck(Handle<JSFunction> function);
  bool PerformSideEffectCheckForCallback(Handle<Object> callback_info);
  bool PerformSideEffectCheckAtBytecode(InterpretedFrame* frame);

  // Flags and states.
  DebugScope* debugger_entry() {
    return reinterpret_cast<DebugScope*>(
        base::Relaxed_Load(&thread_local_.current_debug_scope_));
  }
  inline Handle<Context> debug_context() { return debug_context_; }

  void set_live_edit_enabled(bool v) { live_edit_enabled_ = v; }
  bool live_edit_enabled() const {
    return FLAG_enable_liveedit && live_edit_enabled_;
  }

  inline bool is_active() const { return is_active_; }
  inline bool is_loaded() const { return !debug_context_.is_null(); }
  inline bool in_debug_scope() const {
    return !!base::Relaxed_Load(&thread_local_.current_debug_scope_);
  }
  inline bool needs_check_on_function_call() const {
    return hook_on_function_call_;
  }

  void set_break_points_active(bool v) { break_points_active_ = v; }
  bool break_points_active() const { return break_points_active_; }

  StackFrame::Id break_frame_id() { return thread_local_.break_frame_id_; }
  int break_id() { return thread_local_.break_id_; }

  Handle<Object> return_value_handle() {
    return handle(thread_local_.return_value_, isolate_);
  }
  Object* return_value() { return thread_local_.return_value_; }
  void set_return_value(Object* value) { thread_local_.return_value_ = value; }

  // Support for embedding into generated code.
  Address is_active_address() {
    return reinterpret_cast<Address>(&is_active_);
  }

  Address hook_on_function_call_address() {
    return reinterpret_cast<Address>(&hook_on_function_call_);
  }

  Address last_step_action_address() {
    return reinterpret_cast<Address>(&thread_local_.last_step_action_);
  }

  Address suspended_generator_address() {
    return reinterpret_cast<Address>(&thread_local_.suspended_generator_);
  }

  Address restart_fp_address() {
    return reinterpret_cast<Address>(&thread_local_.restart_fp_);
  }

  StepAction last_step_action() { return thread_local_.last_step_action_; }

  DebugFeatureTracker* feature_tracker() { return &feature_tracker_; }

  // For functions in which we cannot set a break point, use a canonical
  // source position for break points.
  static const int kBreakAtEntryPosition = 0;

 private:
  explicit Debug(Isolate* isolate);
  ~Debug();

  void UpdateState();
  void UpdateHookOnFunctionCall();
  void RemoveDebugDelegate();
  void Unload();
  void SetNextBreakId() {
    thread_local_.break_id_ = ++thread_local_.break_count_;
  }

  // Return the number of virtual frames below debugger entry.
  int CurrentFrameCount();

  inline bool ignore_events() const {
    return is_suppressed_ || !is_active_ ||
           isolate_->debug_execution_mode() == DebugInfo::kSideEffects;
  }
  inline bool break_disabled() const { return break_disabled_; }

  void clear_suspended_generator() {
    thread_local_.suspended_generator_ = Smi::kZero;
  }

  bool has_suspended_generator() const {
    return thread_local_.suspended_generator_ != Smi::kZero;
  }

  bool IsExceptionBlackboxed(bool uncaught);

  void OnException(Handle<Object> exception, Handle<Object> promise);

  // Constructors for debug event objects.
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> MakeExecutionState();
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> MakeExceptionEvent(
      Handle<Object> exception, bool uncaught, Handle<Object> promise);
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> MakeCompileEvent(
      Handle<Script> script, v8::DebugEvent type);
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> MakeAsyncTaskEvent(
      v8::debug::PromiseDebugActionType type, int id);

  void ProcessCompileEvent(v8::DebugEvent event, Handle<Script> script);
  void ProcessDebugEvent(v8::DebugEvent event, Handle<JSObject> event_data);

  // Find the closest source position for a break point for a given position.
  int FindBreakablePosition(Handle<DebugInfo> debug_info, int source_position);
  // Instrument code to break at break points.
  void ApplyBreakPoints(Handle<DebugInfo> debug_info);
  // Clear code from instrumentation.
  void ClearBreakPoints(Handle<DebugInfo> debug_info);
  // Clear all code from instrumentation.
  void ClearAllBreakPoints();
  // Instrument a function with one-shots.
  void FloodWithOneShot(Handle<SharedFunctionInfo> function,
                        bool returns_only = false);
  // Clear all one-shot instrumentations, but restore break points.
  void ClearOneShot();

  bool IsFrameBlackboxed(JavaScriptFrame* frame);

  void ActivateStepOut(StackFrame* frame);
  MaybeHandle<FixedArray> CheckBreakPoints(Handle<DebugInfo> debug_info,
                                           BreakLocation* location,
                                           bool* has_break_points = nullptr);
  bool IsMutedAtCurrentLocation(JavaScriptFrame* frame);
  // Check whether a BreakPoint object is hit. Evaluate condition depending
  // on whether this is a regular break location or a break at function entry.
  bool CheckBreakPoint(Handle<BreakPoint> break_point, bool is_break_at_entry);
  MaybeHandle<Object> CallFunction(const char* name, int argc,
                                   Handle<Object> args[],
                                   bool catch_exceptions = true);

  inline void AssertDebugContext() {
    DCHECK(isolate_->context() == *debug_context());
    DCHECK(in_debug_scope());
  }

  void ThreadInit();

  void PrintBreakLocation();

  // Wraps logic for clearing and maybe freeing all debug infos.
  typedef std::function<bool(Handle<DebugInfo>)> DebugInfoClearFunction;
  void ClearAllDebugInfos(DebugInfoClearFunction clear_function);

  void RemoveBreakInfoAndMaybeFree(Handle<DebugInfo> debug_info);
  void FindDebugInfo(Handle<DebugInfo> debug_info, DebugInfoListNode** prev,
                     DebugInfoListNode** curr);
  void FreeDebugInfoListNode(DebugInfoListNode* prev, DebugInfoListNode* node);

  // Global handles.
  Handle<Context> debug_context_;

  debug::DebugDelegate* debug_delegate_ = nullptr;
  bool owns_debug_delegate_ = false;

  // Debugger is active, i.e. there is a debug event listener attached.
  bool is_active_;
  // Debugger needs to be notified on every new function call.
  // Used for stepping and read-only checks
  bool hook_on_function_call_;
  // Suppress debug events.
  bool is_suppressed_;
  // LiveEdit is enabled.
  bool live_edit_enabled_;
  // Do not trigger debug break events.
  bool break_disabled_;
  // Do not break on break points.
  bool break_points_active_;
  // Trigger debug break events for all exceptions.
  bool break_on_exception_;
  // Trigger debug break events for uncaught exceptions.
  bool break_on_uncaught_exception_;
  // Termination exception because side effect check has failed.
  bool side_effect_check_failed_;

  // List of active debug info objects.
  DebugInfoListNode* debug_info_list_;

  // Used for side effect check to mark temporary objects.
  class TemporaryObjectsTracker;
  std::unique_ptr<TemporaryObjectsTracker> temporary_objects_;

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

    // If set, next PrepareStepIn will ignore this function until stepped into
    // another function, at which point this will be cleared.
    Object* ignore_step_into_function_;

    // If set then we need to repeat StepOut action at return.
    bool fast_forward_to_return_;

    // Source statement position from last step next action.
    int last_statement_position_;

    // Frame pointer from last step next or step frame action.
    int last_frame_count_;

    // Frame pointer of the target frame we want to arrive at.
    int target_frame_count_;

    // Value of the accumulator at the point of entering the debugger.
    Object* return_value_;

    // The suspended generator object to track when stepping.
    Object* suspended_generator_;

    // The new frame pointer to drop to when restarting a frame.
    Address restart_fp_;

    int async_task_count_;

    // Last used inspector breakpoint id.
    int last_breakpoint_id_;
  };

  // Storage location for registers when handling debug break calls
  ThreadLocal thread_local_;

  Isolate* isolate_;

  friend class Isolate;
  friend class DebugScope;
  friend class DisableBreak;
  friend class LiveEdit;
  friend class SuppressDebug;
  friend class NoSideEffectScope;
  friend class LegacyDebugDelegate;

  friend Handle<FixedArray> GetDebuggedFunctions();  // In test-debug.cc
  friend void CheckDebuggerUnloaded();               // In test-debug.cc

  DISALLOW_COPY_AND_ASSIGN(Debug);
};

class LegacyDebugDelegate : public v8::debug::DebugDelegate {
 public:
  explicit LegacyDebugDelegate(Isolate* isolate) : isolate_(isolate) {}
  void PromiseEventOccurred(v8::debug::PromiseDebugActionType type, int id,
                            bool is_blackboxed) override;
  void ScriptCompiled(v8::Local<v8::debug::Script> script, bool is_live_edited,
                      bool has_compile_error) override;
  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             v8::Local<v8::Object> exec_state,
                             const std::vector<debug::BreakpointId>&) override;
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Object> exec_state,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught) override;
  bool IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end) override {
    return false;
  }

 protected:
  Isolate* isolate_;

 private:
  void ProcessDebugEvent(v8::DebugEvent event, Handle<JSObject> event_data);
  virtual void ProcessDebugEvent(v8::DebugEvent event,
                                 Handle<JSObject> event_data,
                                 Handle<JSObject> exec_state) = 0;
};

class NativeDebugDelegate : public LegacyDebugDelegate {
 public:
  NativeDebugDelegate(Isolate* isolate, v8::Debug::EventCallback callback,
                      Handle<Object> data);
  virtual ~NativeDebugDelegate();

 private:
  // Details of the debug event delivered to the debug event listener.
  class EventDetails : public v8::Debug::EventDetails {
   public:
    EventDetails(DebugEvent event, Handle<JSObject> exec_state,
                 Handle<JSObject> event_data, Handle<Object> callback_data);
    virtual DebugEvent GetEvent() const;
    virtual v8::Local<v8::Object> GetExecutionState() const;
    virtual v8::Local<v8::Object> GetEventData() const;
    virtual v8::Local<v8::Context> GetEventContext() const;
    virtual v8::Local<v8::Value> GetCallbackData() const;
    virtual v8::Isolate* GetIsolate() const;

   private:
    DebugEvent event_;              // Debug event causing the break.
    Handle<JSObject> exec_state_;   // Current execution state.
    Handle<JSObject> event_data_;   // Data associated with the event.
    Handle<Object> callback_data_;  // User data passed with the callback
                                    // when it was registered.
  };

  void ProcessDebugEvent(v8::DebugEvent event, Handle<JSObject> event_data,
                         Handle<JSObject> exec_state) override;

  v8::Debug::EventCallback callback_;
  Handle<Object> data_;
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

// This scope is used to handle return values in nested debug break points.
// When there are nested debug breaks, we use this to restore the return
// value to the previous state. This is not merged with DebugScope because
// return_value_ will not be cleared when we use DebugScope.
class ReturnValueScope {
 public:
  explicit ReturnValueScope(Debug* debug);
  ~ReturnValueScope();

 private:
  Debug* debug_;
  Handle<Object> return_value_;  // Previous result.
};

// Stack allocated class for disabling break.
class DisableBreak BASE_EMBEDDED {
 public:
  explicit DisableBreak(Debug* debug, bool disable = true)
      : debug_(debug), previous_break_disabled_(debug->break_disabled_) {
    debug_->break_disabled_ = disable;
  }
  ~DisableBreak() {
    debug_->break_disabled_ = previous_break_disabled_;
  }

 private:
  Debug* debug_;
  bool previous_break_disabled_;
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

  // Builtin to drop frames to restart function.
  static void GenerateFrameDropperTrampoline(MacroAssembler* masm);

  // Builtin to atomically (wrt deopts) handle debugger statement and
  // drop frames to restart function if necessary.
  static void GenerateHandleDebuggerStatement(MacroAssembler* masm);

  // Builtin to trigger a debug break before entering the function.
  static void GenerateDebugBreakTrampoline(MacroAssembler* masm);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_H_
