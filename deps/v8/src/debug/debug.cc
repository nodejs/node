// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug.h"

#include <memory>

#include "src/api.h"
#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/global-handles.h"
#include "src/globals.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/list.h"
#include "src/log.h"
#include "src/messages.h"
#include "src/snapshot/natives.h"
#include "src/wasm/wasm-module.h"

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
      hook_on_function_call_(false),
      is_suppressed_(false),
      live_edit_enabled_(true),  // TODO(yangguo): set to false by default.
      break_disabled_(false),
      break_points_active_(true),
      in_debug_event_listener_(false),
      break_on_exception_(false),
      break_on_uncaught_exception_(false),
      side_effect_check_failed_(false),
      debug_info_list_(NULL),
      feature_tracker_(isolate),
      isolate_(isolate) {
  ThreadInit();
}

BreakLocation BreakLocation::FromFrame(StandardFrame* frame) {
  // TODO(clemensh): Handle Wasm frames.
  DCHECK(!frame->is_wasm());

  auto summary = FrameSummary::GetFirst(frame).AsJavaScript();
  int offset = summary.code_offset();
  Handle<AbstractCode> abstract_code = summary.abstract_code();
  if (abstract_code->IsCode()) offset = offset - 1;
  Handle<DebugInfo> debug_info(summary.function()->shared()->GetDebugInfo());
  auto it = BreakIterator::GetIterator(debug_info, abstract_code);
  it->SkipTo(BreakIndexFromCodeOffset(debug_info, abstract_code, offset));
  return it->GetBreakLocation();
}

void BreakLocation::AllAtCurrentStatement(Handle<DebugInfo> debug_info,
                                          JavaScriptFrame* frame,
                                          List<BreakLocation>* result_out) {
  auto summary = FrameSummary::GetFirst(frame).AsJavaScript();
  int offset = summary.code_offset();
  Handle<AbstractCode> abstract_code = summary.abstract_code();
  if (abstract_code->IsCode()) offset = offset - 1;
  int statement_position;
  {
    auto it = BreakIterator::GetIterator(debug_info, abstract_code);
    it->SkipTo(BreakIndexFromCodeOffset(debug_info, abstract_code, offset));
    statement_position = it->statement_position();
  }
  for (auto it = BreakIterator::GetIterator(debug_info, abstract_code);
       !it->Done(); it->Next()) {
    if (it->statement_position() == statement_position) {
      result_out->Add(it->GetBreakLocation());
    }
  }
}

int BreakLocation::BreakIndexFromCodeOffset(Handle<DebugInfo> debug_info,
                                            Handle<AbstractCode> abstract_code,
                                            int offset) {
  // Run through all break points to locate the one closest to the address.
  int closest_break = 0;
  int distance = kMaxInt;
  DCHECK(0 <= offset && offset < abstract_code->Size());
  for (auto it = BreakIterator::GetIterator(debug_info, abstract_code);
       !it->Done(); it->Next()) {
    // Check if this break point is closer that what was previously found.
    if (it->code_offset() <= offset && offset - it->code_offset() < distance) {
      closest_break = it->break_index();
      distance = offset - it->code_offset();
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
  }
  return closest_break;
}

bool BreakLocation::HasBreakPoint(Handle<DebugInfo> debug_info) const {
  // First check whether there is a break point with the same source position.
  if (!debug_info->HasBreakPoint(position_)) return false;
  // Then check whether a break point at that source position would have
  // the same code offset. Otherwise it's just a break location that we can
  // step to, but not actually a location where we can put a break point.
  if (abstract_code_->IsCode()) {
    DCHECK_EQ(debug_info->DebugCode(), abstract_code_->GetCode());
    CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    it.SkipToPosition(position_, BREAK_POSITION_ALIGNED);
    return it.code_offset() == code_offset_;
  } else {
    DCHECK(abstract_code_->IsBytecodeArray());
    BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    it.SkipToPosition(position_, BREAK_POSITION_ALIGNED);
    return it.code_offset() == code_offset_;
  }
}

std::unique_ptr<BreakIterator> BreakIterator::GetIterator(
    Handle<DebugInfo> debug_info, Handle<AbstractCode> abstract_code,
    BreakLocatorType type) {
  if (abstract_code->IsBytecodeArray()) {
    DCHECK(debug_info->HasDebugBytecodeArray());
    return std::unique_ptr<BreakIterator>(
        new BytecodeArrayBreakIterator(debug_info, type));
  } else {
    DCHECK(abstract_code->IsCode());
    DCHECK(debug_info->HasDebugCode());
    return std::unique_ptr<BreakIterator>(
        new CodeBreakIterator(debug_info, type));
  }
}

BreakIterator::BreakIterator(Handle<DebugInfo> debug_info,
                             BreakLocatorType type)
    : debug_info_(debug_info), break_index_(-1), break_locator_type_(type) {
  position_ = debug_info->shared()->start_position();
  statement_position_ = position_;
}

int BreakIterator::BreakIndexFromPosition(int source_position,
                                          BreakPositionAlignment alignment) {
  int distance = kMaxInt;
  int closest_break = break_index();
  while (!Done()) {
    int next_position;
    if (alignment == STATEMENT_ALIGNED) {
      next_position = statement_position();
    } else {
      DCHECK(alignment == BREAK_POSITION_ALIGNED);
      next_position = position();
    }
    if (source_position <= next_position &&
        next_position - source_position < distance) {
      closest_break = break_index();
      distance = next_position - source_position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    Next();
  }
  return closest_break;
}

CodeBreakIterator::CodeBreakIterator(Handle<DebugInfo> debug_info,
                                     BreakLocatorType type)
    : BreakIterator(debug_info, type),
      reloc_iterator_(debug_info->DebugCode(), GetModeMask(type)),
      source_position_iterator_(
          debug_info->DebugCode()->source_position_table()) {
  // There is at least one break location.
  DCHECK(!Done());
  Next();
}

int CodeBreakIterator::GetModeMask(BreakLocatorType type) {
  int mask = 0;
  mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_RETURN);
  mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_CALL);
  if (isolate()->is_tail_call_elimination_enabled()) {
    mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_TAIL_CALL);
  }
  if (type == ALL_BREAK_LOCATIONS) {
    mask |= RelocInfo::ModeMask(RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION);
    mask |= RelocInfo::ModeMask(RelocInfo::DEBUGGER_STATEMENT);
  }
  return mask;
}

void CodeBreakIterator::Next() {
  DisallowHeapAllocation no_gc;
  DCHECK(!Done());

  // Iterate through reloc info stopping at each breakable code target.
  bool first = break_index_ == -1;

  if (!first) reloc_iterator_.next();
  first = false;
  if (Done()) return;

  int offset = code_offset();
  while (!source_position_iterator_.done() &&
         source_position_iterator_.code_offset() <= offset) {
    position_ = source_position_iterator_.source_position().ScriptOffset();
    if (source_position_iterator_.is_statement()) {
      statement_position_ = position_;
    }
    source_position_iterator_.Advance();
  }

  DCHECK(RelocInfo::IsDebugBreakSlot(rmode()) ||
         RelocInfo::IsDebuggerStatement(rmode()));
  break_index_++;
}

