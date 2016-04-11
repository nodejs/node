// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug.h"

#include "src/api.h"
#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/global-handles.h"
#include "src/isolate-inl.h"
#include "src/list.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/snapshot/natives.h"

#include "include/v8-debug.h"

namespace v8 {
namespace internal {

Debug::Debug(Isolate* isolate)
    : debug_context_(Handle<Context>()),
      event_listener_(Handle<Object>()),
      event_listener_data_(Handle<Object>()),
      message_handler_(NULL),
      command_received_(0),
      command_queue_(isolate->logger(), kQueueInitialSize),
      is_active_(false),
      is_suppressed_(false),
      live_edit_enabled_(true),  // TODO(yangguo): set to false by default.
      break_disabled_(false),
      break_points_active_(true),
      in_debug_event_listener_(false),
      break_on_exception_(false),
      break_on_uncaught_exception_(false),
      debug_info_list_(NULL),
      feature_tracker_(isolate),
      isolate_(isolate) {
  ThreadInit();
}


static v8::Local<v8::Context> GetDebugEventContext(Isolate* isolate) {
  Handle<Context> context = isolate->debug()->debugger_entry()->GetContext();
  // Isolate::context() may have been NULL when "script collected" event
  // occured.
  if (context.is_null()) return v8::Local<v8::Context>();
  Handle<Context> native_context(context->native_context());
  return v8::Utils::ToLocal(native_context);
}


BreakLocation::BreakLocation(Handle<DebugInfo> debug_info, RelocInfo* rinfo,
                             int position, int statement_position)
    : debug_info_(debug_info),
      pc_offset_(static_cast<int>(rinfo->pc() - debug_info->code()->entry())),
      rmode_(rinfo->rmode()),
      data_(rinfo->data()),
      position_(position),
      statement_position_(statement_position) {}


BreakLocation::Iterator::Iterator(Handle<DebugInfo> debug_info,
                                  BreakLocatorType type)
    : debug_info_(debug_info),
      reloc_iterator_(debug_info->code(), GetModeMask(type)),
      break_index_(-1),
      position_(1),
      statement_position_(1) {
  if (!Done()) Next();
}


int BreakLocation::Iterator::GetModeMask(BreakLocatorType type) {
  int mask = 0;
  mask |= RelocInfo::ModeMask(RelocInfo::POSITION);
  mask |= RelocInfo::ModeMask(RelocInfo::STATEMENT_POSITION);
  mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_RETURN);
  mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_CALL);
  if (type == ALL_BREAK_LOCATIONS) {
    mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION);
    mask |= RelocInfo::ModeMask(RelocInfo::DEBUGGER_STATEMENT);
  }
  return mask;
}


void BreakLocation::Iterator::Next() {
  DisallowHeapAllocation no_gc;
  DCHECK(!Done());

  // Iterate through reloc info for code and original code stopping at each
  // breakable code target.
  bool first = break_index_ == -1;
  while (!Done()) {
    if (!first) reloc_iterator_.next();
    first = false;
    if (Done()) return;

    // Whenever a statement position or (plain) position is passed update the
    // current value of these.
    if (RelocInfo::IsPosition(rmode())) {
      if (RelocInfo::IsStatementPosition(rmode())) {
        statement_position_ = static_cast<int>(
            rinfo()->data() - debug_info_->shared()->start_position());
      }
      // Always update the position as we don't want that to be before the
      // statement position.
      position_ = static_cast<int>(rinfo()->data() -
                                   debug_info_->shared()->start_position());
      DCHECK(position_ >= 0);
      DCHECK(statement_position_ >= 0);
      continue;
    }

    DCHECK(RelocInfo::IsDebugBreakSlot(rmode()) ||
           RelocInfo::IsDebuggerStatement(rmode()));

    if (RelocInfo::IsDebugBreakSlotAtReturn(rmode())) {
      // Set the positions to the end of the function.
      if (debug_info_->shared()->HasSourceCode()) {
        position_ = debug_info_->shared()->end_position() -
                    debug_info_->shared()->start_position() - 1;
      } else {
        position_ = 0;
      }
      statement_position_ = position_;
    }

    break;
  }
  break_index_++;
}


// Find the break point at the supplied address, or the closest one before
// the address.
BreakLocation BreakLocation::FromAddress(Handle<DebugInfo> debug_info,
                                         Address pc) {
  Iterator it(debug_info, ALL_BREAK_LOCATIONS);
  it.SkipTo(BreakIndexFromAddress(debug_info, pc));
  return it.GetBreakLocation();
}


// Find the break point at the supplied address, or the closest one before
// the address.
void BreakLocation::FromAddressSameStatement(Handle<DebugInfo> debug_info,
                                             Address pc,
                                             List<BreakLocation>* result_out) {
  int break_index = BreakIndexFromAddress(debug_info, pc);
  Iterator it(debug_info, ALL_BREAK_LOCATIONS);
  it.SkipTo(break_index);
  int statement_position = it.statement_position();
  while (!it.Done() && it.statement_position() == statement_position) {
    result_out->Add(it.GetBreakLocation());
    it.Next();
  }
}


int BreakLocation::BreakIndexFromAddress(Handle<DebugInfo> debug_info,
                                         Address pc) {
  // Run through all break points to locate the one closest to the address.
  int closest_break = 0;
  int distance = kMaxInt;
  for (Iterator it(debug_info, ALL_BREAK_LOCATIONS); !it.Done(); it.Next()) {
    // Check if this break point is closer that what was previously found.
    if (it.pc() <= pc && pc - it.pc() < distance) {
      closest_break = it.break_index();
      distance = static_cast<int>(pc - it.pc());
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
  }
  return closest_break;
}


BreakLocation BreakLocation::FromPosition(Handle<DebugInfo> debug_info,
                                          int position,
                                          BreakPositionAlignment alignment) {
  // Run through all break points to locate the one closest to the source
  // position.
  int closest_break = 0;
  int distance = kMaxInt;

  for (Iterator it(debug_info, ALL_BREAK_LOCATIONS); !it.Done(); it.Next()) {
    int next_position;
    if (alignment == STATEMENT_ALIGNED) {
      next_position = it.statement_position();
    } else {
      DCHECK(alignment == BREAK_POSITION_ALIGNED);
      next_position = it.position();
    }
    if (position <= next_position && next_position - position < distance) {
      closest_break = it.break_index();
      distance = next_position - position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
  }

  Iterator it(debug_info, ALL_BREAK_LOCATIONS);
  it.SkipTo(closest_break);
  return it.GetBreakLocation();
}


void BreakLocation::SetBreakPoint(Handle<Object> break_point_object) {
  // If there is not already a real break point here patch code with debug
  // break.
  if (!HasBreakPoint()) SetDebugBreak();
  DCHECK(IsDebugBreak() || IsDebuggerStatement());
  // Set the break point information.
  DebugInfo::SetBreakPoint(debug_info_, pc_offset_, position_,
                           statement_position_, break_point_object);
}


void BreakLocation::ClearBreakPoint(Handle<Object> break_point_object) {
  // Clear the break point information.
  DebugInfo::ClearBreakPoint(debug_info_, pc_offset_, break_point_object);
  // If there are no more break points here remove the debug break.
  if (!HasBreakPoint()) {
    ClearDebugBreak();
    DCHECK(!IsDebugBreak());
  }
}


void BreakLocation::SetOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    DCHECK(IsDebugBreak());
    return;
  }

  // Patch code with debug break.
  SetDebugBreak();
}


void BreakLocation::ClearOneShot() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is a real break point here no more to do.
  if (HasBreakPoint()) {
    DCHECK(IsDebugBreak());
    return;
  }

  // Patch code removing debug break.
  ClearDebugBreak();
  DCHECK(!IsDebugBreak());
}


void BreakLocation::SetDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  // If there is already a break point here just return. This might happen if
  // the same code is flooded with break points twice. Flooding the same
  // function twice might happen when stepping in a function with an exception
  // handler as the handler and the function is the same.
  if (IsDebugBreak()) return;

  DCHECK(IsDebugBreakSlot());
  Isolate* isolate = debug_info_->GetIsolate();
  Builtins* builtins = isolate->builtins();
  Handle<Code> target =
      IsReturn() ? builtins->Return_DebugBreak() : builtins->Slot_DebugBreak();
  DebugCodegen::PatchDebugBreakSlot(isolate, pc(), target);
  DCHECK(IsDebugBreak());
}


void BreakLocation::ClearDebugBreak() {
  // Debugger statement always calls debugger. No need to modify it.
  if (IsDebuggerStatement()) return;

  DCHECK(IsDebugBreakSlot());
  DebugCodegen::ClearDebugBreakSlot(debug_info_->GetIsolate(), pc());
  DCHECK(!IsDebugBreak());
}


bool BreakLocation::IsDebugBreak() const {
  if (IsDebuggerStatement()) return false;
  DCHECK(IsDebugBreakSlot());
  return rinfo().IsPatchedDebugBreakSlotSequence();
}


Handle<Object> BreakLocation::BreakPointObjects() const {
  return debug_info_->GetBreakPointObjects(pc_offset_);
}


void DebugFeatureTracker::Track(DebugFeatureTracker::Feature feature) {
  uint32_t mask = 1 << feature;
  // Only count one sample per feature and isolate.
  if (bitfield_ & mask) return;
  isolate_->counters()->debug_feature_usage()->AddSample(feature);
  bitfield_ |= mask;
}


// Threading support.
void Debug::ThreadInit() {
  thread_local_.break_count_ = 0;
  thread_local_.break_id_ = 0;
  thread_local_.break_frame_id_ = StackFrame::NO_ID;
  thread_local_.last_step_action_ = StepNone;
  thread_local_.last_statement_position_ = RelocInfo::kNoPosition;
  thread_local_.last_fp_ = 0;
  thread_local_.target_fp_ = 0;
  thread_local_.step_in_enabled_ = false;
  // TODO(isolates): frames_are_dropped_?
  base::NoBarrier_Store(&thread_local_.current_debug_scope_,
                        static_cast<base::AtomicWord>(0));
}


char* Debug::ArchiveDebug(char* storage) {
  char* to = storage;
  MemCopy(to, reinterpret_cast<char*>(&thread_local_), sizeof(ThreadLocal));
  ThreadInit();
  return storage + ArchiveSpacePerThread();
}


char* Debug::RestoreDebug(char* storage) {
  char* from = storage;
  MemCopy(reinterpret_cast<char*>(&thread_local_), from, sizeof(ThreadLocal));
  return storage + ArchiveSpacePerThread();
}


int Debug::ArchiveSpacePerThread() {
  return sizeof(ThreadLocal);
}


DebugInfoListNode::DebugInfoListNode(DebugInfo* debug_info): next_(NULL) {
  // Globalize the request debug info object and make it weak.
  GlobalHandles* global_handles = debug_info->GetIsolate()->global_handles();
  debug_info_ =
      Handle<DebugInfo>::cast(global_handles->Create(debug_info)).location();
}


DebugInfoListNode::~DebugInfoListNode() {
  if (debug_info_ == nullptr) return;
  GlobalHandles::Destroy(reinterpret_cast<Object**>(debug_info_));
  debug_info_ = nullptr;
}


bool Debug::Load() {
  // Return if debugger is already loaded.
  if (is_loaded()) return true;

  // Bail out if we're already in the process of compiling the native
  // JavaScript source code for the debugger.
  if (is_suppressed_) return false;
  SuppressDebug while_loading(this);

  // Disable breakpoints and interrupts while compiling and running the
  // debugger scripts including the context creation code.
  DisableBreak disable(this, true);
  PostponeInterruptsScope postpone(isolate_);

  // Create the debugger context.
  HandleScope scope(isolate_);
  ExtensionConfiguration no_extensions;
  Handle<Context> context = isolate_->bootstrapper()->CreateEnvironment(
      MaybeHandle<JSGlobalProxy>(), v8::Local<ObjectTemplate>(), &no_extensions,
      DEBUG_CONTEXT);

  // Fail if no context could be created.
  if (context.is_null()) return false;

  debug_context_ = Handle<Context>::cast(
      isolate_->global_handles()->Create(*context));

  feature_tracker()->Track(DebugFeatureTracker::kActive);

  return true;
}


void Debug::Unload() {
  ClearAllBreakPoints();
  ClearStepping();

  // Return debugger is not loaded.
  if (!is_loaded()) return;

  // Clear debugger context global handle.
  GlobalHandles::Destroy(Handle<Object>::cast(debug_context_).location());
  debug_context_ = Handle<Context>();
}


void Debug::Break(Arguments args, JavaScriptFrame* frame) {
  HandleScope scope(isolate_);
  DCHECK(args.length() == 0);

  // Initialize LiveEdit.
  LiveEdit::InitializeThreadLocal(this);

  // Just continue if breaks are disabled or debugger cannot be loaded.
  if (break_disabled()) return;

  // Enter the debugger.
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Postpone interrupt during breakpoint processing.
  PostponeInterruptsScope postpone(isolate_);

  // Get the debug info (create it if it does not exist).
  Handle<JSFunction> function(frame->function());
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return;
  }
  Handle<DebugInfo> debug_info(shared->GetDebugInfo());

  // Find the break location where execution has stopped.
  // PC points to the instruction after the current one, possibly a break
  // location as well. So the "- 1" to exclude it from the search.
  Address call_pc = frame->pc() - 1;
  BreakLocation location = BreakLocation::FromAddress(debug_info, call_pc);

  // Find actual break points, if any, and trigger debug break event.
  if (break_points_active_ && location.HasBreakPoint()) {
    Handle<Object> break_point_objects = location.BreakPointObjects();
    Handle<Object> break_points_hit = CheckBreakPoints(break_point_objects);
    if (!break_points_hit->IsUndefined()) {
      // Clear all current stepping setup.
      ClearStepping();
      // Notify the debug event listeners.
      OnDebugBreak(break_points_hit, false);
      return;
    }
  }

  // No break point. Check for stepping.
  StepAction step_action = last_step_action();
  Address current_fp = frame->UnpaddedFP();
  Address target_fp = thread_local_.target_fp_;
  Address last_fp = thread_local_.last_fp_;

  bool step_break = true;
  switch (step_action) {
    case StepNone:
      return;
    case StepOut:
      // Step out has not reached the target frame yet.
      if (current_fp < target_fp) return;
      break;
    case StepNext:
      // Step next should not break in a deeper frame.
      if (current_fp < target_fp) return;
    // Fall through.
    case StepIn:
      step_break = location.IsReturn() || (current_fp != last_fp) ||
                   (thread_local_.last_statement_position_ !=
                    location.code()->SourceStatementPosition(frame->pc()));
      break;
    case StepFrame:
      step_break = current_fp != last_fp;
      break;
  }

  // Clear all current stepping setup.
  ClearStepping();

  if (step_break) {
    // Notify the debug event listeners.
    OnDebugBreak(isolate_->factory()->undefined_value(), false);
  } else {
    // Re-prepare to continue.
    PrepareStep(step_action);
  }
}


// Check the break point objects for whether one or more are actually
// triggered. This function returns a JSArray with the break point objects
// which is triggered.
Handle<Object> Debug::CheckBreakPoints(Handle<Object> break_point_objects) {
  Factory* factory = isolate_->factory();

  // Count the number of break points hit. If there are multiple break points
  // they are in a FixedArray.
  Handle<FixedArray> break_points_hit;
  int break_points_hit_count = 0;
  DCHECK(!break_point_objects->IsUndefined());
  if (break_point_objects->IsFixedArray()) {
    Handle<FixedArray> array(FixedArray::cast(*break_point_objects));
    break_points_hit = factory->NewFixedArray(array->length());
    for (int i = 0; i < array->length(); i++) {
      Handle<Object> o(array->get(i), isolate_);
      if (CheckBreakPoint(o)) {
        break_points_hit->set(break_points_hit_count++, *o);
      }
    }
  } else {
    break_points_hit = factory->NewFixedArray(1);
    if (CheckBreakPoint(break_point_objects)) {
      break_points_hit->set(break_points_hit_count++, *break_point_objects);
    }
  }

  // Return undefined if no break points were triggered.
  if (break_points_hit_count == 0) {
    return factory->undefined_value();
  }
  // Return break points hit as a JSArray.
  Handle<JSArray> result = factory->NewJSArrayWithElements(break_points_hit);
  result->set_length(Smi::FromInt(break_points_hit_count));
  return result;
}


MaybeHandle<Object> Debug::CallFunction(const char* name, int argc,
                                        Handle<Object> args[]) {
  PostponeInterruptsScope no_interrupts(isolate_);
  AssertDebugContext();
  Handle<Object> holder = isolate_->natives_utils_object();
  Handle<JSFunction> fun = Handle<JSFunction>::cast(
      Object::GetProperty(isolate_, holder, name, STRICT).ToHandleChecked());
  Handle<Object> undefined = isolate_->factory()->undefined_value();
  return Execution::TryCall(isolate_, fun, undefined, argc, args);
}


// Check whether a single break point object is triggered.
bool Debug::CheckBreakPoint(Handle<Object> break_point_object) {
  Factory* factory = isolate_->factory();
  HandleScope scope(isolate_);

  // Ignore check if break point object is not a JSObject.
  if (!break_point_object->IsJSObject()) return true;

  // Get the break id as an object.
  Handle<Object> break_id = factory->NewNumberFromInt(Debug::break_id());

  // Call IsBreakPointTriggered.
  Handle<Object> argv[] = { break_id, break_point_object };
  Handle<Object> result;
  if (!CallFunction("IsBreakPointTriggered", arraysize(argv), argv)
           .ToHandle(&result)) {
    return false;
  }

  // Return whether the break point is triggered.
  return result->IsTrue();
}


bool Debug::SetBreakPoint(Handle<JSFunction> function,
                          Handle<Object> break_point_object,
                          int* source_position) {
  HandleScope scope(isolate_);

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if retrieving debug info failed.
    return true;
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  // Source positions starts with zero.
  DCHECK(*source_position >= 0);

  // Find the break point and change it.
  BreakLocation location = BreakLocation::FromPosition(
      debug_info, *source_position, STATEMENT_ALIGNED);
  *source_position = location.statement_position();
  location.SetBreakPoint(break_point_object);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);

  // At least one active break point now.
  return debug_info->GetBreakPointCount() > 0;
}