DebugBreakType CodeBreakIterator::GetDebugBreakType() {
  if (RelocInfo::IsDebugBreakSlotAtReturn(rmode())) {
    return DEBUG_BREAK_SLOT_AT_RETURN;
  } else if (RelocInfo::IsDebugBreakSlotAtCall(rmode())) {
    return DEBUG_BREAK_SLOT_AT_CALL;
  } else if (RelocInfo::IsDebugBreakSlotAtTailCall(rmode())) {
    return isolate()->is_tail_call_elimination_enabled()
               ? DEBUG_BREAK_SLOT_AT_TAIL_CALL
               : DEBUG_BREAK_SLOT_AT_CALL;
  } else if (RelocInfo::IsDebuggerStatement(rmode())) {
    return DEBUGGER_STATEMENT;
  } else if (RelocInfo::IsDebugBreakSlot(rmode())) {
    return DEBUG_BREAK_SLOT;
  } else {
    return NOT_DEBUG_BREAK;
  }
}

void CodeBreakIterator::SkipToPosition(int position,
                                       BreakPositionAlignment alignment) {
  CodeBreakIterator it(debug_info_, break_locator_type_);
  SkipTo(it.BreakIndexFromPosition(position, alignment));
}

void CodeBreakIterator::SetDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  Builtins* builtins = isolate()->builtins();
  Handle<Code> target = debug_break_type == DEBUG_BREAK_SLOT_AT_RETURN
                            ? builtins->Return_DebugBreak()
                            : builtins->Slot_DebugBreak();
  DebugCodegen::PatchDebugBreakSlot(isolate(), rinfo()->pc(), target);
}

void CodeBreakIterator::ClearDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  DebugCodegen::ClearDebugBreakSlot(isolate(), rinfo()->pc());
}

bool CodeBreakIterator::IsDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return false;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  return DebugCodegen::DebugBreakSlotIsPatched(rinfo()->pc());
}

BreakLocation CodeBreakIterator::GetBreakLocation() {
  Handle<AbstractCode> code(AbstractCode::cast(debug_info_->DebugCode()));
  return BreakLocation(code, GetDebugBreakType(), code_offset(), position_);
}

BytecodeArrayBreakIterator::BytecodeArrayBreakIterator(
    Handle<DebugInfo> debug_info, BreakLocatorType type)
    : BreakIterator(debug_info, type),
      source_position_iterator_(
          debug_info->DebugBytecodeArray()->source_position_table()) {
  // There is at least one break location.
  DCHECK(!Done());
  Next();
}

void BytecodeArrayBreakIterator::Next() {
  DisallowHeapAllocation no_gc;
  DCHECK(!Done());
  bool first = break_index_ == -1;
  while (!Done()) {
    if (!first) source_position_iterator_.Advance();
    first = false;
    if (Done()) return;
    position_ = source_position_iterator_.source_position().ScriptOffset();
    if (source_position_iterator_.is_statement()) {
      statement_position_ = position_;
    }
    DCHECK(position_ >= 0);
    DCHECK(statement_position_ >= 0);

    DebugBreakType type = GetDebugBreakType();
    if (type == NOT_DEBUG_BREAK) continue;

    if (break_locator_type_ == ALL_BREAK_LOCATIONS) break;

    DCHECK_EQ(CALLS_AND_RETURNS, break_locator_type_);
    if (type == DEBUG_BREAK_SLOT_AT_CALL) break;
    if (type == DEBUG_BREAK_SLOT_AT_RETURN) break;
  }
  break_index_++;
}

DebugBreakType BytecodeArrayBreakIterator::GetDebugBreakType() {
  BytecodeArray* bytecode_array = debug_info_->OriginalBytecodeArray();
  interpreter::Bytecode bytecode =
      interpreter::Bytecodes::FromByte(bytecode_array->get(code_offset()));

  if (bytecode == interpreter::Bytecode::kDebugger) {
    return DEBUGGER_STATEMENT;
  } else if (bytecode == interpreter::Bytecode::kReturn) {
    return DEBUG_BREAK_SLOT_AT_RETURN;
  } else if (bytecode == interpreter::Bytecode::kTailCall) {
    return isolate()->is_tail_call_elimination_enabled()
               ? DEBUG_BREAK_SLOT_AT_TAIL_CALL
               : DEBUG_BREAK_SLOT_AT_CALL;
  } else if (interpreter::Bytecodes::IsCallOrNew(bytecode)) {
    return DEBUG_BREAK_SLOT_AT_CALL;
  } else if (source_position_iterator_.is_statement()) {
    return DEBUG_BREAK_SLOT;
  } else {
    return NOT_DEBUG_BREAK;
  }
}

void BytecodeArrayBreakIterator::SkipToPosition(
    int position, BreakPositionAlignment alignment) {
  BytecodeArrayBreakIterator it(debug_info_, break_locator_type_);
  SkipTo(it.BreakIndexFromPosition(position, alignment));
}

void BytecodeArrayBreakIterator::SetDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  BytecodeArray* bytecode_array = debug_info_->DebugBytecodeArray();
  interpreter::Bytecode bytecode =
      interpreter::Bytecodes::FromByte(bytecode_array->get(code_offset()));
  if (interpreter::Bytecodes::IsDebugBreak(bytecode)) return;
  interpreter::Bytecode debugbreak =
      interpreter::Bytecodes::GetDebugBreak(bytecode);
  bytecode_array->set(code_offset(),
                      interpreter::Bytecodes::ToByte(debugbreak));
}

void BytecodeArrayBreakIterator::ClearDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  BytecodeArray* bytecode_array = debug_info_->DebugBytecodeArray();
  BytecodeArray* original = debug_info_->OriginalBytecodeArray();
  bytecode_array->set(code_offset(), original->get(code_offset()));
}

bool BytecodeArrayBreakIterator::IsDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return false;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  BytecodeArray* bytecode_array = debug_info_->DebugBytecodeArray();
  interpreter::Bytecode bytecode =
      interpreter::Bytecodes::FromByte(bytecode_array->get(code_offset()));
  return interpreter::Bytecodes::IsDebugBreak(bytecode);
}

BreakLocation BytecodeArrayBreakIterator::GetBreakLocation() {
  Handle<AbstractCode> code(
      AbstractCode::cast(debug_info_->DebugBytecodeArray()));
  return BreakLocation(code, GetDebugBreakType(), code_offset(), position_);
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
  thread_local_.last_statement_position_ = kNoSourcePosition;
  thread_local_.last_fp_ = 0;
  thread_local_.target_fp_ = 0;
  thread_local_.return_value_ = Handle<Object>();
  thread_local_.async_task_count_ = 0;
  clear_suspended_generator();
  // TODO(isolates): frames_are_dropped_?
  base::NoBarrier_Store(&thread_local_.current_debug_scope_,
                        static_cast<base::AtomicWord>(0));
  UpdateHookOnFunctionCall();
}


char* Debug::ArchiveDebug(char* storage) {
  // Simply reset state. Don't archive anything.
  ThreadInit();
  return storage + ArchiveSpacePerThread();
}


char* Debug::RestoreDebug(char* storage) {
  // Simply reset state. Don't restore anything.
  ThreadInit();
  return storage + ArchiveSpacePerThread();
}

int Debug::ArchiveSpacePerThread() { return 0; }