bool Debug::SetBreakPointForScript(Handle<Script> script,
                                   Handle<Object> break_point_object,
                                   int* source_position,
                                   BreakPositionAlignment alignment) {
  HandleScope scope(isolate_);

  // Obtain shared function info for the function.
  Handle<Object> result =
      FindSharedFunctionInfoInScript(script, *source_position);
  if (result->IsUndefined()) return false;

  // Make sure the function has set up the debug info.
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(result);
  if (!EnsureDebugInfo(shared, Handle<JSFunction>::null())) {
    // Return if retrieving debug info failed.
    return false;
  }

  // Find position within function. The script position might be before the
  // source position of the first function.
  int position;
  if (shared->start_position() > *source_position) {
    position = 0;
  } else {
    position = *source_position - shared->start_position();
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  // Source positions starts with zero.
  DCHECK(position >= 0);

  // Find the break point and change it.
  BreakLocation location =
      BreakLocation::FromPosition(debug_info, position, alignment);
  location.SetBreakPoint(break_point_object);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);

  position = (alignment == STATEMENT_ALIGNED) ? location.statement_position()
                                              : location.position();

  *source_position = position + shared->start_position();

  // At least one active break point now.
  DCHECK(debug_info->GetBreakPointCount() > 0);
  return true;
}


void Debug::ClearBreakPoint(Handle<Object> break_point_object) {
  HandleScope scope(isolate_);

  DebugInfoListNode* node = debug_info_list_;
  while (node != NULL) {
    Handle<Object> result =
        DebugInfo::FindBreakPointInfo(node->debug_info(), break_point_object);
    if (!result->IsUndefined()) {
      // Get information in the break point.
      Handle<BreakPointInfo> break_point_info =
          Handle<BreakPointInfo>::cast(result);
      Handle<DebugInfo> debug_info = node->debug_info();

      // Find the break point and clear it.
      Address pc =
          debug_info->code()->entry() + break_point_info->code_position();

      BreakLocation location = BreakLocation::FromAddress(debug_info, pc);
      location.ClearBreakPoint(break_point_object);

      // If there are no more break points left remove the debug info for this
      // function.
      if (debug_info->GetBreakPointCount() == 0) {
        RemoveDebugInfoAndClearFromShared(debug_info);
      }

      return;
    }
    node = node->next();
  }
}


// Clear out all the debug break code. This is ONLY supposed to be used when
// shutting down the debugger as it will leave the break point information in
// DebugInfo even though the code is patched back to the non break point state.
void Debug::ClearAllBreakPoints() {
  for (DebugInfoListNode* node = debug_info_list_; node != NULL;
       node = node->next()) {
    for (BreakLocation::Iterator it(node->debug_info(), ALL_BREAK_LOCATIONS);
         !it.Done(); it.Next()) {
      it.GetBreakLocation().ClearDebugBreak();
    }
  }
  // Remove all debug info.
  while (debug_info_list_ != NULL) {
    RemoveDebugInfoAndClearFromShared(debug_info_list_->debug_info());
  }
}


void Debug::FloodWithOneShot(Handle<JSFunction> function,
                             BreakLocatorType type) {
  // Debug utility functions are not subject to debugging.
  if (function->native_context() == *debug_context()) return;

  if (!function->shared()->IsSubjectToDebugging()) {
    // Builtin functions are not subject to stepping, but need to be
    // deoptimized, because optimized code does not check for debug
    // step in at call sites.
    Deoptimizer::DeoptimizeFunction(*function);
    return;
  }
  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if we failed to retrieve the debug info.
    return;
  }

  // Flood the function with break points.
  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  for (BreakLocation::Iterator it(debug_info, type); !it.Done(); it.Next()) {
    it.GetBreakLocation().SetOneShot();
  }
}


void Debug::ChangeBreakOnException(ExceptionBreakType type, bool enable) {
  if (type == BreakUncaughtException) {
    break_on_uncaught_exception_ = enable;
  } else {
    break_on_exception_ = enable;
  }
}


bool Debug::IsBreakOnException(ExceptionBreakType type) {
  if (type == BreakUncaughtException) {
    return break_on_uncaught_exception_;
  } else {
    return break_on_exception_;
  }
}


FrameSummary GetFirstFrameSummary(JavaScriptFrame* frame) {
  List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
  frame->Summarize(&frames);
  return frames.first();
}


void Debug::PrepareStepIn(Handle<JSFunction> function) {
  if (!is_active()) return;
  if (last_step_action() < StepIn) return;
  if (in_debug_scope()) return;
  if (thread_local_.step_in_enabled_) {
    FloodWithOneShot(function);
  }
}


void Debug::PrepareStepOnThrow() {
  if (!is_active()) return;
  if (last_step_action() == StepNone) return;
  if (in_debug_scope()) return;

  ClearOneShot();

  // Iterate through the JavaScript stack looking for handlers.
  JavaScriptFrameIterator it(isolate_);
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    int stack_slots = 0;  // The computed stack slot count is not used.
    if (frame->LookupExceptionHandlerInTable(&stack_slots, NULL) > 0) break;
    it.Advance();
  }

  // Find the closest Javascript frame we can flood with one-shots.
  while (!it.done() &&
         !it.frame()->function()->shared()->IsSubjectToDebugging()) {
    it.Advance();
  }

  if (it.done()) return;  // No suitable Javascript catch handler.

  FloodWithOneShot(Handle<JSFunction>(it.frame()->function()));
}


void Debug::PrepareStep(StepAction step_action) {
  HandleScope scope(isolate_);

  DCHECK(in_debug_scope());

  // Get the frame where the execution has stopped and skip the debug frame if
  // any. The debug frame will only be present if execution was stopped due to
  // hitting a break point. In other situations (e.g. unhandled exception) the
  // debug frame is not present.
  StackFrame::Id frame_id = break_frame_id();
  // If there is no JavaScript stack don't do anything.
  if (frame_id == StackFrame::NO_ID) return;

  JavaScriptFrameIterator frames_it(isolate_, frame_id);
  JavaScriptFrame* frame = frames_it.frame();

  feature_tracker()->Track(DebugFeatureTracker::kStepping);

  // Remember this step action and count.
  thread_local_.last_step_action_ = step_action;
  STATIC_ASSERT(StepFrame > StepIn);
  thread_local_.step_in_enabled_ = (step_action >= StepIn);

  // If the function on the top frame is unresolved perform step out. This will
  // be the case when calling unknown function and having the debugger stopped
  // in an unhandled exception.
  if (!frame->function()->IsJSFunction()) {
    // Step out: Find the calling JavaScript frame and flood it with
    // breakpoints.
    frames_it.Advance();
    // Fill the function to return to with one-shot break points.
    JSFunction* function = frames_it.frame()->function();
    FloodWithOneShot(Handle<JSFunction>(function));
    return;
  }

  // Get the debug info (create it if it does not exist).
  FrameSummary summary = GetFirstFrameSummary(frame);
  Handle<JSFunction> function(summary.function());
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if ensuring debug info failed.
    return;
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  // Refresh frame summary if the code has been recompiled for debugging.
  if (shared->code() != *summary.code()) summary = GetFirstFrameSummary(frame);

  // PC points to the instruction after the current one, possibly a break
  // location as well. So the "- 1" to exclude it from the search.
  Address call_pc = summary.pc() - 1;
  BreakLocation location = BreakLocation::FromAddress(debug_info, call_pc);

  // At a return statement we will step out either way.
  if (location.IsReturn()) step_action = StepOut;

  thread_local_.last_statement_position_ =
      debug_info->code()->SourceStatementPosition(summary.pc());
  thread_local_.last_fp_ = frame->UnpaddedFP();

  switch (step_action) {
    case StepNone:
      UNREACHABLE();
      break;
    case StepOut:
      // Advance to caller frame.
      frames_it.Advance();
      // Skip native and extension functions on the stack.
      while (!frames_it.done() &&
             !frames_it.frame()->function()->shared()->IsSubjectToDebugging()) {
        // Builtin functions are not subject to stepping, but need to be
        // deoptimized to include checks for step-in at call sites.
        Deoptimizer::DeoptimizeFunction(frames_it.frame()->function());
        frames_it.Advance();
      }
      if (frames_it.done()) {
        // Stepping out to the embedder. Disable step-in to avoid stepping into
        // the next (unrelated) call that the embedder makes.
        thread_local_.step_in_enabled_ = false;
      } else {
        // Fill the caller function to return to with one-shot break points.
        Handle<JSFunction> caller_function(frames_it.frame()->function());
        FloodWithOneShot(caller_function);
        thread_local_.target_fp_ = frames_it.frame()->UnpaddedFP();
      }
      // Clear last position info. For stepping out it does not matter.
      thread_local_.last_statement_position_ = RelocInfo::kNoPosition;
      thread_local_.last_fp_ = 0;
      break;
    case StepNext:
      thread_local_.target_fp_ = frame->UnpaddedFP();
      FloodWithOneShot(function);
      break;
    case StepIn:
      FloodWithOneShot(function);
      break;
    case StepFrame:
      // No point in setting one-shot breaks at places where we are not about
      // to leave the current frame.
      FloodWithOneShot(function, CALLS_AND_RETURNS);
      break;
  }
}


// Simple function for returning the source positions for active break points.
Handle<Object> Debug::GetSourceBreakLocations(
    Handle<SharedFunctionInfo> shared,
    BreakPositionAlignment position_alignment) {
  Isolate* isolate = shared->GetIsolate();
  Heap* heap = isolate->heap();
  if (!shared->HasDebugInfo()) {
    return Handle<Object>(heap->undefined_value(), isolate);
  }
  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  if (debug_info->GetBreakPointCount() == 0) {
    return Handle<Object>(heap->undefined_value(), isolate);
  }
  Handle<FixedArray> locations =
      isolate->factory()->NewFixedArray(debug_info->GetBreakPointCount());
  int count = 0;
  for (int i = 0; i < debug_info->break_points()->length(); ++i) {
    if (!debug_info->break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(debug_info->break_points()->get(i));
      int break_points = break_point_info->GetBreakPointCount();
      if (break_points == 0) continue;
      Smi* position = NULL;
      switch (position_alignment) {
        case STATEMENT_ALIGNED:
          position = Smi::FromInt(break_point_info->statement_position());
          break;
        case BREAK_POSITION_ALIGNED:
          position = Smi::FromInt(break_point_info->source_position());
          break;
      }
      for (int j = 0; j < break_points; ++j) locations->set(count++, position);
    }
  }
  return locations;
}


void Debug::ClearStepping() {
  // Clear the various stepping setup.
  ClearOneShot();

  thread_local_.last_step_action_ = StepNone;
  thread_local_.step_in_enabled_ = false;
  thread_local_.last_statement_position_ = RelocInfo::kNoPosition;
  thread_local_.last_fp_ = 0;
  thread_local_.target_fp_ = 0;
}


// Clears all the one-shot break points that are currently set. Normally this
// function is called each time a break point is hit as one shot break points
// are used to support stepping.
void Debug::ClearOneShot() {
  // The current implementation just runs through all the breakpoints. When the
  // last break point for a function is removed that function is automatically
  // removed from the list.
  for (DebugInfoListNode* node = debug_info_list_; node != NULL;
       node = node->next()) {
    for (BreakLocation::Iterator it(node->debug_info(), ALL_BREAK_LOCATIONS);
         !it.Done(); it.Next()) {
      it.GetBreakLocation().ClearOneShot();
    }
  }
}


void Debug::EnableStepIn() {
  STATIC_ASSERT(StepFrame > StepIn);
  thread_local_.step_in_enabled_ = (last_step_action() >= StepIn);
}


bool MatchingCodeTargets(Code* target1, Code* target2) {
  if (target1 == target2) return true;
  if (target1->kind() != target2->kind()) return false;
  return target1->is_handler() || target1->is_inline_cache_stub();
}


// Count the number of calls before the current frame PC to find the
// corresponding PC in the newly recompiled code.
static Address ComputeNewPcForRedirect(Code* new_code, Code* old_code,
                                       Address old_pc) {
  DCHECK_EQ(old_code->kind(), Code::FUNCTION);
  DCHECK_EQ(new_code->kind(), Code::FUNCTION);
  DCHECK(new_code->has_debug_break_slots());
  static const int mask = RelocInfo::kCodeTargetMask;

  // Find the target of the current call.
  Code* target = NULL;
  intptr_t delta = 0;
  for (RelocIterator it(old_code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address current_pc = rinfo->pc();
    // The frame PC is behind the call instruction by the call instruction size.
    if (current_pc > old_pc) break;
    delta = old_pc - current_pc;
    target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  }

  // Count the number of calls to the same target before the current call.
  int index = 0;
  for (RelocIterator it(old_code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Address current_pc = rinfo->pc();
    if (current_pc > old_pc) break;
    Code* current = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (MatchingCodeTargets(target, current)) index++;
  }

  DCHECK(index > 0);

  // Repeat the count on the new code to find corresponding call.
  for (RelocIterator it(new_code, mask); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    Code* current = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (MatchingCodeTargets(target, current)) index--;
    if (index == 0) return rinfo->pc() + delta;
  }

  UNREACHABLE();
  return NULL;
}


// Count the number of continuations at which the current pc offset is at.
static int ComputeContinuationIndexFromPcOffset(Code* code, int pc_offset) {
  DCHECK_EQ(code->kind(), Code::FUNCTION);
  Address pc = code->instruction_start() + pc_offset;
  int mask = RelocInfo::ModeMask(RelocInfo::GENERATOR_CONTINUATION);
  int index = 0;
  for (RelocIterator it(code, mask); !it.done(); it.next()) {
    index++;
    RelocInfo* rinfo = it.rinfo();
    Address current_pc = rinfo->pc();
    if (current_pc == pc) break;
    DCHECK(current_pc < pc);
  }
  return index;
}


// Find the pc offset for the given continuation index.
static int ComputePcOffsetFromContinuationIndex(Code* code, int index) {
  DCHECK_EQ(code->kind(), Code::FUNCTION);
  DCHECK(code->has_debug_break_slots());
  int mask = RelocInfo::ModeMask(RelocInfo::GENERATOR_CONTINUATION);
  RelocIterator it(code, mask);
  for (int i = 1; i < index; i++) it.next();
  return static_cast<int>(it.rinfo()->pc() - code->instruction_start());
}


class RedirectActiveFunctions : public ThreadVisitor {
 public:
  explicit RedirectActiveFunctions(SharedFunctionInfo* shared)
      : shared_(shared) {
    DCHECK(shared->HasDebugCode());
  }

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
      JavaScriptFrame* frame = it.frame();
      JSFunction* function = frame->function();
      if (frame->is_optimized()) continue;
      if (!function->Inlines(shared_)) continue;

      Code* frame_code = frame->LookupCode();
      DCHECK(frame_code->kind() == Code::FUNCTION);
      if (frame_code->has_debug_break_slots()) continue;

      Code* new_code = function->shared()->code();
      Address old_pc = frame->pc();
      Address new_pc = ComputeNewPcForRedirect(new_code, frame_code, old_pc);

      if (FLAG_trace_deopt) {
        PrintF("Replacing pc for debugging: %08" V8PRIxPTR " => %08" V8PRIxPTR
               "\n",
               reinterpret_cast<intptr_t>(old_pc),
               reinterpret_cast<intptr_t>(new_pc));
      }

      if (FLAG_enable_embedded_constant_pool) {
        // Update constant pool pointer for new code.
        frame->set_constant_pool(new_code->constant_pool());
      }

      // Patch the return address to return into the code with
      // debug break slots.
      frame->set_pc(new_pc);
    }
  }

 private:
  SharedFunctionInfo* shared_;
  DisallowHeapAllocation no_gc_;
};


bool Debug::PrepareFunctionForBreakPoints(Handle<SharedFunctionInfo> shared) {
  DCHECK(shared->is_compiled());

  if (isolate_->concurrent_recompilation_enabled()) {
    isolate_->optimizing_compile_dispatcher()->Flush();
  }

  List<Handle<JSFunction> > functions;
  List<Handle<JSGeneratorObject> > suspended_generators;

  // Flush all optimized code maps. Note that the below heap iteration does not
  // cover this, because the given function might have been inlined into code
  // for which no JSFunction exists.
  {
    SharedFunctionInfo::Iterator iterator(isolate_);
    while (SharedFunctionInfo* shared = iterator.Next()) {
      if (!shared->OptimizedCodeMapIsCleared()) {
        shared->ClearOptimizedCodeMap();
      }
    }
  }

  // Make sure we abort incremental marking.
  isolate_->heap()->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                                      "prepare for break points");

  {
    HeapIterator iterator(isolate_->heap());
    HeapObject* obj;
    bool include_generators = shared->is_generator();

    while ((obj = iterator.next())) {
      if (obj->IsJSFunction()) {
        JSFunction* function = JSFunction::cast(obj);
        if (!function->Inlines(*shared)) continue;
        if (function->code()->kind() == Code::OPTIMIZED_FUNCTION) {
          Deoptimizer::DeoptimizeFunction(function);
        }
        if (function->shared() == *shared) functions.Add(handle(function));
      } else if (include_generators && obj->IsJSGeneratorObject()) {
        JSGeneratorObject* generator_obj = JSGeneratorObject::cast(obj);
        if (!generator_obj->is_suspended()) continue;
        JSFunction* function = generator_obj->function();
        if (!function->Inlines(*shared)) continue;
        int pc_offset = generator_obj->continuation();
        int index =
            ComputeContinuationIndexFromPcOffset(function->code(), pc_offset);
        generator_obj->set_continuation(index);
        suspended_generators.Add(handle(generator_obj));
      }
    }
  }

  if (!shared->HasDebugCode()) {
    DCHECK(functions.length() > 0);
    if (!Compiler::CompileDebugCode(functions.first())) return false;
  }

  for (Handle<JSFunction> const function : functions) {
    function->ReplaceCode(shared->code());
  }

  for (Handle<JSGeneratorObject> const generator_obj : suspended_generators) {
    int index = generator_obj->continuation();
    int pc_offset = ComputePcOffsetFromContinuationIndex(shared->code(), index);
    generator_obj->set_continuation(pc_offset);
  }

  // Update PCs on the stack to point to recompiled code.
  RedirectActiveFunctions redirect_visitor(*shared);
  redirect_visitor.VisitThread(isolate_, isolate_->thread_local_top());
  isolate_->thread_manager()->IterateArchivedThreads(&redirect_visitor);

  return true;
}


class SharedFunctionInfoFinder {
 public:
  explicit SharedFunctionInfoFinder(int target_position)
      : current_candidate_(NULL),
        current_candidate_closure_(NULL),
        current_start_position_(RelocInfo::kNoPosition),
        target_position_(target_position) {}