void Debug::Iterate(ObjectVisitor* v) {
  v->VisitPointer(&thread_local_.suspended_generator_);
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
  DisableBreak disable(this);
  PostponeInterruptsScope postpone(isolate_);

  // Create the debugger context.
  HandleScope scope(isolate_);
  ExtensionConfiguration no_extensions;
  // TODO(yangguo): we rely on the fact that first context snapshot is usable
  //                as debug context. This dependency is gone once we remove
  //                debug context completely.
  static const int kFirstContextSnapshotIndex = 0;
  Handle<Context> context = isolate_->bootstrapper()->CreateEnvironment(
      MaybeHandle<JSGlobalProxy>(), v8::Local<ObjectTemplate>(), &no_extensions,
      kFirstContextSnapshotIndex, v8::DeserializeInternalFieldsCallback(),
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

void Debug::Break(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // Initialize LiveEdit.
  LiveEdit::InitializeThreadLocal(this);

  // Just continue if breaks are disabled or debugger cannot be loaded.
  if (break_disabled()) return;

  // Enter the debugger.
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Postpone interrupt during breakpoint processing.
  PostponeInterruptsScope postpone(isolate_);

  // Return if we fail to retrieve debug info for javascript frames.
  if (frame->is_java_script()) {
    JavaScriptFrame* js_frame = JavaScriptFrame::cast(frame);

    // Get the debug info (create it if it does not exist).
    Handle<JSFunction> function(js_frame->function());
    Handle<SharedFunctionInfo> shared(function->shared());
    if (!EnsureDebugInfo(shared, function)) return;
  }

  BreakLocation location = BreakLocation::FromFrame(frame);

  // Find actual break points, if any, and trigger debug break event.
  MaybeHandle<FixedArray> break_points_hit;
  if (!break_points_active()) {
    // Don't try to find hit breakpoints.
  } else if (frame->is_wasm_interpreter_entry()) {
    // TODO(clemensh): Find hit breakpoints for wasm.
    UNIMPLEMENTED();
  } else {
    // Get the debug info, which must exist if we reach here.
    Handle<DebugInfo> debug_info(
        JavaScriptFrame::cast(frame)->function()->shared()->GetDebugInfo(),
        isolate_);

    break_points_hit = CheckBreakPoints(debug_info, &location);
  }

  if (!break_points_hit.is_null()) {
    // Clear all current stepping setup.
    ClearStepping();
    // Notify the debug event listeners.
    Handle<JSArray> jsarr = isolate_->factory()->NewJSArrayWithElements(
        break_points_hit.ToHandleChecked());
    OnDebugBreak(jsarr, false);
    return;
  }

  // No break point. Check for stepping.
  StepAction step_action = last_step_action();
  Address current_fp = frame->UnpaddedFP();
  Address target_fp = thread_local_.target_fp_;
  Address last_fp = thread_local_.last_fp_;

  bool step_break = false;
  switch (step_action) {
    case StepNone:
      return;
    case StepOut:
      // Step out has not reached the target frame yet.
      if (current_fp < target_fp) return;
      step_break = true;
      break;
    case StepNext:
      // Step next should not break in a deeper frame.
      if (current_fp < target_fp) return;
      // For step-next, a tail call is like a return and should break.
      step_break = location.IsTailCall();
    // Fall through.
    case StepIn: {
      FrameSummary summary = FrameSummary::GetFirst(frame);
      step_break = step_break || location.IsReturn() || current_fp != last_fp ||
                   thread_local_.last_statement_position_ !=
                       summary.SourceStatementPosition();
      break;
    }
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


// Find break point objects for this location, if any, and evaluate them.
// Return an array of break point objects that evaluated true, or an empty
// handle if none evaluated true.
MaybeHandle<FixedArray> Debug::CheckBreakPoints(Handle<DebugInfo> debug_info,
                                                BreakLocation* location,
                                                bool* has_break_points) {
  bool has_break_points_to_check =
      break_points_active_ && location->HasBreakPoint(debug_info);
  if (has_break_points) *has_break_points = has_break_points_to_check;
  if (!has_break_points_to_check) return {};

  Handle<Object> break_point_objects =
      debug_info->GetBreakPointObjects(location->position());
  return Debug::GetHitBreakPointObjects(break_point_objects);
}


bool Debug::IsMutedAtCurrentLocation(JavaScriptFrame* frame) {
  // A break location is considered muted if break locations on the current
  // statement have at least one break point, and all of these break points
  // evaluate to false. Aside from not triggering a debug break event at the
  // break location, we also do not trigger one for debugger statements, nor
  // an exception event on exception at this location.
  Object* fun = frame->function();
  if (!fun->IsJSFunction()) return false;
  JSFunction* function = JSFunction::cast(fun);
  if (!function->shared()->HasDebugInfo()) return false;
  HandleScope scope(isolate_);
  Handle<DebugInfo> debug_info(function->shared()->GetDebugInfo());
  // Enter the debugger.
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return false;
  List<BreakLocation> break_locations;
  BreakLocation::AllAtCurrentStatement(debug_info, frame, &break_locations);
  bool has_break_points_at_all = false;
  for (int i = 0; i < break_locations.length(); i++) {
    bool has_break_points;
    MaybeHandle<FixedArray> check_result =
        CheckBreakPoints(debug_info, &break_locations[i], &has_break_points);
    has_break_points_at_all |= has_break_points;
    if (has_break_points && !check_result.is_null()) return false;
  }
  return has_break_points_at_all;
}


MaybeHandle<Object> Debug::CallFunction(const char* name, int argc,
                                        Handle<Object> args[]) {
  PostponeInterruptsScope no_interrupts(isolate_);
  AssertDebugContext();
  Handle<JSReceiver> holder =
      Handle<JSReceiver>::cast(isolate_->natives_utils_object());
  Handle<JSFunction> fun = Handle<JSFunction>::cast(
      JSReceiver::GetProperty(isolate_, holder, name).ToHandleChecked());
  Handle<Object> undefined = isolate_->factory()->undefined_value();
  MaybeHandle<Object> maybe_exception;
  return Execution::TryCall(isolate_, fun, undefined, argc, args,
                            Execution::MessageHandling::kReport,
                            &maybe_exception);
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
  return result->IsTrue(isolate_);
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
  *source_position =
      FindBreakablePosition(debug_info, *source_position, STATEMENT_ALIGNED);
  DebugInfo::SetBreakPoint(debug_info, *source_position, break_point_object);
  // At least one active break point now.
  DCHECK(debug_info->GetBreakPointCount() > 0);

  ClearBreakPoints(debug_info);
  ApplyBreakPoints(debug_info);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);
  return true;
}


bool Debug::SetBreakPointForScript(Handle<Script> script,
                                   Handle<Object> break_point_object,
                                   int* source_position,
                                   BreakPositionAlignment alignment) {
  if (script->type() == Script::TYPE_WASM) {
    // TODO(clemensh): set breakpoint for wasm.
    return false;
  }
  HandleScope scope(isolate_);

  // Obtain shared function info for the function.
  Handle<Object> result =
      FindSharedFunctionInfoInScript(script, *source_position);
  if (result->IsUndefined(isolate_)) return false;

  // Make sure the function has set up the debug info.
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(result);
  if (!EnsureDebugInfo(shared, Handle<JSFunction>::null())) {
    // Return if retrieving debug info failed.
    return false;
  }

  // Find position within function. The script position might be before the
  // source position of the first function.
  if (shared->start_position() > *source_position) {
    *source_position = shared->start_position();
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo());

  // Find the break point and change it.
  *source_position =
      FindBreakablePosition(debug_info, *source_position, alignment);
  DebugInfo::SetBreakPoint(debug_info, *source_position, break_point_object);
  // At least one active break point now.
  DCHECK(debug_info->GetBreakPointCount() > 0);

  ClearBreakPoints(debug_info);
  ApplyBreakPoints(debug_info);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);
  return true;
}

int Debug::FindBreakablePosition(Handle<DebugInfo> debug_info,
                                 int source_position,
                                 BreakPositionAlignment alignment) {
  int statement_position;
  int position;
  if (debug_info->HasDebugCode()) {
    CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    it.SkipToPosition(source_position, alignment);
    statement_position = it.statement_position();
    position = it.position();
  } else {
    DCHECK(debug_info->HasDebugBytecodeArray());
    BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    it.SkipToPosition(source_position, alignment);
    statement_position = it.statement_position();
    position = it.position();
  }
  return alignment == STATEMENT_ALIGNED ? statement_position : position;
}

void Debug::ApplyBreakPoints(Handle<DebugInfo> debug_info) {
  DisallowHeapAllocation no_gc;
  if (debug_info->break_points()->IsUndefined(isolate_)) return;
  FixedArray* break_points = debug_info->break_points();
  for (int i = 0; i < break_points->length(); i++) {
    if (break_points->get(i)->IsUndefined(isolate_)) continue;
    BreakPointInfo* info = BreakPointInfo::cast(break_points->get(i));
    if (info->GetBreakPointCount() == 0) continue;
    if (debug_info->HasDebugCode()) {
      CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
      it.SkipToPosition(info->source_position(), BREAK_POSITION_ALIGNED);
      it.SetDebugBreak();
    }
    if (debug_info->HasDebugBytecodeArray()) {
      BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
      it.SkipToPosition(info->source_position(), BREAK_POSITION_ALIGNED);
      it.SetDebugBreak();
    }
  }
}

void Debug::ClearBreakPoints(Handle<DebugInfo> debug_info) {
  DisallowHeapAllocation no_gc;
  if (debug_info->HasDebugCode()) {
    for (CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS); !it.Done();
         it.Next()) {
      it.ClearDebugBreak();
    }
  }
  if (debug_info->HasDebugBytecodeArray()) {
    for (BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
         !it.Done(); it.Next()) {
      it.ClearDebugBreak();
    }
  }
}

void Debug::ClearBreakPoint(Handle<Object> break_point_object) {
  HandleScope scope(isolate_);

  for (DebugInfoListNode* node = debug_info_list_; node != NULL;
       node = node->next()) {
    Handle<Object> result =
        DebugInfo::FindBreakPointInfo(node->debug_info(), break_point_object);
    if (result->IsUndefined(isolate_)) continue;
    Handle<DebugInfo> debug_info = node->debug_info();
    if (DebugInfo::ClearBreakPoint(debug_info, break_point_object)) {
      ClearBreakPoints(debug_info);
      if (debug_info->GetBreakPointCount() == 0) {
        RemoveDebugInfoAndClearFromShared(debug_info);
      } else {
        ApplyBreakPoints(debug_info);
      }
      return;
    }
  }
}

// Clear out all the debug break code. This is ONLY supposed to be used when
// shutting down the debugger as it will leave the break point information in
// DebugInfo even though the code is patched back to the non break point state.
void Debug::ClearAllBreakPoints() {
  for (DebugInfoListNode* node = debug_info_list_; node != NULL;
       node = node->next()) {
    ClearBreakPoints(node->debug_info());
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
  if (debug_info->HasDebugCode()) {
    for (CodeBreakIterator it(debug_info, type); !it.Done(); it.Next()) {
      it.SetDebugBreak();
    }
  }
  if (debug_info->HasDebugBytecodeArray()) {
    for (BytecodeArrayBreakIterator it(debug_info, type); !it.Done();
         it.Next()) {
      it.SetDebugBreak();
    }
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

MaybeHandle<FixedArray> Debug::GetHitBreakPointObjects(
    Handle<Object> break_point_objects) {
  DCHECK(!break_point_objects->IsUndefined(isolate_));
  if (!break_point_objects->IsFixedArray()) {
    if (!CheckBreakPoint(break_point_objects)) return {};
    Handle<FixedArray> break_points_hit = isolate_->factory()->NewFixedArray(1);
    break_points_hit->set(0, *break_point_objects);
    return break_points_hit;
  }

  Handle<FixedArray> array(FixedArray::cast(*break_point_objects));
  int num_objects = array->length();
  Handle<FixedArray> break_points_hit =
      isolate_->factory()->NewFixedArray(num_objects);
  int break_points_hit_count = 0;
  for (int i = 0; i < num_objects; ++i) {
    Handle<Object> break_point_object(array->get(i), isolate_);
    if (CheckBreakPoint(break_point_object)) {
      break_points_hit->set(break_points_hit_count++, *break_point_object);
    }
  }
  if (break_points_hit_count == 0) return {};
  break_points_hit->Shrink(break_points_hit_count);
  return break_points_hit;
}

void Debug::PrepareStepIn(Handle<JSFunction> function) {
  CHECK(last_step_action() >= StepIn);
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;
  FloodWithOneShot(function);
}

void Debug::PrepareStepInSuspendedGenerator() {
  CHECK(has_suspended_generator());
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;
  thread_local_.last_step_action_ = StepIn;
  UpdateHookOnFunctionCall();
  Handle<JSFunction> function(
      JSGeneratorObject::cast(thread_local_.suspended_generator_)->function());
  FloodWithOneShot(function);
  clear_suspended_generator();
}

void Debug::PrepareStepOnThrow() {
  if (last_step_action() == StepNone) return;
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;

  ClearOneShot();

  // Iterate through the JavaScript stack looking for handlers.
  JavaScriptFrameIterator it(isolate_);
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->LookupExceptionHandlerInTable(nullptr, nullptr) > 0) break;
    it.Advance();
  }

  if (last_step_action() == StepNext || last_step_action() == StepOut) {
    while (!it.done()) {
      Address current_fp = it.frame()->UnpaddedFP();
      if (current_fp >= thread_local_.target_fp_) break;
      it.Advance();
    }
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

  thread_local_.last_step_action_ = step_action;
  UpdateHookOnFunctionCall();

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
  auto summary = FrameSummary::GetFirst(frame).AsJavaScript();
  Handle<JSFunction> function(summary.function());
  Handle<SharedFunctionInfo> shared(function->shared());
  if (!EnsureDebugInfo(shared, function)) {
    // Return if ensuring debug info failed.
    return;
  }

  BreakLocation location = BreakLocation::FromFrame(frame);

  // Any step at a return is a step-out.
  if (location.IsReturn()) step_action = StepOut;
  // A step-next at a tail call is a step-out.
  if (location.IsTailCall() && step_action == StepNext) step_action = StepOut;

  thread_local_.last_statement_position_ =
      summary.abstract_code()->SourceStatementPosition(summary.code_offset());
  thread_local_.last_fp_ = frame->UnpaddedFP();
  // No longer perform the current async step.
  clear_suspended_generator();

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
      if (!frames_it.done()) {
        // Fill the caller function to return to with one-shot break points.
        Handle<JSFunction> caller_function(frames_it.frame()->function());
        FloodWithOneShot(caller_function);
        thread_local_.target_fp_ = frames_it.frame()->UnpaddedFP();
      }
      // Clear last position info. For stepping out it does not matter.
      thread_local_.last_statement_position_ = kNoSourcePosition;
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
  if (!shared->HasDebugInfo()) {
    return isolate->factory()->undefined_value();
  }
  Handle<DebugInfo> debug_info(shared->GetDebugInfo());
  if (debug_info->GetBreakPointCount() == 0) {
    return isolate->factory()->undefined_value();
  }
  Handle<FixedArray> locations =
      isolate->factory()->NewFixedArray(debug_info->GetBreakPointCount());
  int count = 0;
  for (int i = 0; i < debug_info->break_points()->length(); ++i) {
    if (!debug_info->break_points()->get(i)->IsUndefined(isolate)) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(debug_info->break_points()->get(i));
      int break_points = break_point_info->GetBreakPointCount();
      if (break_points == 0) continue;
      Smi* position = NULL;
      if (position_alignment == STATEMENT_ALIGNED) {
        if (debug_info->HasDebugCode()) {
          CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
          it.SkipToPosition(break_point_info->source_position(),
                            BREAK_POSITION_ALIGNED);
          position = Smi::FromInt(it.statement_position());
        } else {
          DCHECK(debug_info->HasDebugBytecodeArray());
          BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
          it.SkipToPosition(break_point_info->source_position(),
                            BREAK_POSITION_ALIGNED);
          position = Smi::FromInt(it.statement_position());
        }
      } else {
        DCHECK_EQ(BREAK_POSITION_ALIGNED, position_alignment);
        position = Smi::FromInt(break_point_info->source_position());
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
  thread_local_.last_statement_position_ = kNoSourcePosition;
  thread_local_.last_fp_ = 0;
  thread_local_.target_fp_ = 0;
  UpdateHookOnFunctionCall();
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
    Handle<DebugInfo> debug_info = node->debug_info();
    ClearBreakPoints(debug_info);
    ApplyBreakPoints(debug_info);
  }
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

      if (frame->is_interpreted()) {
        InterpretedFrame* interpreted_frame =
            reinterpret_cast<InterpretedFrame*>(frame);
        BytecodeArray* debug_copy =
            shared_->GetDebugInfo()->DebugBytecodeArray();
        interpreted_frame->PatchBytecodeArray(debug_copy);
        continue;
      }

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
    isolate_->optimizing_compile_dispatcher()->Flush(
        OptimizingCompileDispatcher::BlockingBehavior::kBlock);
  }

  List<Handle<JSFunction> > functions;

  // Flush all optimized code maps. Note that the below heap iteration does not
  // cover this, because the given function might have been inlined into code
  // for which no JSFunction exists.
  {
    SharedFunctionInfo::GlobalIterator iterator(isolate_);
    while (SharedFunctionInfo* shared = iterator.Next()) {
      shared->ClearCodeFromOptimizedCodeMap();
    }
  }

  // The native context also has a list of OSR'd optimized code. Clear it.
  isolate_->ClearOSROptimizedCode();

  // Make sure we abort incremental marking.
  isolate_->heap()->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                                      GarbageCollectionReason::kDebugger);

  DCHECK(shared->is_compiled());
  bool baseline_exists = shared->HasBaselineCode();

  {
    // TODO(yangguo): with bytecode, we still walk the heap to find all
    // optimized code for the function to deoptimize. We can probably be
    // smarter here and avoid the heap walk.
    HeapIterator iterator(isolate_->heap());
    HeapObject* obj;

    while ((obj = iterator.next())) {
      if (obj->IsJSFunction()) {
        JSFunction* function = JSFunction::cast(obj);
        if (!function->Inlines(*shared)) continue;
        if (function->code()->kind() == Code::OPTIMIZED_FUNCTION) {
          Deoptimizer::DeoptimizeFunction(function);
        }
        if (baseline_exists && function->shared() == *shared) {
          functions.Add(handle(function));
        }
      }
    }
  }

  // We do not need to replace code to debug bytecode.
  DCHECK(baseline_exists || functions.is_empty());

  // We do not need to recompile to debug bytecode.
  if (baseline_exists && !shared->code()->has_debug_break_slots()) {
    if (!Compiler::CompileDebugCode(shared)) return false;
  }

  for (Handle<JSFunction> const function : functions) {
    function->ReplaceCode(shared->code());
    JSFunction::EnsureLiterals(function);
  }

  // Update PCs on the stack to point to recompiled code.
  RedirectActiveFunctions redirect_visitor(*shared);
  redirect_visitor.VisitThread(isolate_, isolate_->thread_local_top());
  isolate_->thread_manager()->IterateArchivedThreads(&redirect_visitor);

  return true;
}

namespace {
template <typename Iterator>
void GetBreakablePositions(Iterator* it, int start_position, int end_position,
                           BreakPositionAlignment alignment,
                           std::set<int>* positions) {
  it->SkipToPosition(start_position, alignment);
  while (!it->Done() && it->position() < end_position &&
         it->position() >= start_position) {
    positions->insert(alignment == STATEMENT_ALIGNED ? it->statement_position()
                                                     : it->position());
    it->Next();
  }
}

void FindBreakablePositions(Handle<DebugInfo> debug_info, int start_position,
                            int end_position, BreakPositionAlignment alignment,
                            std::set<int>* positions) {
  if (debug_info->HasDebugCode()) {
    CodeBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    GetBreakablePositions(&it, start_position, end_position, alignment,
                          positions);
  } else {
    DCHECK(debug_info->HasDebugBytecodeArray());
    BytecodeArrayBreakIterator it(debug_info, ALL_BREAK_LOCATIONS);
    GetBreakablePositions(&it, start_position, end_position, alignment,
                          positions);
  }
}
}  // namespace

bool Debug::GetPossibleBreakpoints(Handle<Script> script, int start_position,
                                   int end_position, std::set<int>* positions) {
  while (true) {
    HandleScope scope(isolate_);
    List<Handle<SharedFunctionInfo>> candidates;
    SharedFunctionInfo::ScriptIterator iterator(script);
    for (SharedFunctionInfo* info = iterator.Next(); info != nullptr;
         info = iterator.Next()) {
      if (info->end_position() < start_position ||
          info->start_position() >= end_position) {
        continue;
      }
      if (!info->IsSubjectToDebugging()) continue;
      if (!info->HasDebugCode() && !info->allows_lazy_compilation()) continue;
      candidates.Add(i::handle(info));
    }

    bool was_compiled = false;
    for (int i = 0; i < candidates.length(); ++i) {
      // Code that cannot be compiled lazily are internal and not debuggable.
      DCHECK(candidates[i]->allows_lazy_compilation());
      if (!candidates[i]->HasDebugCode()) {
        if (!Compiler::CompileDebugCode(candidates[i])) {
          return false;
        } else {
          was_compiled = true;
        }
      }
      if (!EnsureDebugInfo(candidates[i], Handle<JSFunction>::null()))
        return false;
    }
    if (was_compiled) continue;

    for (int i = 0; i < candidates.length(); ++i) {
      CHECK(candidates[i]->HasDebugInfo());
      Handle<DebugInfo> debug_info(candidates[i]->GetDebugInfo());
      FindBreakablePositions(debug_info, start_position, end_position,
                             STATEMENT_ALIGNED, positions);
    }
    return true;
  }
  UNREACHABLE();
  return false;
}

void Debug::RecordGenerator(Handle<JSGeneratorObject> generator_object) {
  if (last_step_action() <= StepOut) return;

  if (last_step_action() == StepNext) {
    // Only consider this generator a step-next target if not stepping in.
    JavaScriptFrameIterator stack_iterator(isolate_);
    JavaScriptFrame* frame = stack_iterator.frame();
    if (frame->UnpaddedFP() < thread_local_.target_fp_) return;
  }

  DCHECK(!has_suspended_generator());
  thread_local_.suspended_generator_ = *generator_object;
  ClearStepping();
}

class SharedFunctionInfoFinder {
 public:
  explicit SharedFunctionInfoFinder(int target_position)
      : current_candidate_(NULL),
        current_candidate_closure_(NULL),
        current_start_position_(kNoSourcePosition),
        target_position_(target_position) {}

  void NewCandidate(SharedFunctionInfo* shared, JSFunction* closure = NULL) {
    if (!shared->IsSubjectToDebugging()) return;
    int start_position = shared->function_token_position();
    if (start_position == kNoSourcePosition) {
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

    SharedFunctionInfo* shared;
    {
      SharedFunctionInfoFinder finder(position);
      SharedFunctionInfo::ScriptIterator iterator(script);
      for (SharedFunctionInfo* info = iterator.Next(); info != nullptr;
           info = iterator.Next()) {
        finder.NewCandidate(info);
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
    // If not, compile to reveal inner functions.
    HandleScope scope(isolate_);
    // Code that cannot be compiled lazily are internal and not debuggable.
    DCHECK(shared->allows_lazy_compilation());
    if (!Compiler::CompileDebugCode(handle(shared))) break;
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
  } else if (!Compiler::Compile(function, Compiler::CLEAR_EXCEPTION)) {
    return false;
  }

  // To prepare bytecode for debugging, we already need to have the debug
  // info (containing the debug copy) upfront, but since we do not recompile,
  // preparing for break points cannot fail.
  CreateDebugInfo(shared);
  CHECK(PrepareFunctionForBreakPoints(shared));
  return true;
}


void Debug::CreateDebugInfo(Handle<SharedFunctionInfo> shared) {
  // Create the debug info object.
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
      shared->set_debug_info(DebugInfo::uninitialized());
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
  if (!LiveEdit::SetAfterBreakTarget(this)) {
    // Continue just after the slot.
    after_break_target_ = frame->pc();
  }
}

bool Debug::IsBreakAtReturn(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // With no debug info there are no break points, so we can't be at a return.
  if (!frame->function()->shared()->HasDebugInfo()) return false;

  DCHECK(!frame->is_optimized());
  BreakLocation location = BreakLocation::FromFrame(frame);
  return location.IsReturn() || location.IsTailCall();
}

void Debug::FramesHaveBeenDropped(StackFrame::Id new_break_frame_id,
                                  LiveEditFrameDropMode mode) {
  if (mode != LIVE_EDIT_CURRENTLY_SET_MODE) {
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
  isolate_->heap()->CollectAllGarbage(Heap::kFinalizeIncrementalMarkingMask,
                                      GarbageCollectionReason::kDebugger);
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

MaybeHandle<Object> Debug::MakeAsyncTaskEvent(Handle<Smi> type,
                                              Handle<Smi> id) {
  DCHECK(id->IsNumber());
  // Create the async task event object.
  Handle<Object> argv[] = {type, id};
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

void Debug::OnPromiseReject(Handle<Object> promise, Handle<Object> value) {
  if (in_debug_scope() || ignore_events()) return;
  HandleScope scope(isolate_);
  // Check whether the promise has been marked as having triggered a message.
  Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
  if (!promise->IsJSObject() ||
      JSReceiver::GetDataProperty(Handle<JSObject>::cast(promise), key)
          ->IsUndefined(isolate_)) {
    OnException(value, promise);
  }
}


void Debug::OnException(Handle<Object> exception, Handle<Object> promise) {
  // We cannot generate debug events when JS execution is disallowed.
  // TODO(5530): Reenable debug events within DisallowJSScopes once relevant
  // code (MakeExceptionEvent and ProcessDebugEvent) have been moved to C++.
  if (!AllowJavascriptExecution::IsAllowed(isolate_)) return;

  Isolate::CatchType catch_type = isolate_->PredictExceptionCatcher();

  // Don't notify listener of exceptions that are internal to a desugaring.
  if (catch_type == Isolate::CAUGHT_BY_DESUGARING) return;

  bool uncaught = catch_type == Isolate::NOT_CAUGHT;
  if (promise->IsJSObject()) {
    Handle<JSObject> jspromise = Handle<JSObject>::cast(promise);
    // Mark the promise as already having triggered a message.
    Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
    JSObject::SetProperty(jspromise, key, key, STRICT).Assert();
    // Check whether the promise reject is considered an uncaught exception.
    uncaught = !isolate_->PromiseHasUserDefinedRejectHandler(jspromise);
  }
  // Bail out if exception breaks are not active
  if (uncaught) {
    // Uncaught exceptions are reported by either flags.
    if (!(break_on_uncaught_exception_ || break_on_exception_)) return;
  } else {
    // Caught exceptions are reported is activated.
    if (!break_on_exception_) return;
  }

  {
    // Check whether the break location is muted.
    JavaScriptFrameIterator it(isolate_);
    if (!it.done() && IsMutedAtCurrentLocation(it.frame())) return;
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

void Debug::OnDebugBreak(Handle<Object> break_points_hit, bool auto_continue) {
  // The caller provided for DebugScope.
  AssertDebugContext();
  // Bail out if there is no listener for this event
  if (ignore_events()) return;

#ifdef DEBUG
  PrintBreakLocation();
#endif  // DEBUG

  HandleScope scope(isolate_);
  // Create the event data object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeBreakEvent(break_points_hit).ToHandle(&event_data)) return;

  // Process debug event.
  ProcessDebugEvent(v8::Break, Handle<JSObject>::cast(event_data),
                    auto_continue);
}


void Debug::OnCompileError(Handle<Script> script) {
  ProcessCompileEvent(v8::CompileError, script);
}


// Handle debugger actions when a new script is compiled.
void Debug::OnAfterCompile(Handle<Script> script) {
  ProcessCompileEvent(v8::AfterCompile, script);
}

namespace {
struct CollectedCallbackData {
  Object** location;
  int id;
  Debug* debug;
  Isolate* isolate;

  CollectedCallbackData(Object** location, int id, Debug* debug,
                        Isolate* isolate)
      : location(location), id(id), debug(debug), isolate(isolate) {}
};

void SendAsyncTaskEventCancel(const v8::WeakCallbackInfo<void>& info) {
  std::unique_ptr<CollectedCallbackData> data(
      reinterpret_cast<CollectedCallbackData*>(info.GetParameter()));
  if (!data->debug->is_active()) return;
  HandleScope scope(data->isolate);
  data->debug->OnAsyncTaskEvent(debug::kDebugPromiseCollected, data->id);
}

void ResetPromiseHandle(const v8::WeakCallbackInfo<void>& info) {
  CollectedCallbackData* data =
      reinterpret_cast<CollectedCallbackData*>(info.GetParameter());
  GlobalHandles::Destroy(data->location);
  info.SetSecondPassCallback(&SendAsyncTaskEventCancel);
}
}  //  namespace

int Debug::NextAsyncTaskId(Handle<JSObject> promise) {
  LookupIterator it(promise, isolate_->factory()->promise_async_id_symbol());
  Maybe<bool> maybe = JSReceiver::HasProperty(&it);
  if (maybe.ToChecked()) {
    MaybeHandle<Object> result = Object::GetProperty(&it);
    return Handle<Smi>::cast(result.ToHandleChecked())->value();
  }
  Handle<Smi> async_id =
      handle(Smi::FromInt(++thread_local_.async_task_count_), isolate_);
  Object::SetProperty(&it, async_id, SLOPPY, Object::MAY_BE_STORE_FROM_KEYED)
      .ToChecked();
  Handle<Object> global_handle = isolate_->global_handles()->Create(*promise);
  // We send EnqueueRecurring async task event when promise is fulfilled or
  // rejected, WillHandle and DidHandle for every scheduled microtask for this
  // promise.
  // We need to send a cancel event when no other microtasks can be
  // started for this promise and all current microtasks are finished.
  // Since we holding promise when at least one microtask is scheduled (inside
  // PromiseReactionJobInfo), we can send cancel event in weak callback.
  GlobalHandles::MakeWeak(
      global_handle.location(),
      new CollectedCallbackData(global_handle.location(), async_id->value(),
                                this, isolate_),
      &ResetPromiseHandle, v8::WeakCallbackType::kParameter);
  return async_id->value();
}

void Debug::SetAsyncTaskListener(debug::AsyncTaskListener listener,
                                 void* data) {
  async_task_listener_ = listener;
  async_task_listener_data_ = data;
  UpdateState();
}

void Debug::OnAsyncTaskEvent(debug::PromiseDebugActionType type, int id) {
  if (in_debug_scope() || ignore_events()) return;

  if (async_task_listener_) {
    async_task_listener_(type, id, async_task_listener_data_);
    // There are three types of event listeners: C++ message_handler,
    // JavaScript event listener and C++ event listener.
    // Currently inspector still uses C++ event listener and installs
    // more specific event listeners for part of events. Calling of
    // C++ event listener is redundant when more specific event listener
    // is presented. Other clients can install JavaScript event listener
    // (e.g. some of NodeJS module).
    bool non_inspector_listener_exists =
        message_handler_ != nullptr ||
        (event_listener_.is_null() && !event_listener_->IsForeign());
    if (!non_inspector_listener_exists) return;
  }

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

  // Create the script collected state object.
  Handle<Object> event_data;
  // Bail out and don't call debugger if exception.
  if (!MakeAsyncTaskEvent(handle(Smi::FromInt(type), isolate_),
                          handle(Smi::FromInt(id), isolate_))
           .ToHandle(&event_data))
    return;

  // Process debug event.
  ProcessDebugEvent(v8::AsyncTaskEvent, Handle<JSObject>::cast(event_data),
                    true);
}

void Debug::ProcessDebugEvent(v8::DebugEvent event, Handle<JSObject> event_data,
                              bool auto_continue) {
  HandleScope scope(isolate_);

  // Create the execution state.
  Handle<Object> exec_state;
  // Bail out and don't call debugger if exception.
  if (!MakeExecutionState().ToHandle(&exec_state)) return;

  // First notify the message handler if any.
  if (message_handler_ != NULL) {
    NotifyMessageHandler(event, Handle<JSObject>::cast(exec_state), event_data,
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
    debug::EventCallback callback = FUNCTION_CAST<debug::EventCallback>(
        Handle<Foreign>::cast(event_listener_)->foreign_address());
    EventDetailsImpl event_details(event,
                                   Handle<JSObject>::cast(exec_state),
                                   Handle<JSObject>::cast(event_data),
                                   event_listener_data_,
                                   client_data);
    callback(event_details);
    CHECK(!isolate_->has_scheduled_exception());
  } else {
    // Invoke the JavaScript debug event listener.
    DCHECK(event_listener_->IsJSFunction());
    Handle<Object> argv[] = { Handle<Object>(Smi::FromInt(event), isolate_),
                              exec_state,
                              event_data,
                              event_listener_data_ };
    Handle<JSReceiver> global = isolate_->global_proxy();
    MaybeHandle<Object> result =
        Execution::Call(isolate_, Handle<JSFunction>::cast(event_listener_),
                        global, arraysize(argv), argv);
    CHECK(!result.is_null());  // Listeners must not throw.
  }
  in_debug_event_listener_ = previous;
}


void Debug::ProcessCompileEvent(v8::DebugEvent event, Handle<Script> script) {
  if (ignore_events()) return;
  if (script->type() != i::Script::TYPE_NORMAL &&
      script->type() != i::Script::TYPE_WASM) {
    return;
  }
  SuppressDebug while_processing(this);

  bool in_nested_debug_scope = in_debug_scope();
  HandleScope scope(isolate_);
  DebugScope debug_scope(this);
  if (debug_scope.failed()) return;

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
    case v8::CompileError:
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
        event, auto_continue, Handle<JSObject>::cast(exec_state),
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

  Handle<Object> cmd_processor_ctor =
      JSReceiver::GetProperty(isolate_, exec_state, "debugCommandProcessor")
          .ToHandleChecked();
  Handle<Object> ctor_args[] = {isolate_->factory()->ToBoolean(running)};
  Handle<JSReceiver> cmd_processor = Handle<JSReceiver>::cast(
      Execution::Call(isolate_, cmd_processor_ctor, exec_state, 1, ctor_args)
          .ToHandleChecked());
  Handle<JSFunction> process_debug_request = Handle<JSFunction>::cast(
      JSReceiver::GetProperty(isolate_, cmd_processor, "processDebugRequest")
          .ToHandleChecked());
  Handle<Object> is_running =
      JSReceiver::GetProperty(isolate_, cmd_processor, "isRunning")
          .ToHandleChecked();

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
    Handle<String> request_text = isolate_->factory()
                                      ->NewStringFromTwoByte(command_text)
                                      .ToHandleChecked();
    Handle<Object> request_args[] = {request_text};
    Handle<Object> answer_value;
    Handle<String> answer;
    MaybeHandle<Object> maybe_exception;
    MaybeHandle<Object> maybe_result = Execution::TryCall(
        isolate_, process_debug_request, cmd_processor, 1, request_args,
        Execution::MessageHandling::kReport, &maybe_exception);

    if (maybe_result.ToHandle(&answer_value)) {
      if (answer_value->IsUndefined(isolate_)) {
        answer = isolate_->factory()->empty_string();
      } else {
        answer = Handle<String>::cast(answer_value);
      }

      // Log the JSON request/response.
      if (FLAG_trace_debug_json) {
        PrintF("%s\n", request_text->ToCString().get());
        PrintF("%s\n", answer->ToCString().get());
      }

      Handle<Object> is_running_args[] = {answer};
      maybe_result = Execution::Call(isolate_, is_running, cmd_processor, 1,
                                     is_running_args);
      Handle<Object> result;
      if (!maybe_result.ToHandle(&result)) break;
      running = result->IsTrue(isolate_);
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
  if (!callback->IsNullOrUndefined(isolate_)) {
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
  bool is_active = message_handler_ != nullptr || !event_listener_.is_null() ||
                   async_task_listener_ != nullptr;
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

void Debug::UpdateHookOnFunctionCall() {
  STATIC_ASSERT(StepFrame > StepIn);
  STATIC_ASSERT(LastStepAction == StepFrame);
  hook_on_function_call_ = thread_local_.last_step_action_ >= StepIn ||
                           isolate_->needs_side_effect_check();
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
      // Don't stop if the break location is muted.
      if (IsMutedAtCurrentLocation(it.frame())) return;
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

#ifdef DEBUG
void Debug::PrintBreakLocation() {
  if (!FLAG_print_break_location) return;
  HandleScope scope(isolate_);
  JavaScriptFrameIterator iterator(isolate_);
  if (iterator.done()) return;
  JavaScriptFrame* frame = iterator.frame();
  FrameSummary summary = FrameSummary::GetFirst(frame);
  int source_position = summary.SourcePosition();
  Handle<Object> script_obj = summary.script();
  PrintF("[debug] break in function '");
  summary.FunctionName()->PrintOn(stdout);
  PrintF("'.\n");
  if (script_obj->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_obj);
    Handle<String> source(String::cast(script->source()));
    Script::InitLineEnds(script);
    int line =
        Script::GetLineNumber(script, source_position) - script->line_offset();
    int column = Script::GetColumnNumber(script, source_position) -
                 (line == 0 ? script->column_offset() : 0);
    Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
    int line_start =
        line == 0 ? 0 : Smi::cast(line_ends->get(line - 1))->value() + 1;
    int line_end = Smi::cast(line_ends->get(line))->value();
    DisallowHeapAllocation no_gc;
    String::FlatContent content = source->GetFlatContent();
    if (content.IsOneByte()) {
      PrintF("[debug] %.*s\n", line_end - line_start,
             content.ToOneByteVector().start() + line_start);
      PrintF("[debug] ");
      for (int i = 0; i < column; i++) PrintF(" ");
      PrintF("^\n");
    } else {
      PrintF("[debug] at line %d column %d\n", line, column);
    }
  }
}
#endif  // DEBUG

DebugScope::DebugScope(Debug* debug)
    : debug_(debug),
      prev_(debug->debugger_entry()),
      save_(debug_->isolate_),
      no_termination_exceptons_(debug_->isolate_,
                                StackGuard::TERMINATE_EXECUTION) {
  // Link recursive debugger entry.
  base::NoBarrier_Store(&debug_->thread_local_.current_debug_scope_,
                        reinterpret_cast<base::AtomicWord>(this));

  // Store the previous break id, frame id and return value.
  break_id_ = debug_->break_id();
  break_frame_id_ = debug_->break_frame_id();
  return_value_ = debug_->return_value();

  // Create the new break info. If there is no proper frames there is no break
  // frame id.
  StackTraceFrameIterator it(isolate());
  bool has_frames = !it.done();
  // We don't currently support breaking inside wasm framess.
  DCHECK(!has_frames || !it.is_wasm());
  debug_->thread_local_.break_frame_id_ =
      has_frames ? it.frame()->id() : StackFrame::NO_ID;
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
  debug_->thread_local_.return_value_ = return_value_;

  debug_->UpdateState();
}

bool Debug::PerformSideEffectCheck(Handle<JSFunction> function) {
  DCHECK(isolate_->needs_side_effect_check());
  DisallowJavascriptExecution no_js(isolate_);
  if (!Compiler::Compile(function, Compiler::KEEP_EXCEPTION)) return false;
  Deoptimizer::DeoptimizeFunction(*function);
  if (!function->shared()->HasNoSideEffect()) {
    if (FLAG_trace_side_effect_free_debug_evaluate) {
      PrintF("[debug-evaluate] Function %s failed side effect check.\n",
             function->shared()->DebugName()->ToCString().get());
    }
    side_effect_check_failed_ = true;
    // Throw an uncatchable termination exception.
    isolate_->TerminateExecution();
    return false;
  }
  return true;
}

bool Debug::PerformSideEffectCheckForCallback(Address function) {
  DCHECK(isolate_->needs_side_effect_check());
  if (DebugEvaluate::CallbackHasNoSideEffect(function)) return true;
  side_effect_check_failed_ = true;
  // Throw an uncatchable termination exception.
  isolate_->TerminateExecution();
  isolate_->OptionalRescheduleException(false);
  return false;
}

NoSideEffectScope::~NoSideEffectScope() {
  if (isolate_->needs_side_effect_check() &&
      isolate_->debug()->side_effect_check_failed_) {
    DCHECK(isolate_->has_pending_exception());
    DCHECK_EQ(isolate_->heap()->termination_exception(),
              isolate_->pending_exception());
    // Convert the termination exception into a regular exception.
    isolate_->CancelTerminateExecution();
    isolate_->Throw(*isolate_->factory()->NewEvalError(
        MessageTemplate::kNoSideEffectDebugEvaluate));
  }
  isolate_->set_needs_side_effect_check(old_needs_side_effect_check_);
  isolate_->debug()->UpdateHookOnFunctionCall();
  isolate_->debug()->side_effect_check_failed_ = false;
}

MessageImpl MessageImpl::NewEvent(DebugEvent event, bool running,
                                  Handle<JSObject> exec_state,
                                  Handle<JSObject> event_data) {
  MessageImpl message(true, event, running, exec_state, event_data,
                      Handle<String>(), NULL);
  return message;
}

MessageImpl MessageImpl::NewResponse(DebugEvent event, bool running,
                                     Handle<JSObject> exec_state,
                                     Handle<JSObject> event_data,
                                     Handle<String> response_json,
                                     v8::Debug::ClientData* client_data) {
  MessageImpl message(false, event, running, exec_state, event_data,
                      response_json, client_data);
  return message;
}

MessageImpl::MessageImpl(bool is_event, DebugEvent event, bool running,
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

bool MessageImpl::IsEvent() const { return is_event_; }

bool MessageImpl::IsResponse() const { return !is_event_; }

DebugEvent MessageImpl::GetEvent() const { return event_; }

bool MessageImpl::WillStartRunning() const { return running_; }

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
    Handle<Object> fun =
        JSReceiver::GetProperty(isolate, event_data_, "toJSONProtocol")
            .ToHandleChecked();
    if (!fun->IsJSFunction()) {
      return v8::Local<v8::String>();
    }

    MaybeHandle<Object> maybe_exception;
    MaybeHandle<Object> maybe_json = Execution::TryCall(
        isolate, fun, event_data_, 0, nullptr,
        Execution::MessageHandling::kReport, &maybe_exception);
    Handle<Object> json;
    if (!maybe_json.ToHandle(&json) || !json->IsString()) {
      return v8::Local<v8::String>();
    }
    return scope.Escape(v8::Utils::ToLocal(Handle<String>::cast(json)));
  } else {
    return v8::Utils::ToLocal(response_json_);
  }
}

namespace {
v8::Local<v8::Context> GetDebugEventContext(Isolate* isolate) {
  Handle<Context> context = isolate->debug()->debugger_entry()->GetContext();
  // Isolate::context() may have been NULL when "script collected" event
  // occured.
  if (context.is_null()) return v8::Local<v8::Context>();
  Handle<Context> native_context(context->native_context());
  return v8::Utils::ToLocal(native_context);
}
}  // anonymous namespace

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

v8::Isolate* EventDetailsImpl::GetIsolate() const {
  return reinterpret_cast<v8::Isolate*>(exec_state_->GetIsolate());
}

CommandMessage::CommandMessage()
    : text_(Vector<uint16_t>::empty()), client_data_(NULL) {}

CommandMessage::CommandMessage(const Vector<uint16_t>& text,
                               v8::Debug::ClientData* data)
    : text_(text), client_data_(data) {}

void CommandMessage::Dispose() {
  text_.Dispose();
  delete client_data_;
  client_data_ = NULL;
}

CommandMessage CommandMessage::New(const Vector<uint16_t>& command,
                                   v8::Debug::ClientData* data) {
  return CommandMessage(command.Clone(), data);
}

CommandMessageQueue::CommandMessageQueue(int size)
    : start_(0), end_(0), size_(size) {
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