  void NewCandidate(SharedFunctionInfo* shared, JSFunction* closure = NULL) {
    if (!shared->IsSubjectToDebugging()) return;
    int start_position = shared->function_token_position();
    if (start_position == RelocInfo::kNoPosition) {
      start_position = shared->start_position();
    }

    if (start_position > target_position_) return;
    if (target_position_ > shared->end_position()) return;

    if (current_candidate_ != NULL) {
      if (current_start_position_ == start_position &&
          shared->end_position() == current_candidate_->end_position()) {
        // If we already have a matching closure, do not throw it away.
        if (current_candidate_closure_ != NULL && closure == NULL) return;
        // If a top-level function contains only one function
        // declaration the source for the top-level and the function
        // is the same. In that case prefer the non top-level function.
        if (!current_candidate_->is_toplevel() && shared->is_toplevel()) return;
      } else if (start_position < current_start_position_ ||
                 current_candidate_->end_position() < shared->end_position()) {
        return;
      }
    }

    current_start_position_ = start_position;
    current_candidate_ = shared;
    current_candidate_closure_ = closure;
  }

  SharedFunctionInfo* Result() { return current_candidate_; }

  JSFunction* ResultClosure() { return current_candidate_closure_; }

 private:
  SharedFunctionInfo* current_candidate_;
  JSFunction* current_candidate_closure_;
  int current_start_position_;
  int target_position_;
  DisallowHeapAllocation no_gc_;
};


// We need to find a SFI for a literal that may not yet have been compiled yet,
// and there may not be a JSFunction referencing it. Find the SFI closest to
// the given position, compile it to reveal possible inner SFIs and repeat.
// While we are at this, also ensure code with debug break slots so that we do
// not have to compile a SFI without JSFunction, which is paifu for those that
// cannot be compiled without context (need to find outer compilable SFI etc.)
Handle<Object> Debug::FindSharedFunctionInfoInScript(Handle<Script> script,
                                                     int position) {
  for (int iteration = 0;; iteration++) {
    // Go through all shared function infos associated with this script to
    // find the inner most function containing this position.
    // If there is no shared function info for this script at all, there is
    // no point in looking for it by walking the heap.
    if (!script->shared_function_infos()->IsWeakFixedArray()) break;

    SharedFunctionInfo* shared;
    {
      SharedFunctionInfoFinder finder(position);
      WeakFixedArray::Iterator iterator(script->shared_function_infos());
      SharedFunctionInfo* candidate;
      while ((candidate = iterator.Next<SharedFunctionInfo>())) {
        finder.NewCandidate(candidate);
      }
      shared = finder.Result();
      if (shared == NULL) break;
      // We found it if it's already compiled and has debug code.
      if (shared->HasDebugCode()) {
        Handle<SharedFunctionInfo> shared_handle(shared);
        // If the iteration count is larger than 1, we had to compile the outer
        // function in order to create this shared function info. So there can
        // be no JSFunction referencing it. We can anticipate creating a debug
        // info while bypassing PrepareFunctionForBreakpoints.
        if (iteration > 1) {
          AllowHeapAllocation allow_before_return;
          CreateDebugInfo(shared_handle);
        }
        return shared_handle;
      }
    }
    // If not, compile to reveal inner functions, if possible.
    if (shared->allows_lazy_compilation_without_context()) {
      HandleScope scope(isolate_);
      if (!Compiler::CompileDebugCode(handle(shared))) break;
      continue;
    }

    // If not possible, comb the heap for the best suitable compile target.
    JSFunction* closure;
    {
      HeapIterator it(isolate_->heap());
      SharedFunctionInfoFinder finder(position);
      while (HeapObject* object = it.next()) {
        JSFunction* candidate_closure = NULL;
        SharedFunctionInfo* candidate = NULL;
        if (object->IsJSFunction()) {
          candidate_closure = JSFunction::cast(object);
          candidate = candidate_closure->shared();
        } else if (object->IsSharedFunctionInfo()) {
          candidate = SharedFunctionInfo::cast(object);
          if (!candidate->allows_lazy_compilation_without_context()) continue;
        } else {
          continue;
        }
        if (candidate->script() == *script) {
          finder.NewCandidate(candidate, candidate_closure);
        }
      }
      closure = finder.ResultClosure();
      shared = finder.Result();
    }
    if (shared == NULL) break;
    HandleScope scope(isolate_);
    if (closure == NULL) {
      if (!Compiler::CompileDebugCode(handle(shared))) break;
    } else {
      if (!Compiler::CompileDebugCode(handle(closure))) break;
    }
  }
  return isolate_->factory()->undefined_value();
}


// Ensures the debug information is present for shared.
bool Debug::EnsureDebugInfo(Handle<SharedFunctionInfo> shared,
                            Handle<JSFunction> function) {
  if (!shared->IsSubjectToDebugging()) return false;

  // Return if we already have the debug info for shared.
  if (shared->HasDebugInfo()) return true;

  if (function.is_null()) {
    DCHECK(shared->HasDebugCode());
  } else if (!Compiler::Compile(function, CLEAR_EXCEPTION)) {
    return false;
  }

  if (!PrepareFunctionForBreakPoints(shared)) return false;

  CreateDebugInfo(shared);

  return true;
}


void Debug::CreateDebugInfo(Handle<SharedFunctionInfo> shared) {
  // Create the debug info object.
  DCHECK(shared->HasDebugCode());
  Handle<DebugInfo> debug_info = isolate_->factory()->NewDebugInfo(shared);

  // Add debug info to the list.
  DebugInfoListNode* node = new DebugInfoListNode(*debug_info);
  node->set_next(debug_info_list_);
  debug_info_list_ = node;
}


void Debug::RemoveDebugInfoAndClearFromShared(Handle<DebugInfo> debug_info) {
  HandleScope scope(isolate_);
  Handle<SharedFunctionInfo> shared(debug_info->shared());

  DCHECK_NOT_NULL(debug_info_list_);
  // Run through the debug info objects to find this one and remove it.
  DebugInfoListNode* prev = NULL;
  DebugInfoListNode* current = debug_info_list_;
  while (current != NULL) {
    if (current->debug_info().is_identical_to(debug_info)) {
      // Unlink from list. If prev is NULL we are looking at the first element.
      if (prev == NULL) {
        debug_info_list_ = current->next();
      } else {
        prev->set_next(current->next());
      }
      delete current;
      shared->set_debug_info(isolate_->heap()->undefined_value());
      return;
    }
    // Move to next in list.
    prev = current;
    current = current->next();
  }

  UNREACHABLE();
}


void Debug::SetAfterBreakTarget(JavaScriptFrame* frame) {
  after_break_target_ = NULL;

  if (LiveEdit::SetAfterBreakTarget(this)) return;  // LiveEdit did the job.

  // Continue just after the slot.
  after_break_target_ = frame->pc();
}


bool Debug::IsBreakAtReturn(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // Get the executing function in which the debug break occurred.
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());

  // With no debug info there are no break points, so we can't be at a return.
  if (!shared->HasDebugInfo()) return false;
  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  Handle<Code> code(debug_info->code());
#ifdef DEBUG
  // Get the code which is actually executing.
  Handle<Code> frame_code(frame->LookupCode());
  DCHECK(frame_code.is_identical_to(code));
#endif

  // Find the reloc info matching the start of the debug break slot.
  Address slot_pc = frame->pc() - Assembler::kDebugBreakSlotLength;
  int mask = RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_RETURN);
  for (RelocIterator it(*code, mask); !it.done(); it.next()) {
    if (it.rinfo()->pc() == slot_pc) return true;
  }
  return false;
}


void Debug::FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                                  LiveEdit::FrameDropMode mode) {
  if (mode != LiveEdit::CURRENTLY_SET_MODE) {
    thread_local_.frame_drop_mode_ = mode;
  }
  thread_local_.break_frame_id_ = new_break_frame_id;
}


bool Debug::IsDebugGlobal(JSGlobalObject* global) {
  return is_loaded() && global == debug_context()->global_object();
}


void Debug::ClearMirrorCache() {
  PostponeInterruptsScope postpone(isolate_);
  HandleScope scope(isolate_);
  CallFunction("ClearMirrorCache", 0, NULL);
}


Handle<FixedArray> Debug::GetLoadedScripts() {
  isolate_->heap()->CollectAllGarbage();
  Factory* factory = isolate_->factory();
  if (!factory->script_list()->IsWeakFixedArray()) {
    return factory->empty_fixed_array();
  }
  Handle<WeakFixedArray> array =
      Handle<WeakFixedArray>::cast(factory->script_list());
  Handle<FixedArray> results = factory->NewFixedArray(array->Length());
  int length = 0;
  {
    Script::Iterator iterator(isolate_);
    Script* script;
    while ((script = iterator.Next())) {
      if (script->HasValidSource()) results->set(length++, script);
    }
  }
  results->Shrink(length);
  return results;
}


void Debug::GetStepinPositions(JavaScriptFrame* frame, StackFrame::Id frame_id,
                               List<int>* results_out) {
  FrameSummary summary = GetFirstFrameSummary(frame);

  Handle<JSFunction> fun = Handle<JSFunction>(summary.function());
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>(fun->shared());

  if (!EnsureDebugInfo(shared, fun)) return;

  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  // Refresh frame summary if the code has been recompiled for debugging.
  if (shared->code() != *summary.code()) summary = GetFirstFrameSummary(frame);

  // Find range of break points starting from the break point where execution
  // has stopped.
  Address call_pc = summary.pc() - 1;
  List<BreakLocation> locations;
  BreakLocation::FromAddressSameStatement(debug_info, call_pc, &locations);

  for (BreakLocation location : locations) {
    if (location.pc() <= summary.pc()) {
      // The break point is near our pc. Could be a step-in possibility,
      // that is currently taken by active debugger call.
      if (break_frame_id() == StackFrame::NO_ID) {
        continue;  // We are not stepping.
      } else {
        JavaScriptFrameIterator frame_it(isolate_, break_frame_id());
        // If our frame is a top frame and we are stepping, we can do step-in
        // at this place.
        if (frame_it.frame()->id() != frame_id) continue;
      }
    }
    if (location.IsCall()) results_out->Add(location.position());
  }
}


void Debug::RecordEvalCaller(Handle<Script> script) {
  script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
  // For eval scripts add information on the function from which eval was
  // called.
  StackTraceFrameIterator it(script->GetIsolate());
  if (!it.done()) {
    script->set_eval_from_shared(it.frame()->function()->shared());
    Code* code = it.frame()->LookupCode();
    int offset = static_cast<int>(
        it.frame()->pc() - code->instruction_start());
    script->set_eval_from_instructions_offset(offset);
  }
}


MaybeHandle<Object> Debug::MakeExecutionState() {
  // Create the execution state object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()) };
  return CallFunction("MakeExecutionState", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeBreakEvent(Handle<Object> break_points_hit) {
  // Create the new break event object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()),
                            break_points_hit };
  return CallFunction("MakeBreakEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeExceptionEvent(Handle<Object> exception,
                                              bool uncaught,
                                              Handle<Object> promise) {
  // Create the new exception event object.
  Handle<Object> argv[] = { isolate_->factory()->NewNumberFromInt(break_id()),
                            exception,
                            isolate_->factory()->ToBoolean(uncaught),
                            promise };
  return CallFunction("MakeExceptionEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeCompileEvent(Handle<Script> script,
                                            v8::DebugEvent type) {
  // Create the compile event object.
  Handle<Object> script_wrapper = Script::GetWrapper(script);
  Handle<Object> argv[] = { script_wrapper,
                            isolate_->factory()->NewNumberFromInt(type) };
  return CallFunction("MakeCompileEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakePromiseEvent(Handle<JSObject> event_data) {
  // Create the promise event object.
  Handle<Object> argv[] = { event_data };
  return CallFunction("MakePromiseEvent", arraysize(argv), argv);
}


MaybeHandle<Object> Debug::MakeAsyncTaskEvent(Handle<JSObject> task_event) {
  // Create the async task event object.
  Handle<Object> argv[] = { task_event };
  return CallFunction("MakeAsyncTaskEvent", arraysize(argv), argv);
}


void Debug::OnThrow(Handle<Object> exception) {
  if (in_debug_scope() || ignore_events()) return;
  PrepareStepOnThrow();
  // Temporarily clear any scheduled_exception to allow evaluating
  // JavaScript from the debug event handler.
  HandleScope scope(isolate_);
  Handle<Object> scheduled_exception;
  if (isolate_->has_scheduled_exception()) {
    scheduled_exception = handle(isolate_->scheduled_exception(), isolate_);
    isolate_->clear_scheduled_exception();
  }
  OnException(exception, isolate_->GetPromiseOnStackOnThrow());
  if (!scheduled_exception.is_null()) {
    isolate_->thread_local_top()->scheduled_exception_ = *scheduled_exception;
  }
}


void Debug::OnPromiseReject(Handle<JSObject> promise, Handle<Object> value) {
  if (in_debug_scope() || ignore_events()) return;
  HandleScope scope(isolate_);
  // Check whether the promise has been marked as having triggered a message.
  Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
  if (JSReceiver::GetDataProperty(promise, key)->IsUndefined()) {
    OnException(value, promise);
  }
}


MaybeHandle<Object> Debug::PromiseHasUserDefinedRejectHandler(
    Handle<JSObject> promise) {
  Handle<JSFunction> fun = isolate_->promise_has_user_defined_reject_handler();
  return Execution::Call(isolate_, fun, promise, 0, NULL);
}


void Debug::OnException(Handle<Object> exception, Handle<Object> promise) {
  // In our prediction, try-finally is not considered to catch.
  Isolate::CatchType catch_type = isolate_->PredictExceptionCatcher();
  bool uncaught = (catch_type == Isolate::NOT_CAUGHT);
  if (promise->IsJSObject()) {
    Handle<JSObject> jspromise = Handle<JSObject>::cast(promise);
    // Mark the promise as already having triggered a message.
    Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
    JSObject::SetProperty(jspromise, key, key, STRICT).Assert();
    // Check whether the promise reject is considered an uncaught exception.
    Handle<Object> has_reject_handler;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, has_reject_handler,
        PromiseHasUserDefinedRejectHandler(jspromise), /* void */);
    uncaught = has_reject_handler->IsFalse();
  }
  // Bail out if exception breaks are not active
  if (uncaught) {
    // Uncaught exceptions are reported by either flags.
    if (!(break_on_uncaught_exception_ || break_on_exception_)) return;
  } else {
    // Caught exceptions are reported is activated.
    if (!break_on_exception_) return;
  }

  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeExceptionEvent(
          exception, uncaught, promise).ToHandle(&event_data)) {
    return;
  }

  // Process debug event.
  ProcessDebugEvent(v8::Exception, Handle<JSObject>::cast(event_data), false);
  // Return to continue execution from where the exception was thrown.
}


void Debug::OnDebugBreak(Handle<Object> break_points_hit,
                            bool auto_continue) {
  // The caller provided for DebugScope.
  AssertDebugContext();
  // Bail out if there is no listener for this event
  if (ignore_events()) return;

  HandleScope scope(isolate_);
  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeBreakEvent(break_points_hit).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::Break,
                    Handle<JSObject>::cast(event_data),
                    auto_continue);
}


void Debug::OnCompileError(Handle<Script> script) {
  ProcessCompileEvent(v8::CompileError, script);
}


void Debug::OnBeforeCompile(Handle<Script> script) {
  ProcessCompileEvent(v8::BeforeCompile, script);
}


// Handle debugger actions when a new script is compiled.
void Debug::OnAfterCompile(Handle<Script> script) {
  ProcessCompileEvent(v8::AfterCompile, script);
}


void Debug::OnPromiseEvent(Handle<JSObject> data) {
  if (in_debug_scope() || ignore_events()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the script collected state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakePromiseEvent(data).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::PromiseEvent,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debug::OnAsyncTaskEvent(Handle<JSObject> data) {
  if (in_debug_scope() || ignore_events()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the script collected state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeAsyncTaskEvent(data).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::AsyncTaskEvent,
                    Handle<JSObject>::cast(event_data),
                    true);
}


void Debug::ProcessDebugEvent(v8::DebugEvent event,
                              Handle<JSObject> event_data,
                              bool auto_continue) {
  HandleScope scope(isolate_);

  // Create the execution state.
  Handle<Object> exec_state;
  // Bail out and don't call debugger if exception.
  if (!MakeExecutionState().ToHandle(&exec_state)) return;

  // First notify the message handler if any.
  if (message_handler_ != NULL) {
    NotifyMessageHandler(event,
                         Handle<JSObject>::cast(exec_state),
                         event_data,
                         auto_continue);
  }
  // Notify registered debug event listener. This can be either a C or
  // a JavaScript function. Don't call event listener for v8::Break
  // here, if it's only a debug command -- they will be processed later.
  if ((event != v8::Break || !auto_continue) && !event_listener_.is_null()) {
    CallEventCallback(event, exec_state, event_data, NULL);
  }
}


void Debug::CallEventCallback(v8::DebugEvent event,
                              Handle<Object> exec_state,
                              Handle<Object> event_data,
                              v8::Debug::ClientData* client_data) {
  // Prevent other interrupts from triggering, for example API callbacks,
  // while dispatching event listners.
  PostponeInterruptsScope postpone(isolate_);
  bool previous = in_debug_event_listener_;
  in_debug_event_listener_ = true;
  if (event_listener_->IsForeign()) {
    // Invoke the C debug event listener.
    v8::Debug::EventCallback callback =
        FUNCTION_CAST<v8::Debug::EventCallback>(
            Handle<Foreign>::cast(event_listener_)->foreign_address());
    EventDetailsImpl event_details(event,
                                   Handle<JSObject>::cast(exec_state),
                                   Handle<JSObject>::cast(event_data),
                                   event_listener_data_,
                                   client_data);
    callback(event_details);
    DCHECK(!isolate_->has_scheduled_exception());
  } else {
    // Invoke the JavaScript debug event listener.
    DCHECK(event_listener_->IsJSFunction());
    Handle<Object> argv[] = { Handle<Object>(Smi::FromInt(event), isolate_),
                              exec_state,
                              event_data,
                              event_listener_data_ };
    Handle<JSReceiver> global(isolate_->global_proxy());
    Execution::TryCall(isolate_, Handle<JSFunction>::cast(event_listener_),
                       global, arraysize(argv), argv);
  }
  in_debug_event_listener_ = previous;
}


void Debug::ProcessCompileEvent(v8::DebugEvent event, Handle<Script> script) {
  if (ignore_events()) return;
  SuppressDebug while_processing(this);

  bool in_nested_debug_scope = in_debug_scope();
  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  if (event == v8::AfterCompile) {
    // If debugging there might be script break points registered for this
    // script. Make sure that these break points are set.
    Handle<Object> argv[] = {Script::GetWrapper(script)};
    if (CallFunction("UpdateScriptBreakPoints", arraysize(argv), argv)
            .is_null()) {
      return;
    }
  }

  // Create the compile state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeCompileEvent(script, event).ToHandle(&event_data)) return;

  // Don't call NotifyMessageHandler if already in debug scope to avoid running
  // nested command loop.
  if (in_nested_debug_scope) {
    if (event_listener_.is_null()) return;
    // Create the execution state.
    Handle<Object> exec_state;
    // Bail out and don't call debugger if exception.
    if (!MakeExecutionState().ToHandle(&exec_state)) return;

    CallEventCallback(event, exec_state, event_data, NULL);
  } else {
    // Process debug event.
    ProcessDebugEvent(event, Handle<JSObject>::cast(event_data), true);
  }
}


Handle<Context> Debug::GetDebugContext() {
  if (!is_loaded()) return Handle<Context>();
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return Handle<Context>();
  // The global handle may be destroyed soon after.  Return it reboxed.
  return handle(*debug_context(), isolate_);
}


void Debug::NotifyMessageHandler(v8::DebugEvent event,
                                 Handle<JSObject> exec_state,
                                 Handle<JSObject> event_data,
                                 bool auto_continue) {
  // Prevent other interrupts from triggering, for example API callbacks,
  // while dispatching message handler callbacks.
  PostponeInterruptsScope no_interrupts(isolate_);
  DCHECK(is_active_);
  HandleScope scope(isolate_);
  // Process the individual events.
  bool sendEventMessage = false;
  switch (event) {
    case v8::Break:
      sendEventMessage = !auto_continue;
      break;
    case v8::NewFunction:
    case v8::BeforeCompile:
    case v8::CompileError:
    case v8::PromiseEvent:
    case v8::AsyncTaskEvent:
      break;
    case v8::Exception:
    case v8::AfterCompile:
      sendEventMessage = true;
      break;
  }

  // The debug command interrupt flag might have been set when the command was
  // added. It should be enough to clear the flag only once while we are in the
  // debugger.
  DCHECK(in_debug_scope());
  isolate_->stack_guard()->ClearDebugCommand();

  // Notify the debugger that a debug event has occurred unless auto continue is
  // active in which case no event is send.
  if (sendEventMessage) {
    MessageImpl message = MessageImpl::NewEvent(
        event,
        auto_continue,
        Handle<JSObject>::cast(exec_state),
        Handle<JSObject>::cast(event_data));
    InvokeMessageHandler(message);
  }

  // If auto continue don't make the event cause a break, but process messages
  // in the queue if any. For script collected events don't even process
  // messages in the queue as the execution state might not be what is expected
  // by the client.
  if (auto_continue && !has_commands()) return;

  // DebugCommandProcessor goes here.
  bool running = auto_continue;

  Handle<Object> cmd_processor_ctor = Object::GetProperty(
      isolate_, exec_state, "debugCommandProcessor").ToHandleChecked();
  Handle<Object> ctor_args[] = { isolate_->factory()->ToBoolean(running) };
  Handle<Object> cmd_processor = Execution::Call(
      isolate_, cmd_processor_ctor, exec_state, 1, ctor_args).ToHandleChecked();
  Handle<JSFunction> process_debug_request = Handle<JSFunction>::cast(
      Object::GetProperty(
          isolate_, cmd_processor, "processDebugRequest").ToHandleChecked());
  Handle<Object> is_running = Object::GetProperty(
      isolate_, cmd_processor, "isRunning").ToHandleChecked();

  // Process requests from the debugger.
  do {
    // Wait for new command in the queue.
    command_received_.Wait();

    // Get the command from the queue.
    CommandMessage command = command_queue_.Get();
    isolate_->logger()->DebugTag(
        "Got request from command queue, in interactive loop.");
    if (!is_active()) {
      // Delete command text and user data.
      command.Dispose();
      return;
    }

    Vector<const uc16> command_text(
        const_cast<const uc16*>(command.text().start()),
        command.text().length());
    Handle<String> request_text = isolate_->factory()->NewStringFromTwoByte(
        command_text).ToHandleChecked();
    Handle<Object> request_args[] = { request_text };
    Handle<Object> answer_value;
    Handle<String> answer;
    MaybeHandle<Object> maybe_exception;
    MaybeHandle<Object> maybe_result =
        Execution::TryCall(isolate_, process_debug_request, cmd_processor, 1,
                           request_args, &maybe_exception);

    if (maybe_result.ToHandle(&answer_value)) {
      if (answer_value->IsUndefined()) {
        answer = isolate_->factory()->empty_string();
      } else {
        answer = Handle<String>::cast(answer_value);
      }

      // Log the JSON request/response.
      if (FLAG_trace_debug_json) {
        PrintF("%s\n", request_text->ToCString().get());
        PrintF("%s\n", answer->ToCString().get());
      }

      Handle<Object> is_running_args[] = { answer };
      maybe_result = Execution::Call(
          isolate_, is_running, cmd_processor, 1, is_running_args);
      Handle<Object> result;
      if (!maybe_result.ToHandle(&result)) break;
      running = result->IsTrue();
    } else {
      Handle<Object> exception;
      if (!maybe_exception.ToHandle(&exception)) break;
      Handle<Object> result;
      if (!Object::ToString(isolate_, exception).ToHandle(&result)) break;
      answer = Handle<String>::cast(result);
    }

    // Return the result.
    MessageImpl message = MessageImpl::NewResponse(
        event, running, exec_state, event_data, answer, command.client_data());
    InvokeMessageHandler(message);
    command.Dispose();

    // Return from debug event processing if either the VM is put into the
    // running state (through a continue command) or auto continue is active
    // and there are no more commands queued.
  } while (!running || has_commands());
  command_queue_.Clear();
}


void Debug::SetEventListener(Handle<Object> callback,
                             Handle<Object> data) {
  GlobalHandles* global_handles = isolate_->global_handles();

  // Remove existing entry.
  GlobalHandles::Destroy(event_listener_.location());
  event_listener_ = Handle<Object>();
  GlobalHandles::Destroy(event_listener_data_.location());
  event_listener_data_ = Handle<Object>();

  // Set new entry.
  if (!callback->IsUndefined() && !callback->IsNull()) {
    event_listener_ = global_handles->Create(*callback);
    if (data.is_null()) data = isolate_->factory()->undefined_value();
    event_listener_data_ = global_handles->Create(*data);
  }

  UpdateState();
}


void Debug::SetMessageHandler(v8::Debug::MessageHandler handler) {
  message_handler_ = handler;
  UpdateState();
  if (handler == NULL && in_debug_scope()) {
    // Send an empty command to the debugger if in a break to make JavaScript
    // run again if the debugger is closed.
    EnqueueCommandMessage(Vector<const uint16_t>::empty());
  }
}



void Debug::UpdateState() {
  bool is_active = message_handler_ != NULL || !event_listener_.is_null();
  if (is_active || in_debug_scope()) {
    // Note that the debug context could have already been loaded to
    // bootstrap test cases.
    isolate_->compilation_cache()->Disable();
    is_active = Load();
  } else if (is_loaded()) {
    isolate_->compilation_cache()->Enable();
    Unload();
  }
  is_active_ = is_active;
}


// Calls the registered debug message handler. This callback is part of the
// public API.
void Debug::InvokeMessageHandler(MessageImpl message) {
  if (message_handler_ != NULL) message_handler_(message);
}


// Puts a command coming from the public API on the queue.  Creates
// a copy of the command string managed by the debugger.  Up to this
// point, the command data was managed by the API client.  Called
// by the API client thread.
void Debug::EnqueueCommandMessage(Vector<const uint16_t> command,
                                  v8::Debug::ClientData* client_data) {
  // Need to cast away const.
  CommandMessage message = CommandMessage::New(
      Vector<uint16_t>(const_cast<uint16_t*>(command.start()),
                       command.length()),
      client_data);
  isolate_->logger()->DebugTag("Put command on command_queue.");
  command_queue_.Put(message);
  command_received_.Signal();

  // Set the debug command break flag to have the command processed.
  if (!in_debug_scope()) isolate_->stack_guard()->RequestDebugCommand();
}


MaybeHandle<Object> Debug::Call(Handle<Object> fun, Handle<Object> data) {
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return isolate_->factory()->undefined_value();

  // Create the execution state.
  Handle<Object> exec_state;
  if (!MakeExecutionState().ToHandle(&exec_state)) {
    return isolate_->factory()->undefined_value();
  }

  Handle<Object> argv[] = { exec_state, data };
  return Execution::Call(
      isolate_,
      fun,
      Handle<Object>(debug_context()->global_proxy(), isolate_),
      arraysize(argv),
      argv);
}


void Debug::HandleDebugBreak() {
  // Ignore debug break during bootstrapping.
  if (isolate_->bootstrapper()->IsActive()) return;
  // Just continue if breaks are disabled.
  if (break_disabled()) return;
  // Ignore debug break if debugger is not active.
  if (!is_active()) return;

  StackLimitCheck check(isolate_);
  if (check.HasOverflowed()) return;

  { JavaScriptFrameIterator it(isolate_);
    DCHECK(!it.done());
    Object* fun = it.frame()->function();
    if (fun && fun->IsJSFunction()) {
      // Don't stop in builtin functions.
      if (!JSFunction::cast(fun)->shared()->IsSubjectToDebugging()) return;
      JSGlobalObject* global =
          JSFunction::cast(fun)->context()->global_object();
      // Don't stop in debugger functions.
      if (IsDebugGlobal(global)) return;
    }
  }

  // Collect the break state before clearing the flags.
  bool debug_command_only = isolate_->stack_guard()->CheckDebugCommand() &&
                            !isolate_->stack_guard()->CheckDebugBreak();

  isolate_->stack_guard()->ClearDebugBreak();

  // Clear stepping to avoid duplicate breaks.
  ClearStepping();

  ProcessDebugMessages(debug_command_only);
}


void Debug::ProcessDebugMessages(bool debug_command_only) {
  isolate_->stack_guard()->ClearDebugCommand();

  StackLimitCheck check(isolate_);
  if (check.HasOverflowed()) return;

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Notify the debug event listeners. Indicate auto continue if the break was
  // a debug command break.
  OnDebugBreak(isolate_->factory()->undefined_value(), debug_command_only);
}


DebugScope::DebugScope(Debug* debug)
    : debug_(debug),
      prev_(debug->debugger_entry()),
      save_(debug_->isolate_),
      no_termination_exceptons_(debug_->isolate_,
                                StackGuard::TERMINATE_EXECUTION) {
  // Link recursive debugger entry.
  base::NoBarrier_Store(&debug_->thread_local_.current_debug_scope_,
                        reinterpret_cast<base::AtomicWord>(this));

  // Store the previous break id and frame id.
  break_id_ = debug_->break_id();
  break_frame_id_ = debug_->break_frame_id();

  // Create the new break info. If there is no JavaScript frames there is no
  // break frame id.
  JavaScriptFrameIterator it(isolate());
  bool has_js_frames = !it.done();
  debug_->thread_local_.break_frame_id_ = has_js_frames ? it.frame()->id()
                                                        : StackFrame::NO_ID;
  debug_->SetNextBreakId();

  debug_->UpdateState();
  // Make sure that debugger is loaded and enter the debugger context.
  // The previous context is kept in save_.
  failed_ = !debug_->is_loaded();
  if (!failed_) isolate()->set_context(*debug->debug_context());
}


DebugScope::~DebugScope() {
  if (!failed_ && prev_ == NULL) {
    // Clear mirror cache when leaving the debugger. Skip this if there is a
    // pending exception as clearing the mirror cache calls back into
    // JavaScript. This can happen if the v8::Debug::Call is used in which
    // case the exception should end up in the calling code.
    if (!isolate()->has_pending_exception()) debug_->ClearMirrorCache();

    // If there are commands in the queue when leaving the debugger request
    // that these commands are processed.
    if (debug_->has_commands()) isolate()->stack_guard()->RequestDebugCommand();
  }

  // Leaving this debugger entry.
  base::NoBarrier_Store(&debug_->thread_local_.current_debug_scope_,
                        reinterpret_cast<base::AtomicWord>(prev_));

  // Restore to the previous break state.
  debug_->thread_local_.break_frame_id_ = break_frame_id_;
  debug_->thread_local_.break_id_ = break_id_;

  debug_->UpdateState();
}


MessageImpl MessageImpl::NewEvent(DebugEvent event,
                                  bool running,
                                  Handle<JSObject> exec_state,
                                  Handle<JSObject> event_data) {
  MessageImpl message(true, event, running,
                      exec_state, event_data, Handle<String>(), NULL);
  return message;
}


MessageImpl MessageImpl::NewResponse(DebugEvent event,
                                     bool running,
                                     Handle<JSObject> exec_state,
                                     Handle<JSObject> event_data,
                                     Handle<String> response_json,
                                     v8::Debug::ClientData* client_data) {
  MessageImpl message(false, event, running,
                      exec_state, event_data, response_json, client_data);
  return message;
}


MessageImpl::MessageImpl(bool is_event,
                         DebugEvent event,
                         bool running,
                         Handle<JSObject> exec_state,
                         Handle<JSObject> event_data,
                         Handle<String> response_json,
                         v8::Debug::ClientData* client_data)
    : is_event_(is_event),
      event_(event),
      running_(running),
      exec_state_(exec_state),
      event_data_(event_data),
      response_json_(response_json),
      client_data_(client_data) {}


bool MessageImpl::IsEvent() const {
  return is_event_;
}


bool MessageImpl::IsResponse() const {
  return !is_event_;
}


DebugEvent MessageImpl::GetEvent() const {
  return event_;
}


bool MessageImpl::WillStartRunning() const {
  return running_;
}


v8::Local<v8::Object> MessageImpl::GetExecutionState() const {
  return v8::Utils::ToLocal(exec_state_);
}


v8::Isolate* MessageImpl::GetIsolate() const {
  return reinterpret_cast<v8::Isolate*>(exec_state_->GetIsolate());
}


v8::Local<v8::Object> MessageImpl::GetEventData() const {
  return v8::Utils::ToLocal(event_data_);
}


v8::Local<v8::String> MessageImpl::GetJSON() const {
  Isolate* isolate = event_data_->GetIsolate();
  v8::EscapableHandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));

  if (IsEvent()) {
    // Call toJSONProtocol on the debug event object.
    Handle<Object> fun = Object::GetProperty(
        isolate, event_data_, "toJSONProtocol").ToHandleChecked();
    if (!fun->IsJSFunction()) {
      return v8::Local<v8::String>();
    }

    MaybeHandle<Object> maybe_json =
        Execution::TryCall(isolate, fun, event_data_, 0, NULL);
    Handle<Object> json;
    if (!maybe_json.ToHandle(&json) || !json->IsString()) {
      return v8::Local<v8::String>();
    }
    return scope.Escape(v8::Utils::ToLocal(Handle<String>::cast(json)));
  } else {
    return v8::Utils::ToLocal(response_json_);
  }
}


v8::Local<v8::Context> MessageImpl::GetEventContext() const {
  Isolate* isolate = event_data_->GetIsolate();
  v8::Local<v8::Context> context = GetDebugEventContext(isolate);
  // Isolate::context() may be NULL when "script collected" event occurs.
  DCHECK(!context.IsEmpty());
  return context;
}


v8::Debug::ClientData* MessageImpl::GetClientData() const {
  return client_data_;
}


EventDetailsImpl::EventDetailsImpl(DebugEvent event,
                                   Handle<JSObject> exec_state,
                                   Handle<JSObject> event_data,
                                   Handle<Object> callback_data,
                                   v8::Debug::ClientData* client_data)
    : event_(event),
      exec_state_(exec_state),
      event_data_(event_data),
      callback_data_(callback_data),
      client_data_(client_data) {}


DebugEvent EventDetailsImpl::GetEvent() const {
  return event_;
}


v8::Local<v8::Object> EventDetailsImpl::GetExecutionState() const {
  return v8::Utils::ToLocal(exec_state_);
}


v8::Local<v8::Object> EventDetailsImpl::GetEventData() const {
  return v8::Utils::ToLocal(event_data_);
}


v8::Local<v8::Context> EventDetailsImpl::GetEventContext() const {
  return GetDebugEventContext(exec_state_->GetIsolate());
}


v8::Local<v8::Value> EventDetailsImpl::GetCallbackData() const {
  return v8::Utils::ToLocal(callback_data_);
}


v8::Debug::ClientData* EventDetailsImpl::GetClientData() const {
  return client_data_;
}


CommandMessage::CommandMessage() : text_(Vector<uint16_t>::empty()),
                                   client_data_(NULL) {
}


CommandMessage::CommandMessage(const Vector<uint16_t>& text,
                               v8::Debug::ClientData* data)
    : text_(text),
      client_data_(data) {
}


void CommandMessage::Dispose() {
  text_.Dispose();
  delete client_data_;
  client_data_ = NULL;
}


CommandMessage CommandMessage::New(const Vector<uint16_t>& command,
                                   v8::Debug::ClientData* data) {
  return CommandMessage(command.Clone(), data);
}


CommandMessageQueue::CommandMessageQueue(int size) : start_(0), end_(0),
                                                     size_(size) {
  messages_ = NewArray<CommandMessage>(size);
}


CommandMessageQueue::~CommandMessageQueue() {
  while (!IsEmpty()) Get().Dispose();
  DeleteArray(messages_);
}


CommandMessage CommandMessageQueue::Get() {
  DCHECK(!IsEmpty());
  int result = start_;
  start_ = (start_ + 1) % size_;
  return messages_[result];
}


void CommandMessageQueue::Put(const CommandMessage& message) {
  if ((end_ + 1) % size_ == start_) {
    Expand();
  }
  messages_[end_] = message;
  end_ = (end_ + 1) % size_;
}


void CommandMessageQueue::Expand() {
  CommandMessageQueue new_queue(size_ * 2);
  while (!IsEmpty()) {
    new_queue.Put(Get());
  }
  CommandMessage* array_to_free = messages_;
  *this = new_queue;
  new_queue.messages_ = array_to_free;
  // Make the new_queue empty so that it doesn't call Dispose on any messages.
  new_queue.start_ = new_queue.end_;
  // Automatic destructor called on new_queue, freeing array_to_free.
}


LockingCommandMessageQueue::LockingCommandMessageQueue(Logger* logger, int size)
    : logger_(logger), queue_(size) {}


bool LockingCommandMessageQueue::IsEmpty() const {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  return queue_.IsEmpty();
}


CommandMessage LockingCommandMessageQueue::Get() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  CommandMessage result = queue_.Get();
  logger_->DebugEvent("Get", result.text());
  return result;
}


void LockingCommandMessageQueue::Put(const CommandMessage& message) {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  queue_.Put(message);
  logger_->DebugEvent("Put", message.text());
}


void LockingCommandMessageQueue::Clear() {
  base::LockGuard<base::Mutex> lock_guard(&mutex_);
  queue_.Clear();
}

}  // namespace internal
}  // namespace v8
