// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug.h"

#include <memory>
#include <unordered_set>

#include "src/api-inl.h"
#include "src/arguments.h"
#include "src/assembler-inl.h"
#include "src/base/platform/mutex.h"
#include "src/bootstrapper.h"
#include "src/builtins/builtins.h"
#include "src/compilation-cache.h"
#include "src/compiler.h"
#include "src/counters.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/global-handles.h"
#include "src/globals.h"
#include "src/heap/heap-inl.h"  // For NextDebuggingId.
#include "src/interpreter/bytecode-array-accessor.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/message-template.h"
#include "src/objects/api-callbacks-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/snapshot.h"
#include "src/v8threads.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

class Debug::TemporaryObjectsTracker : public HeapObjectAllocationTracker {
 public:
  TemporaryObjectsTracker() = default;
  ~TemporaryObjectsTracker() override = default;

  void AllocationEvent(Address addr, int) override { objects_.insert(addr); }

  void MoveEvent(Address from, Address to, int) override {
    if (from == to) return;
    base::MutexGuard guard(&mutex_);
    auto it = objects_.find(from);
    if (it == objects_.end()) {
      // If temporary object was collected we can get MoveEvent which moves
      // existing non temporary object to the address where we had temporary
      // object. So we should mark new address as non temporary.
      objects_.erase(to);
      return;
    }
    objects_.erase(it);
    objects_.insert(to);
  }

  bool HasObject(Handle<HeapObject> obj) const {
    if (obj->IsJSObject() &&
        Handle<JSObject>::cast(obj)->GetEmbedderFieldCount()) {
      // Embedder may store any pointers using embedder fields and implements
      // non trivial logic, e.g. create wrappers lazily and store pointer to
      // native object inside embedder field. We should consider all objects
      // with embedder fields as non temporary.
      return false;
    }
    return objects_.find(obj->address()) != objects_.end();
  }

 private:
  std::unordered_set<Address> objects_;
  base::Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(TemporaryObjectsTracker);
};

Debug::Debug(Isolate* isolate)
    : is_active_(false),
      hook_on_function_call_(false),
      is_suppressed_(false),
      break_disabled_(false),
      break_points_active_(true),
      break_on_exception_(false),
      break_on_uncaught_exception_(false),
      side_effect_check_failed_(false),
      debug_info_list_(nullptr),
      feature_tracker_(isolate),
      isolate_(isolate) {
  ThreadInit();
}

Debug::~Debug() { DCHECK_NULL(debug_delegate_); }

BreakLocation BreakLocation::FromFrame(Handle<DebugInfo> debug_info,
                                       JavaScriptFrame* frame) {
  if (debug_info->CanBreakAtEntry()) {
    return BreakLocation(Debug::kBreakAtEntryPosition, DEBUG_BREAK_AT_ENTRY);
  }
  auto summary = FrameSummary::GetTop(frame).AsJavaScript();
  int offset = summary.code_offset();
  Handle<AbstractCode> abstract_code = summary.abstract_code();
  BreakIterator it(debug_info);
  it.SkipTo(BreakIndexFromCodeOffset(debug_info, abstract_code, offset));
  return it.GetBreakLocation();
}

void BreakLocation::AllAtCurrentStatement(
    Handle<DebugInfo> debug_info, JavaScriptFrame* frame,
    std::vector<BreakLocation>* result_out) {
  DCHECK(!debug_info->CanBreakAtEntry());
  auto summary = FrameSummary::GetTop(frame).AsJavaScript();
  int offset = summary.code_offset();
  Handle<AbstractCode> abstract_code = summary.abstract_code();
  if (abstract_code->IsCode()) offset = offset - 1;
  int statement_position;
  {
    BreakIterator it(debug_info);
    it.SkipTo(BreakIndexFromCodeOffset(debug_info, abstract_code, offset));
    statement_position = it.statement_position();
  }
  for (BreakIterator it(debug_info); !it.Done(); it.Next()) {
    if (it.statement_position() == statement_position) {
      result_out->push_back(it.GetBreakLocation());
    }
  }
}

JSGeneratorObject BreakLocation::GetGeneratorObjectForSuspendedFrame(
    JavaScriptFrame* frame) const {
  DCHECK(IsSuspend());
  DCHECK_GE(generator_obj_reg_index_, 0);

  Object generator_obj = InterpretedFrame::cast(frame)->ReadInterpreterRegister(
      generator_obj_reg_index_);

  return JSGeneratorObject::cast(generator_obj);
}

int BreakLocation::BreakIndexFromCodeOffset(Handle<DebugInfo> debug_info,
                                            Handle<AbstractCode> abstract_code,
                                            int offset) {
  // Run through all break points to locate the one closest to the address.
  int closest_break = 0;
  int distance = kMaxInt;
  DCHECK(0 <= offset && offset < abstract_code->Size());
  for (BreakIterator it(debug_info); !it.Done(); it.Next()) {
    // Check if this break point is closer that what was previously found.
    if (it.code_offset() <= offset && offset - it.code_offset() < distance) {
      closest_break = it.break_index();
      distance = offset - it.code_offset();
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
  }
  return closest_break;
}

bool BreakLocation::HasBreakPoint(Isolate* isolate,
                                  Handle<DebugInfo> debug_info) const {
  // First check whether there is a break point with the same source position.
  if (!debug_info->HasBreakPoint(isolate, position_)) return false;
  if (debug_info->CanBreakAtEntry()) {
    DCHECK_EQ(Debug::kBreakAtEntryPosition, position_);
    return debug_info->BreakAtEntry();
  } else {
    // Then check whether a break point at that source position would have
    // the same code offset. Otherwise it's just a break location that we can
    // step to, but not actually a location where we can put a break point.
    DCHECK(abstract_code_->IsBytecodeArray());
    BreakIterator it(debug_info);
    it.SkipToPosition(position_);
    return it.code_offset() == code_offset_;
  }
}

debug::BreakLocationType BreakLocation::type() const {
  switch (type_) {
    case DEBUGGER_STATEMENT:
      return debug::kDebuggerStatementBreakLocation;
    case DEBUG_BREAK_SLOT_AT_CALL:
      return debug::kCallBreakLocation;
    case DEBUG_BREAK_SLOT_AT_RETURN:
      return debug::kReturnBreakLocation;

    // Externally, suspend breaks should look like normal breaks.
    case DEBUG_BREAK_SLOT_AT_SUSPEND:
    default:
      return debug::kCommonBreakLocation;
  }
}

BreakIterator::BreakIterator(Handle<DebugInfo> debug_info)
    : debug_info_(debug_info),
      break_index_(-1),
      source_position_iterator_(
          debug_info->DebugBytecodeArray()->SourcePositionTable()) {
  position_ = debug_info->shared()->StartPosition();
  statement_position_ = position_;
  // There is at least one break location.
  DCHECK(!Done());
  Next();
}

int BreakIterator::BreakIndexFromPosition(int source_position) {
  int distance = kMaxInt;
  int closest_break = break_index();
  while (!Done()) {
    int next_position = position();
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

void BreakIterator::Next() {
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
    DCHECK_LE(0, position_);
    DCHECK_LE(0, statement_position_);

    DebugBreakType type = GetDebugBreakType();
    if (type != NOT_DEBUG_BREAK) break;
  }
  break_index_++;
}

DebugBreakType BreakIterator::GetDebugBreakType() {
  BytecodeArray bytecode_array = debug_info_->OriginalBytecodeArray();
  interpreter::Bytecode bytecode =
      interpreter::Bytecodes::FromByte(bytecode_array->get(code_offset()));

  // Make sure we read the actual bytecode, not a prefix scaling bytecode.
  if (interpreter::Bytecodes::IsPrefixScalingBytecode(bytecode)) {
    bytecode = interpreter::Bytecodes::FromByte(
        bytecode_array->get(code_offset() + 1));
  }

  if (bytecode == interpreter::Bytecode::kDebugger) {
    return DEBUGGER_STATEMENT;
  } else if (bytecode == interpreter::Bytecode::kReturn) {
    return DEBUG_BREAK_SLOT_AT_RETURN;
  } else if (bytecode == interpreter::Bytecode::kSuspendGenerator) {
    return DEBUG_BREAK_SLOT_AT_SUSPEND;
  } else if (interpreter::Bytecodes::IsCallOrConstruct(bytecode)) {
    return DEBUG_BREAK_SLOT_AT_CALL;
  } else if (source_position_iterator_.is_statement()) {
    return DEBUG_BREAK_SLOT;
  } else {
    return NOT_DEBUG_BREAK;
  }
}

void BreakIterator::SkipToPosition(int position) {
  BreakIterator it(debug_info_);
  SkipTo(it.BreakIndexFromPosition(position));
}

void BreakIterator::SetDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  HandleScope scope(isolate());
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  Handle<BytecodeArray> bytecode_array(debug_info_->DebugBytecodeArray(),
                                       isolate());
  interpreter::BytecodeArrayAccessor(bytecode_array, code_offset())
      .ApplyDebugBreak();
}

void BreakIterator::ClearDebugBreak() {
  DebugBreakType debug_break_type = GetDebugBreakType();
  if (debug_break_type == DEBUGGER_STATEMENT) return;
  DCHECK(debug_break_type >= DEBUG_BREAK_SLOT);
  BytecodeArray bytecode_array = debug_info_->DebugBytecodeArray();
  BytecodeArray original = debug_info_->OriginalBytecodeArray();
  bytecode_array->set(code_offset(), original->get(code_offset()));
}

BreakLocation BreakIterator::GetBreakLocation() {
  Handle<AbstractCode> code(
      AbstractCode::cast(debug_info_->DebugBytecodeArray()), isolate());
  DebugBreakType type = GetDebugBreakType();
  int generator_object_reg_index = -1;
  if (type == DEBUG_BREAK_SLOT_AT_SUSPEND) {
    // For suspend break, we'll need the generator object to be able to step
    // over the suspend as if it didn't return. We get the interpreter register
    // index that holds the generator object by reading it directly off the
    // bytecode array, and we'll read the actual generator object off the
    // interpreter stack frame in GetGeneratorObjectForSuspendedFrame.
    BytecodeArray bytecode_array = debug_info_->OriginalBytecodeArray();
    interpreter::BytecodeArrayAccessor accessor(
        handle(bytecode_array, isolate()), code_offset());

    DCHECK_EQ(accessor.current_bytecode(),
              interpreter::Bytecode::kSuspendGenerator);
    interpreter::Register generator_obj_reg = accessor.GetRegisterOperand(0);
    generator_object_reg_index = generator_obj_reg.index();
  }
  return BreakLocation(code, type, code_offset(), position_,
                       generator_object_reg_index);
}

Isolate* BreakIterator::isolate() { return debug_info_->GetIsolate(); }

void DebugFeatureTracker::Track(DebugFeatureTracker::Feature feature) {
  uint32_t mask = 1 << feature;
  // Only count one sample per feature and isolate.
  if (bitfield_ & mask) return;
  isolate_->counters()->debug_feature_usage()->AddSample(feature);
  bitfield_ |= mask;
}


// Threading support.
void Debug::ThreadInit() {
  thread_local_.break_frame_id_ = StackFrame::NO_ID;
  thread_local_.last_step_action_ = StepNone;
  thread_local_.last_statement_position_ = kNoSourcePosition;
  thread_local_.last_frame_count_ = -1;
  thread_local_.fast_forward_to_return_ = false;
  thread_local_.ignore_step_into_function_ = Smi::kZero;
  thread_local_.target_frame_count_ = -1;
  thread_local_.return_value_ = Smi::kZero;
  thread_local_.last_breakpoint_id_ = 0;
  clear_suspended_generator();
  thread_local_.restart_fp_ = kNullAddress;
  base::Relaxed_Store(&thread_local_.current_debug_scope_,
                      static_cast<base::AtomicWord>(0));
  thread_local_.break_on_next_function_call_ = false;
  UpdateHookOnFunctionCall();
}


char* Debug::ArchiveDebug(char* storage) {
  MemCopy(storage, reinterpret_cast<char*>(&thread_local_),
          ArchiveSpacePerThread());
  return storage + ArchiveSpacePerThread();
}

char* Debug::RestoreDebug(char* storage) {
  MemCopy(reinterpret_cast<char*>(&thread_local_), storage,
          ArchiveSpacePerThread());

  // Enter the debugger.
  DebugScope debug_scope(this);

  // Clear any one-shot breakpoints that may have been set by the other
  // thread, and reapply breakpoints for this thread.
  ClearOneShot();

  if (thread_local_.last_step_action_ != StepNone) {
    // Reset the previous step action for this thread.
    PrepareStep(thread_local_.last_step_action_);
  }

  return storage + ArchiveSpacePerThread();
}

int Debug::ArchiveSpacePerThread() { return sizeof(ThreadLocal); }

void Debug::Iterate(RootVisitor* v) {
  v->VisitRootPointer(Root::kDebug, nullptr,
                      FullObjectSlot(&thread_local_.return_value_));
  v->VisitRootPointer(Root::kDebug, nullptr,
                      FullObjectSlot(&thread_local_.suspended_generator_));
  v->VisitRootPointer(
      Root::kDebug, nullptr,
      FullObjectSlot(&thread_local_.ignore_step_into_function_));
}

DebugInfoListNode::DebugInfoListNode(Isolate* isolate, DebugInfo debug_info)
    : next_(nullptr) {
  // Globalize the request debug info object and make it weak.
  GlobalHandles* global_handles = isolate->global_handles();
  debug_info_ = global_handles->Create(debug_info).location();
}

DebugInfoListNode::~DebugInfoListNode() {
  if (debug_info_ == nullptr) return;
  GlobalHandles::Destroy(debug_info_);
  debug_info_ = nullptr;
}

void Debug::Unload() {
  ClearAllBreakPoints();
  ClearStepping();
  RemoveAllCoverageInfos();
  ClearAllDebuggerHints();
  debug_delegate_ = nullptr;
}

void Debug::Break(JavaScriptFrame* frame, Handle<JSFunction> break_target) {
  // Initialize LiveEdit.
  LiveEdit::InitializeThreadLocal(this);

  // Just continue if breaks are disabled or debugger cannot be loaded.
  if (break_disabled()) return;

  // Enter the debugger.
  DebugScope debug_scope(this);
  DisableBreak no_recursive_break(this);

  // Return if we fail to retrieve debug info.
  Handle<SharedFunctionInfo> shared(break_target->shared(), isolate_);
  if (!EnsureBreakInfo(shared)) return;
  PrepareFunctionForDebugExecution(shared);

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);

  // Find the break location where execution has stopped.
  BreakLocation location = BreakLocation::FromFrame(debug_info, frame);

  // Find actual break points, if any, and trigger debug break event.
  MaybeHandle<FixedArray> break_points_hit =
      CheckBreakPoints(debug_info, &location);
  if (!break_points_hit.is_null() || break_on_next_function_call()) {
    // Clear all current stepping setup.
    ClearStepping();
    // Notify the debug event listeners.
    OnDebugBreak(!break_points_hit.is_null()
                     ? break_points_hit.ToHandleChecked()
                     : isolate_->factory()->empty_fixed_array());
    return;
  }

  // Debug break at function entry, do not worry about stepping.
  if (location.IsDebugBreakAtEntry()) {
    DCHECK(debug_info->BreakAtEntry());
    return;
  }

  DCHECK_NOT_NULL(frame);

  // No break point. Check for stepping.
  StepAction step_action = last_step_action();
  int current_frame_count = CurrentFrameCount();
  int target_frame_count = thread_local_.target_frame_count_;
  int last_frame_count = thread_local_.last_frame_count_;

  // StepOut at not return position was requested and return break locations
  // were flooded with one shots.
  if (thread_local_.fast_forward_to_return_) {
    DCHECK(location.IsReturnOrSuspend());
    // We have to ignore recursive calls to function.
    if (current_frame_count > target_frame_count) return;
    ClearStepping();
    PrepareStep(StepOut);
    return;
  }

  bool step_break = false;
  switch (step_action) {
    case StepNone:
      return;
    case StepOut:
      // Step out should not break in a deeper frame than target frame.
      if (current_frame_count > target_frame_count) return;
      step_break = true;
      break;
    case StepNext:
      // Step next should not break in a deeper frame than target frame.
      if (current_frame_count > target_frame_count) return;
      V8_FALLTHROUGH;
    case StepIn: {
      // Special case "next" and "in" for generators that are about to suspend.
      if (location.IsSuspend()) {
        DCHECK(!has_suspended_generator());
        thread_local_.suspended_generator_ =
            location.GetGeneratorObjectForSuspendedFrame(frame);
        ClearStepping();
        return;
      }

      FrameSummary summary = FrameSummary::GetTop(frame);
      step_break = step_break || location.IsReturn() ||
                   current_frame_count != last_frame_count ||
                   thread_local_.last_statement_position_ !=
                       summary.SourceStatementPosition();
      break;
    }
  }

  // Clear all current stepping setup.
  ClearStepping();

  if (step_break) {
    // Notify the debug event listeners.
    OnDebugBreak(isolate_->factory()->empty_fixed_array());
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
      break_points_active_ && location->HasBreakPoint(isolate_, debug_info);
  if (has_break_points) *has_break_points = has_break_points_to_check;
  if (!has_break_points_to_check) return {};

  return Debug::GetHitBreakPoints(debug_info, location->position());
}


bool Debug::IsMutedAtCurrentLocation(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);
  // A break location is considered muted if break locations on the current
  // statement have at least one break point, and all of these break points
  // evaluate to false. Aside from not triggering a debug break event at the
  // break location, we also do not trigger one for debugger statements, nor
  // an exception event on exception at this location.
  FrameSummary summary = FrameSummary::GetTop(frame);
  DCHECK(!summary.IsWasm());
  Handle<JSFunction> function = summary.AsJavaScript().function();
  if (!function->shared()->HasBreakInfo()) return false;
  Handle<DebugInfo> debug_info(function->shared()->GetDebugInfo(), isolate_);
  // Enter the debugger.
  DebugScope debug_scope(this);
  std::vector<BreakLocation> break_locations;
  BreakLocation::AllAtCurrentStatement(debug_info, frame, &break_locations);
  bool has_break_points_at_all = false;
  for (size_t i = 0; i < break_locations.size(); i++) {
    bool has_break_points;
    MaybeHandle<FixedArray> check_result =
        CheckBreakPoints(debug_info, &break_locations[i], &has_break_points);
    has_break_points_at_all |= has_break_points;
    if (has_break_points && !check_result.is_null()) return false;
  }
  return has_break_points_at_all;
}

// Check whether a single break point object is triggered.
bool Debug::CheckBreakPoint(Handle<BreakPoint> break_point,
                            bool is_break_at_entry) {
  HandleScope scope(isolate_);

  if (!break_point->condition()->length()) return true;
  Handle<String> condition(break_point->condition(), isolate_);
  MaybeHandle<Object> maybe_result;
  Handle<Object> result;

  if (is_break_at_entry) {
    maybe_result = DebugEvaluate::WithTopmostArguments(isolate_, condition);
  } else {
    // Since we call CheckBreakpoint only for deoptimized frame on top of stack,
    // we can use 0 as index of inlined frame.
    const int inlined_jsframe_index = 0;
    const bool throw_on_side_effect = false;
    maybe_result =
        DebugEvaluate::Local(isolate_, break_frame_id(), inlined_jsframe_index,
                             condition, throw_on_side_effect);
  }

  if (!maybe_result.ToHandle(&result)) {
    if (isolate_->has_pending_exception()) {
      isolate_->clear_pending_exception();
    }
    return false;
  }
  return result->BooleanValue(isolate_);
}

bool Debug::SetBreakPoint(Handle<JSFunction> function,
                          Handle<BreakPoint> break_point,
                          int* source_position) {
  HandleScope scope(isolate_);

  // Make sure the function is compiled and has set up the debug info.
  Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
  if (!EnsureBreakInfo(shared)) return false;
  PrepareFunctionForDebugExecution(shared);

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);
  // Source positions starts with zero.
  DCHECK_LE(0, *source_position);

  // Find the break point and change it.
  *source_position = FindBreakablePosition(debug_info, *source_position);
  DebugInfo::SetBreakPoint(isolate_, debug_info, *source_position, break_point);
  // At least one active break point now.
  DCHECK_LT(0, debug_info->GetBreakPointCount(isolate_));

  ClearBreakPoints(debug_info);
  ApplyBreakPoints(debug_info);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);
  return true;
}

bool Debug::SetBreakPointForScript(Handle<Script> script,
                                   Handle<String> condition,
                                   int* source_position, int* id) {
  *id = ++thread_local_.last_breakpoint_id_;
  Handle<BreakPoint> break_point =
      isolate_->factory()->NewBreakPoint(*id, condition);
  if (script->type() == Script::TYPE_WASM) {
    Handle<WasmModuleObject> module_object(
        WasmModuleObject::cast(script->wasm_module_object()), isolate_);
    return WasmModuleObject::SetBreakPoint(module_object, source_position,
                                           break_point);
  }

  HandleScope scope(isolate_);

  // Obtain shared function info for the function.
  Handle<Object> result =
      FindSharedFunctionInfoInScript(script, *source_position);
  if (result->IsUndefined(isolate_)) return false;

  // Make sure the function has set up the debug info.
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>::cast(result);
  if (!EnsureBreakInfo(shared)) return false;
  PrepareFunctionForDebugExecution(shared);

  // Find position within function. The script position might be before the
  // source position of the first function.
  if (shared->StartPosition() > *source_position) {
    *source_position = shared->StartPosition();
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);

  // Find breakable position returns first breakable position after
  // *source_position, it can return 0 if no break location is found after
  // *source_position.
  int breakable_position = FindBreakablePosition(debug_info, *source_position);
  if (breakable_position < *source_position) return false;
  *source_position = breakable_position;

  DebugInfo::SetBreakPoint(isolate_, debug_info, *source_position, break_point);
  // At least one active break point now.
  DCHECK_LT(0, debug_info->GetBreakPointCount(isolate_));

  ClearBreakPoints(debug_info);
  ApplyBreakPoints(debug_info);

  feature_tracker()->Track(DebugFeatureTracker::kBreakPoint);
  return true;
}

int Debug::FindBreakablePosition(Handle<DebugInfo> debug_info,
                                 int source_position) {
  if (debug_info->CanBreakAtEntry()) {
    return kBreakAtEntryPosition;
  } else {
    DCHECK(debug_info->HasInstrumentedBytecodeArray());
    BreakIterator it(debug_info);
    it.SkipToPosition(source_position);
    return it.position();
  }
}

void Debug::ApplyBreakPoints(Handle<DebugInfo> debug_info) {
  DisallowHeapAllocation no_gc;
  if (debug_info->CanBreakAtEntry()) {
    debug_info->SetBreakAtEntry();
  } else {
    if (!debug_info->HasInstrumentedBytecodeArray()) return;
    FixedArray break_points = debug_info->break_points();
    for (int i = 0; i < break_points->length(); i++) {
      if (break_points->get(i)->IsUndefined(isolate_)) continue;
      BreakPointInfo info = BreakPointInfo::cast(break_points->get(i));
      if (info->GetBreakPointCount(isolate_) == 0) continue;
      DCHECK(debug_info->HasInstrumentedBytecodeArray());
      BreakIterator it(debug_info);
      it.SkipToPosition(info->source_position());
      it.SetDebugBreak();
    }
  }
  debug_info->SetDebugExecutionMode(DebugInfo::kBreakpoints);
}

void Debug::ClearBreakPoints(Handle<DebugInfo> debug_info) {
  if (debug_info->CanBreakAtEntry()) {
    debug_info->ClearBreakAtEntry();
  } else {
    // If we attempt to clear breakpoints but none exist, simply return. This
    // can happen e.g. CoverageInfos exist but no breakpoints are set.
    if (!debug_info->HasInstrumentedBytecodeArray() ||
        !debug_info->HasBreakInfo()) {
      return;
    }

    DisallowHeapAllocation no_gc;
    for (BreakIterator it(debug_info); !it.Done(); it.Next()) {
      it.ClearDebugBreak();
    }
  }
}

void Debug::ClearBreakPoint(Handle<BreakPoint> break_point) {
  HandleScope scope(isolate_);

  for (DebugInfoListNode* node = debug_info_list_; node != nullptr;
       node = node->next()) {
    if (!node->debug_info()->HasBreakInfo()) continue;
    Handle<Object> result = DebugInfo::FindBreakPointInfo(
        isolate_, node->debug_info(), break_point);
    if (result->IsUndefined(isolate_)) continue;
    Handle<DebugInfo> debug_info = node->debug_info();
    if (DebugInfo::ClearBreakPoint(isolate_, debug_info, break_point)) {
      ClearBreakPoints(debug_info);
      if (debug_info->GetBreakPointCount(isolate_) == 0) {
        RemoveBreakInfoAndMaybeFree(debug_info);
      } else {
        ApplyBreakPoints(debug_info);
      }
      return;
    }
  }
}

int Debug::GetFunctionDebuggingId(Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared = handle(function->shared(), isolate_);
  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);
  int id = debug_info->debugging_id();
  if (id == DebugInfo::kNoDebuggingId) {
    id = isolate_->heap()->NextDebuggingId();
    debug_info->set_debugging_id(id);
  }
  return id;
}

bool Debug::SetBreakpointForFunction(Handle<JSFunction> function,
                                     Handle<String> condition, int* id) {
  *id = ++thread_local_.last_breakpoint_id_;
  Handle<BreakPoint> breakpoint =
      isolate_->factory()->NewBreakPoint(*id, condition);
  int source_position = 0;
  return SetBreakPoint(function, breakpoint, &source_position);
}

void Debug::RemoveBreakpoint(int id) {
  Handle<BreakPoint> breakpoint = isolate_->factory()->NewBreakPoint(
      id, isolate_->factory()->empty_string());
  ClearBreakPoint(breakpoint);
}

// Clear out all the debug break code.
void Debug::ClearAllBreakPoints() {
  ClearAllDebugInfos([=](Handle<DebugInfo> info) {
    ClearBreakPoints(info);
    info->ClearBreakInfo(isolate_);
  });
}

void Debug::FloodWithOneShot(Handle<SharedFunctionInfo> shared,
                             bool returns_only) {
  if (IsBlackboxed(shared)) return;
  // Make sure the function is compiled and has set up the debug info.
  if (!EnsureBreakInfo(shared)) return;
  PrepareFunctionForDebugExecution(shared);

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);
  // Flood the function with break points.
  DCHECK(debug_info->HasInstrumentedBytecodeArray());
  for (BreakIterator it(debug_info); !it.Done(); it.Next()) {
    if (returns_only && !it.GetBreakLocation().IsReturnOrSuspend()) continue;
    it.SetDebugBreak();
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

MaybeHandle<FixedArray> Debug::GetHitBreakPoints(Handle<DebugInfo> debug_info,
                                                 int position) {
  Handle<Object> break_points = debug_info->GetBreakPoints(isolate_, position);
  bool is_break_at_entry = debug_info->BreakAtEntry();
  DCHECK(!break_points->IsUndefined(isolate_));
  if (!break_points->IsFixedArray()) {
    if (!CheckBreakPoint(Handle<BreakPoint>::cast(break_points),
                         is_break_at_entry)) {
      return {};
    }
    Handle<FixedArray> break_points_hit = isolate_->factory()->NewFixedArray(1);
    break_points_hit->set(0, *break_points);
    return break_points_hit;
  }

  Handle<FixedArray> array(FixedArray::cast(*break_points), isolate_);
  int num_objects = array->length();
  Handle<FixedArray> break_points_hit =
      isolate_->factory()->NewFixedArray(num_objects);
  int break_points_hit_count = 0;
  for (int i = 0; i < num_objects; ++i) {
    Handle<Object> break_point(array->get(i), isolate_);
    if (CheckBreakPoint(Handle<BreakPoint>::cast(break_point),
                        is_break_at_entry)) {
      break_points_hit->set(break_points_hit_count++, *break_point);
    }
  }
  if (break_points_hit_count == 0) return {};
  break_points_hit->Shrink(isolate_, break_points_hit_count);
  return break_points_hit;
}

void Debug::SetBreakOnNextFunctionCall() {
  // This method forces V8 to break on next function call regardless current
  // last_step_action_. If any break happens between SetBreakOnNextFunctionCall
  // and ClearBreakOnNextFunctionCall, we will clear this flag and stepping. If
  // break does not happen, e.g. all called functions are blackboxed or no
  // function is called, then we will clear this flag and let stepping continue
  // its normal business.
  thread_local_.break_on_next_function_call_ = true;
  UpdateHookOnFunctionCall();
}

void Debug::ClearBreakOnNextFunctionCall() {
  thread_local_.break_on_next_function_call_ = false;
  UpdateHookOnFunctionCall();
}

void Debug::PrepareStepIn(Handle<JSFunction> function) {
  CHECK(last_step_action() >= StepIn || break_on_next_function_call());
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;
  Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
  if (IsBlackboxed(shared)) return;
  if (*function == thread_local_.ignore_step_into_function_) return;
  thread_local_.ignore_step_into_function_ = Smi::kZero;
  FloodWithOneShot(Handle<SharedFunctionInfo>(function->shared(), isolate_));
}

void Debug::PrepareStepInSuspendedGenerator() {
  CHECK(has_suspended_generator());
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;
  thread_local_.last_step_action_ = StepIn;
  UpdateHookOnFunctionCall();
  Handle<JSFunction> function(
      JSGeneratorObject::cast(thread_local_.suspended_generator_)->function(),
      isolate_);
  FloodWithOneShot(Handle<SharedFunctionInfo>(function->shared(), isolate_));
  clear_suspended_generator();
}

void Debug::PrepareStepOnThrow() {
  if (last_step_action() == StepNone) return;
  if (ignore_events()) return;
  if (in_debug_scope()) return;
  if (break_disabled()) return;

  ClearOneShot();

  int current_frame_count = CurrentFrameCount();

  // Iterate through the JavaScript stack looking for handlers.
  JavaScriptFrameIterator it(isolate_);
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->LookupExceptionHandlerInTable(nullptr, nullptr) > 0) break;
    std::vector<SharedFunctionInfo> infos;
    frame->GetFunctions(&infos);
    current_frame_count -= infos.size();
    it.Advance();
  }

  // No handler found. Nothing to instrument.
  if (it.done()) return;

  bool found_handler = false;
  // Iterate frames, including inlined frames. First, find the handler frame.
  // Then skip to the frame we want to break in, then instrument for stepping.
  for (; !it.done(); it.Advance()) {
    JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
    if (last_step_action() == StepIn) {
      // Deoptimize frame to ensure calls are checked for step-in.
      Deoptimizer::DeoptimizeFunction(frame->function());
    }
    std::vector<FrameSummary> summaries;
    frame->Summarize(&summaries);
    for (size_t i = summaries.size(); i != 0; i--, current_frame_count--) {
      const FrameSummary& summary = summaries[i - 1];
      if (!found_handler) {
        // We have yet to find the handler. If the frame inlines multiple
        // functions, we have to check each one for the handler.
        // If it only contains one function, we already found the handler.
        if (summaries.size() > 1) {
          Handle<AbstractCode> code = summary.AsJavaScript().abstract_code();
          CHECK_EQ(AbstractCode::INTERPRETED_FUNCTION, code->kind());
          HandlerTable table(code->GetBytecodeArray());
          int code_offset = summary.code_offset();
          HandlerTable::CatchPrediction prediction;
          int index = table.LookupRange(code_offset, nullptr, &prediction);
          if (index > 0) found_handler = true;
        } else {
          found_handler = true;
        }
      }

      if (found_handler) {
        // We found the handler. If we are stepping next or out, we need to
        // iterate until we found the suitable target frame to break in.
        if ((last_step_action() == StepNext || last_step_action() == StepOut) &&
            current_frame_count > thread_local_.target_frame_count_) {
          continue;
        }
        Handle<SharedFunctionInfo> info(
            summary.AsJavaScript().function()->shared(), isolate_);
        if (IsBlackboxed(info)) continue;
        FloodWithOneShot(info);
        return;
      }
    }
  }
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

  feature_tracker()->Track(DebugFeatureTracker::kStepping);

  thread_local_.last_step_action_ = step_action;

  StackTraceFrameIterator frames_it(isolate_, frame_id);
  StandardFrame* frame = frames_it.frame();

  // Handle stepping in wasm functions via the wasm interpreter.
  if (frame->is_wasm()) {
    // If the top frame is compiled, we cannot step.
    if (frame->is_wasm_compiled()) return;
    WasmInterpreterEntryFrame* wasm_frame =
        WasmInterpreterEntryFrame::cast(frame);
    wasm_frame->debug_info()->PrepareStep(step_action);
    return;
  }

  JavaScriptFrame* js_frame = JavaScriptFrame::cast(frame);
  DCHECK(js_frame->function()->IsJSFunction());

  // Get the debug info (create it if it does not exist).
  auto summary = FrameSummary::GetTop(frame).AsJavaScript();
  Handle<JSFunction> function(summary.function());
  Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
  if (!EnsureBreakInfo(shared)) return;
  PrepareFunctionForDebugExecution(shared);

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);

  BreakLocation location = BreakLocation::FromFrame(debug_info, js_frame);

  // Any step at a return is a step-out, and a step-out at a suspend behaves
  // like a return.
  if (location.IsReturn() || (location.IsSuspend() && step_action == StepOut)) {
    // On StepOut we'll ignore our further calls to current function in
    // PrepareStepIn callback.
    if (last_step_action() == StepOut) {
      thread_local_.ignore_step_into_function_ = *function;
    }
    step_action = StepOut;
    thread_local_.last_step_action_ = StepIn;
  }

  // We need to schedule DebugOnFunction call callback
  UpdateHookOnFunctionCall();

  // A step-next in blackboxed function is a step-out.
  if (step_action == StepNext && IsBlackboxed(shared)) step_action = StepOut;

  thread_local_.last_statement_position_ =
      summary.abstract_code()->SourceStatementPosition(summary.code_offset());
  int current_frame_count = CurrentFrameCount();
  thread_local_.last_frame_count_ = current_frame_count;
  // No longer perform the current async step.
  clear_suspended_generator();

  switch (step_action) {
    case StepNone:
      UNREACHABLE();
      break;
    case StepOut: {
      // Clear last position info. For stepping out it does not matter.
      thread_local_.last_statement_position_ = kNoSourcePosition;
      thread_local_.last_frame_count_ = -1;
      if (!location.IsReturnOrSuspend() && !IsBlackboxed(shared)) {
        // At not return position we flood return positions with one shots and
        // will repeat StepOut automatically at next break.
        thread_local_.target_frame_count_ = current_frame_count;
        thread_local_.fast_forward_to_return_ = true;
        FloodWithOneShot(shared, true);
        return;
      }
      // Skip the current frame, find the first frame we want to step out to
      // and deoptimize every frame along the way.
      bool in_current_frame = true;
      for (; !frames_it.done(); frames_it.Advance()) {
        // TODO(clemensh): Implement stepping out from JS to wasm.
        if (frames_it.frame()->is_wasm()) continue;
        JavaScriptFrame* frame = JavaScriptFrame::cast(frames_it.frame());
        if (last_step_action() == StepIn) {
          // Deoptimize frame to ensure calls are checked for step-in.
          Deoptimizer::DeoptimizeFunction(frame->function());
        }
        HandleScope scope(isolate_);
        std::vector<Handle<SharedFunctionInfo>> infos;
        frame->GetFunctions(&infos);
        for (; !infos.empty(); current_frame_count--) {
          Handle<SharedFunctionInfo> info = infos.back();
          infos.pop_back();
          if (in_current_frame) {
            // We want to skip out, so skip the current frame.
            in_current_frame = false;
            continue;
          }
          if (IsBlackboxed(info)) continue;
          FloodWithOneShot(info);
          thread_local_.target_frame_count_ = current_frame_count;
          return;
        }
      }
      break;
    }
    case StepNext:
      thread_local_.target_frame_count_ = current_frame_count;
      V8_FALLTHROUGH;
    case StepIn:
      // TODO(clemensh): Implement stepping from JS into wasm.
      FloodWithOneShot(shared);
      break;
  }
}

// Simple function for returning the source positions for active break points.
Handle<Object> Debug::GetSourceBreakLocations(
    Isolate* isolate, Handle<SharedFunctionInfo> shared) {
  if (!shared->HasBreakInfo()) {
    return isolate->factory()->undefined_value();
  }

  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate);
  if (debug_info->GetBreakPointCount(isolate) == 0) {
    return isolate->factory()->undefined_value();
  }
  Handle<FixedArray> locations = isolate->factory()->NewFixedArray(
      debug_info->GetBreakPointCount(isolate));
  int count = 0;
  for (int i = 0; i < debug_info->break_points()->length(); ++i) {
    if (!debug_info->break_points()->get(i)->IsUndefined(isolate)) {
      BreakPointInfo break_point_info =
          BreakPointInfo::cast(debug_info->break_points()->get(i));
      int break_points = break_point_info->GetBreakPointCount(isolate);
      if (break_points == 0) continue;
      for (int j = 0; j < break_points; ++j) {
        locations->set(count++,
                       Smi::FromInt(break_point_info->source_position()));
      }
    }
  }
  return locations;
}

void Debug::ClearStepping() {
  // Clear the various stepping setup.
  ClearOneShot();

  thread_local_.last_step_action_ = StepNone;
  thread_local_.last_statement_position_ = kNoSourcePosition;
  thread_local_.ignore_step_into_function_ = Smi::kZero;
  thread_local_.fast_forward_to_return_ = false;
  thread_local_.last_frame_count_ = -1;
  thread_local_.target_frame_count_ = -1;
  thread_local_.break_on_next_function_call_ = false;
  UpdateHookOnFunctionCall();
}


// Clears all the one-shot break points that are currently set. Normally this
// function is called each time a break point is hit as one shot break points
// are used to support stepping.
void Debug::ClearOneShot() {
  // The current implementation just runs through all the breakpoints. When the
  // last break point for a function is removed that function is automatically
  // removed from the list.
  for (DebugInfoListNode* node = debug_info_list_; node != nullptr;
       node = node->next()) {
    Handle<DebugInfo> debug_info = node->debug_info();
    ClearBreakPoints(debug_info);
    ApplyBreakPoints(debug_info);
  }
}

class RedirectActiveFunctions : public ThreadVisitor {
 public:
  explicit RedirectActiveFunctions(SharedFunctionInfo shared)
      : shared_(shared) {
    DCHECK(shared->HasBytecodeArray());
  }

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
    for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
      JavaScriptFrame* frame = it.frame();
      JSFunction function = frame->function();
      if (!frame->is_interpreted()) continue;
      if (function->shared() != shared_) continue;
      InterpretedFrame* interpreted_frame =
          reinterpret_cast<InterpretedFrame*>(frame);
      BytecodeArray debug_copy = shared_->GetDebugInfo()->DebugBytecodeArray();
      interpreted_frame->PatchBytecodeArray(debug_copy);
    }
  }

 private:
  SharedFunctionInfo shared_;
  DisallowHeapAllocation no_gc_;
};

void Debug::DeoptimizeFunction(Handle<SharedFunctionInfo> shared) {
  // Deoptimize all code compiled from this shared function info including
  // inlining.
  isolate_->AbortConcurrentOptimization(BlockingBehavior::kBlock);

  // TODO(mlippautz): Try to remove this call.
  isolate_->heap()->PreciseCollectAllGarbage(
      Heap::kNoGCFlags, GarbageCollectionReason::kDebugger);

  bool found_something = false;
  Code::OptimizedCodeIterator iterator(isolate_);
  do {
    Code code = iterator.Next();
    if (code.is_null()) break;
    if (code->Inlines(*shared)) {
      code->set_marked_for_deoptimization(true);
      found_something = true;
    }
  } while (true);

  if (found_something) {
    // Only go through with the deoptimization if something was found.
    Deoptimizer::DeoptimizeMarkedCode(isolate_);
  }
}

void Debug::PrepareFunctionForDebugExecution(
    Handle<SharedFunctionInfo> shared) {
  // To prepare bytecode for debugging, we already need to have the debug
  // info (containing the debug copy) upfront, but since we do not recompile,
  // preparing for break points cannot fail.
  DCHECK(shared->is_compiled());
  DCHECK(shared->HasDebugInfo());
  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);
  if (debug_info->flags() & DebugInfo::kPreparedForDebugExecution) return;

  // Make a copy of the bytecode array if available.
  Handle<Object> maybe_original_bytecode_array =
      isolate_->factory()->undefined_value();
  if (shared->HasBytecodeArray()) {
    Handle<BytecodeArray> original_bytecode_array =
        handle(shared->GetBytecodeArray(), isolate_);
    Handle<BytecodeArray> debug_bytecode_array =
        isolate_->factory()->CopyBytecodeArray(original_bytecode_array);
    debug_info->set_debug_bytecode_array(*debug_bytecode_array);
    shared->SetDebugBytecodeArray(*debug_bytecode_array);
    maybe_original_bytecode_array = original_bytecode_array;
  }
  debug_info->set_original_bytecode_array(*maybe_original_bytecode_array);

  if (debug_info->CanBreakAtEntry()) {
    // Deopt everything in case the function is inlined anywhere.
    Deoptimizer::DeoptimizeAll(isolate_);
    InstallDebugBreakTrampoline();
  } else {
    DeoptimizeFunction(shared);
    // Update PCs on the stack to point to recompiled code.
    RedirectActiveFunctions redirect_visitor(*shared);
    redirect_visitor.VisitThread(isolate_, isolate_->thread_local_top());
    isolate_->thread_manager()->IterateArchivedThreads(&redirect_visitor);
  }
  debug_info->set_flags(debug_info->flags() |
                        DebugInfo::kPreparedForDebugExecution);
}

void Debug::InstallDebugBreakTrampoline() {
  // Check the list of debug infos whether the debug break trampoline needs to
  // be installed. If that's the case, iterate the heap for functions to rewire
  // to the trampoline.
  HandleScope scope(isolate_);
  // If there is a breakpoint at function entry, we need to install trampoline.
  bool needs_to_use_trampoline = false;
  // If there we break at entry to an api callback, we need to clear ICs.
  bool needs_to_clear_ic = false;
  for (DebugInfoListNode* current = debug_info_list_; current != nullptr;
       current = current->next()) {
    if (current->debug_info()->CanBreakAtEntry()) {
      needs_to_use_trampoline = true;
      if (current->debug_info()->shared()->IsApiFunction()) {
        needs_to_clear_ic = true;
        break;
      }
    }
  }

  if (!needs_to_use_trampoline) return;

  Handle<Code> trampoline = BUILTIN_CODE(isolate_, DebugBreakTrampoline);
  std::vector<Handle<JSFunction>> needs_compile;
  {
    HeapIterator iterator(isolate_->heap());
    for (HeapObject obj = iterator.next(); !obj.is_null();
         obj = iterator.next()) {
      if (needs_to_clear_ic && obj->IsFeedbackVector()) {
        FeedbackVector::cast(obj)->ClearSlots(isolate_);
        continue;
      } else if (obj->IsJSFunction()) {
        JSFunction fun = JSFunction::cast(obj);
        SharedFunctionInfo shared = fun->shared();
        if (!shared->HasDebugInfo()) continue;
        if (!shared->GetDebugInfo()->CanBreakAtEntry()) continue;
        if (!fun->is_compiled()) {
          needs_compile.push_back(handle(fun, isolate_));
        } else {
          fun->set_code(*trampoline);
        }
      }
    }
  }
  // By overwriting the function code with DebugBreakTrampoline, which tailcalls
  // to shared code, we bypass CompileLazy. Perform CompileLazy here instead.
  for (Handle<JSFunction> fun : needs_compile) {
    IsCompiledScope is_compiled_scope;
    Compiler::Compile(fun, Compiler::CLEAR_EXCEPTION, &is_compiled_scope);
    DCHECK(is_compiled_scope.is_compiled());
    fun->set_code(*trampoline);
  }
}

namespace {
template <typename Iterator>
void GetBreakablePositions(Iterator* it, int start_position, int end_position,
                           std::vector<BreakLocation>* locations) {
  while (!it->Done()) {
    if (it->position() >= start_position && it->position() < end_position) {
      locations->push_back(it->GetBreakLocation());
    }
    it->Next();
  }
}

void FindBreakablePositions(Handle<DebugInfo> debug_info, int start_position,
                            int end_position,
                            std::vector<BreakLocation>* locations) {
  DCHECK(debug_info->HasInstrumentedBytecodeArray());
  BreakIterator it(debug_info);
  GetBreakablePositions(&it, start_position, end_position, locations);
}
}  // namespace

bool Debug::GetPossibleBreakpoints(Handle<Script> script, int start_position,
                                   int end_position, bool restrict_to_function,
                                   std::vector<BreakLocation>* locations) {
  if (restrict_to_function) {
    Handle<Object> result =
        FindSharedFunctionInfoInScript(script, start_position);
    if (result->IsUndefined(isolate_)) return false;

    // Make sure the function has set up the debug info.
    Handle<SharedFunctionInfo> shared =
        Handle<SharedFunctionInfo>::cast(result);
    if (!EnsureBreakInfo(shared)) return false;
    PrepareFunctionForDebugExecution(shared);

    Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);
    FindBreakablePositions(debug_info, start_position, end_position, locations);
    return true;
  }

  while (true) {
    HandleScope scope(isolate_);
    std::vector<Handle<SharedFunctionInfo>> candidates;
    std::vector<IsCompiledScope> compiled_scopes;
    SharedFunctionInfo::ScriptIterator iterator(isolate_, *script);
    for (SharedFunctionInfo info = iterator.Next(); !info.is_null();
         info = iterator.Next()) {
      if (info->EndPosition() < start_position ||
          info->StartPosition() >= end_position) {
        continue;
      }
      if (!info->IsSubjectToDebugging()) continue;
      if (!info->is_compiled() && !info->allows_lazy_compilation()) continue;
      candidates.push_back(i::handle(info, isolate_));
    }

    bool was_compiled = false;
    for (const auto& candidate : candidates) {
      IsCompiledScope is_compiled_scope(candidate->is_compiled_scope());
      if (!is_compiled_scope.is_compiled()) {
        // Code that cannot be compiled lazily are internal and not debuggable.
        DCHECK(candidate->allows_lazy_compilation());
        if (!Compiler::Compile(candidate, Compiler::CLEAR_EXCEPTION,
                               &is_compiled_scope)) {
          return false;
        } else {
          was_compiled = true;
        }
      }
      DCHECK(is_compiled_scope.is_compiled());
      compiled_scopes.push_back(is_compiled_scope);
      if (!EnsureBreakInfo(candidate)) return false;
      PrepareFunctionForDebugExecution(candidate);
    }
    if (was_compiled) continue;

    for (const auto& candidate : candidates) {
      CHECK(candidate->HasBreakInfo());
      Handle<DebugInfo> debug_info(candidate->GetDebugInfo(), isolate_);
      FindBreakablePositions(debug_info, start_position, end_position,
                             locations);
    }
    return true;
  }
  UNREACHABLE();
}

MaybeHandle<JSArray> Debug::GetPrivateFields(Handle<JSReceiver> receiver) {
  Factory* factory = isolate_->factory();

  Handle<FixedArray> internal_fields;
  ASSIGN_RETURN_ON_EXCEPTION(isolate_, internal_fields,
                             JSReceiver::GetPrivateEntries(isolate_, receiver),
                             JSArray);

  int nof_internal_fields = internal_fields->length();
  if (nof_internal_fields == 0) {
    return factory->NewJSArray(0);
  }

  return factory->NewJSArrayWithElements(internal_fields);
}

class SharedFunctionInfoFinder {
 public:
  explicit SharedFunctionInfoFinder(int target_position)
      : current_start_position_(kNoSourcePosition),
        target_position_(target_position) {}

  void NewCandidate(SharedFunctionInfo shared,
                    JSFunction closure = JSFunction()) {
    if (!shared->IsSubjectToDebugging()) return;
    int start_position = shared->function_token_position();
    if (start_position == kNoSourcePosition) {
      start_position = shared->StartPosition();
    }

    if (start_position > target_position_) return;
    if (target_position_ > shared->EndPosition()) return;

    if (!current_candidate_.is_null()) {
      if (current_start_position_ == start_position &&
          shared->EndPosition() == current_candidate_->EndPosition()) {
        // If we already have a matching closure, do not throw it away.
        if (!current_candidate_closure_.is_null() && closure.is_null()) return;
        // If a top-level function contains only one function
        // declaration the source for the top-level and the function
        // is the same. In that case prefer the non top-level function.
        if (!current_candidate_->is_toplevel() && shared->is_toplevel()) return;
      } else if (start_position < current_start_position_ ||
                 current_candidate_->EndPosition() < shared->EndPosition()) {
        return;
      }
    }

    current_start_position_ = start_position;
    current_candidate_ = shared;
    current_candidate_closure_ = closure;
  }

  SharedFunctionInfo Result() { return current_candidate_; }

  JSFunction ResultClosure() { return current_candidate_closure_; }

 private:
  SharedFunctionInfo current_candidate_;
  JSFunction current_candidate_closure_;
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

    SharedFunctionInfo shared;
    IsCompiledScope is_compiled_scope;
    {
      SharedFunctionInfoFinder finder(position);
      SharedFunctionInfo::ScriptIterator iterator(isolate_, *script);
      for (SharedFunctionInfo info = iterator.Next(); !info.is_null();
           info = iterator.Next()) {
        finder.NewCandidate(info);
      }
      shared = finder.Result();
      if (shared.is_null()) break;
      // We found it if it's already compiled.
      is_compiled_scope = shared->is_compiled_scope();
      if (is_compiled_scope.is_compiled()) {
        Handle<SharedFunctionInfo> shared_handle(shared, isolate_);
        // If the iteration count is larger than 1, we had to compile the outer
        // function in order to create this shared function info. So there can
        // be no JSFunction referencing it. We can anticipate creating a debug
        // info while bypassing PrepareFunctionForDebugExecution.
        if (iteration > 1) {
          AllowHeapAllocation allow_before_return;
          CreateBreakInfo(shared_handle);
        }
        return shared_handle;
      }
    }
    // If not, compile to reveal inner functions.
    HandleScope scope(isolate_);
    // Code that cannot be compiled lazily are internal and not debuggable.
    DCHECK(shared->allows_lazy_compilation());
    if (!Compiler::Compile(handle(shared, isolate_), Compiler::CLEAR_EXCEPTION,
                           &is_compiled_scope)) {
      break;
    }
  }
  return isolate_->factory()->undefined_value();
}


// Ensures the debug information is present for shared.
bool Debug::EnsureBreakInfo(Handle<SharedFunctionInfo> shared) {
  // Return if we already have the break info for shared.
  if (shared->HasBreakInfo()) return true;
  if (!shared->IsSubjectToDebugging() && !CanBreakAtEntry(shared)) {
    return false;
  }
  IsCompiledScope is_compiled_scope = shared->is_compiled_scope();
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(shared, Compiler::CLEAR_EXCEPTION,
                         &is_compiled_scope)) {
    return false;
  }
  CreateBreakInfo(shared);
  return true;
}

void Debug::CreateBreakInfo(Handle<SharedFunctionInfo> shared) {
  HandleScope scope(isolate_);
  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);

  // Initialize with break information.

  DCHECK(!debug_info->HasBreakInfo());

  Factory* factory = isolate_->factory();
  Handle<FixedArray> break_points(
      factory->NewFixedArray(DebugInfo::kEstimatedNofBreakPointsInFunction));

  int flags = debug_info->flags();
  flags |= DebugInfo::kHasBreakInfo;
  if (CanBreakAtEntry(shared)) flags |= DebugInfo::kCanBreakAtEntry;
  debug_info->set_flags(flags);
  debug_info->set_break_points(*break_points);

  SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate_, shared);
}

Handle<DebugInfo> Debug::GetOrCreateDebugInfo(
    Handle<SharedFunctionInfo> shared) {
  if (shared->HasDebugInfo()) return handle(shared->GetDebugInfo(), isolate_);

  // Create debug info and add it to the list.
  Handle<DebugInfo> debug_info = isolate_->factory()->NewDebugInfo(shared);
  DebugInfoListNode* node = new DebugInfoListNode(isolate_, *debug_info);
  node->set_next(debug_info_list_);
  debug_info_list_ = node;

  return debug_info;
}

void Debug::InstallCoverageInfo(Handle<SharedFunctionInfo> shared,
                                Handle<CoverageInfo> coverage_info) {
  DCHECK(!coverage_info.is_null());

  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);

  DCHECK(!debug_info->HasCoverageInfo());

  debug_info->set_flags(debug_info->flags() | DebugInfo::kHasCoverageInfo);
  debug_info->set_coverage_info(*coverage_info);
}

void Debug::RemoveAllCoverageInfos() {
  ClearAllDebugInfos(
      [=](Handle<DebugInfo> info) { info->ClearCoverageInfo(isolate_); });
}

void Debug::ClearAllDebuggerHints() {
  ClearAllDebugInfos(
      [=](Handle<DebugInfo> info) { info->set_debugger_hints(0); });
}

void Debug::FindDebugInfo(Handle<DebugInfo> debug_info,
                          DebugInfoListNode** prev, DebugInfoListNode** curr) {
  HandleScope scope(isolate_);
  *prev = nullptr;
  *curr = debug_info_list_;
  while (*curr != nullptr) {
    if ((*curr)->debug_info().is_identical_to(debug_info)) return;
    *prev = *curr;
    *curr = (*curr)->next();
  }

  UNREACHABLE();
}

void Debug::ClearAllDebugInfos(const DebugInfoClearFunction& clear_function) {
  DebugInfoListNode* prev = nullptr;
  DebugInfoListNode* current = debug_info_list_;
  while (current != nullptr) {
    DebugInfoListNode* next = current->next();
    Handle<DebugInfo> debug_info = current->debug_info();
    clear_function(debug_info);
    if (debug_info->IsEmpty()) {
      FreeDebugInfoListNode(prev, current);
      current = next;
    } else {
      prev = current;
      current = next;
    }
  }
}

void Debug::RemoveBreakInfoAndMaybeFree(Handle<DebugInfo> debug_info) {
  debug_info->ClearBreakInfo(isolate_);
  if (debug_info->IsEmpty()) {
    DebugInfoListNode* prev;
    DebugInfoListNode* node;
    FindDebugInfo(debug_info, &prev, &node);
    FreeDebugInfoListNode(prev, node);
  }
}

void Debug::FreeDebugInfoListNode(DebugInfoListNode* prev,
                                  DebugInfoListNode* node) {
  DCHECK(node->debug_info()->IsEmpty());

  // Unlink from list. If prev is nullptr we are looking at the first element.
  if (prev == nullptr) {
    debug_info_list_ = node->next();
  } else {
    prev->set_next(node->next());
  }

  // Pack script back into the
  // SFI::script_or_debug_info field.
  Handle<DebugInfo> debug_info(node->debug_info());
  debug_info->shared()->set_script_or_debug_info(debug_info->script());

  delete node;
}

bool Debug::IsBreakAtReturn(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);

  // Get the executing function in which the debug break occurred.
  Handle<SharedFunctionInfo> shared(frame->function()->shared(), isolate_);

  // With no debug info there are no break points, so we can't be at a return.
  if (!shared->HasBreakInfo()) return false;

  DCHECK(!frame->is_optimized());
  Handle<DebugInfo> debug_info(shared->GetDebugInfo(), isolate_);
  BreakLocation location = BreakLocation::FromFrame(debug_info, frame);
  return location.IsReturn();
}

void Debug::ScheduleFrameRestart(StackFrame* frame) {
  // Set a target FP for the FrameDropperTrampoline builtin to drop to once
  // we return from the debugger.
  DCHECK(frame->is_java_script());
  // Only reschedule to a frame further below a frame we already scheduled for.
  if (frame->fp() <= thread_local_.restart_fp_) return;
  // If the frame is optimized, trigger a deopt and jump into the
  // FrameDropperTrampoline in the deoptimizer.
  thread_local_.restart_fp_ = frame->fp();

  // Reset break frame ID to the frame below the restarted frame.
  StackTraceFrameIterator it(isolate_);
  thread_local_.break_frame_id_ = StackFrame::NO_ID;
  for (StackTraceFrameIterator it(isolate_); !it.done(); it.Advance()) {
    if (it.frame()->fp() > thread_local_.restart_fp_) {
      thread_local_.break_frame_id_ = it.frame()->id();
      return;
    }
  }
}

Handle<FixedArray> Debug::GetLoadedScripts() {
  isolate_->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                      GarbageCollectionReason::kDebugger);
  Factory* factory = isolate_->factory();
  if (!factory->script_list()->IsWeakArrayList()) {
    return factory->empty_fixed_array();
  }
  Handle<WeakArrayList> array =
      Handle<WeakArrayList>::cast(factory->script_list());
  Handle<FixedArray> results = factory->NewFixedArray(array->length());
  int length = 0;
  {
    Script::Iterator iterator(isolate_);
    for (Script script = iterator.Next(); !script.is_null();
         script = iterator.Next()) {
      if (script->HasValidSource()) results->set(length++, script);
    }
  }
  return FixedArray::ShrinkOrEmpty(isolate_, results, length);
}

void Debug::OnThrow(Handle<Object> exception) {
  if (in_debug_scope() || ignore_events()) return;
  // Temporarily clear any scheduled_exception to allow evaluating
  // JavaScript from the debug event handler.
  HandleScope scope(isolate_);
  Handle<Object> scheduled_exception;
  if (isolate_->has_scheduled_exception()) {
    scheduled_exception = handle(isolate_->scheduled_exception(), isolate_);
    isolate_->clear_scheduled_exception();
  }
  Handle<Object> maybe_promise = isolate_->GetPromiseOnStackOnThrow();
  OnException(exception, maybe_promise,
              maybe_promise->IsJSPromise() ? v8::debug::kPromiseRejection
                                           : v8::debug::kException);
  if (!scheduled_exception.is_null()) {
    isolate_->thread_local_top()->scheduled_exception_ = *scheduled_exception;
  }
  PrepareStepOnThrow();
}

void Debug::OnPromiseReject(Handle<Object> promise, Handle<Object> value) {
  if (in_debug_scope() || ignore_events()) return;
  HandleScope scope(isolate_);
  // Check whether the promise has been marked as having triggered a message.
  Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
  if (!promise->IsJSObject() ||
      JSReceiver::GetDataProperty(Handle<JSObject>::cast(promise), key)
          ->IsUndefined(isolate_)) {
    OnException(value, promise, v8::debug::kPromiseRejection);
  }
}

bool Debug::IsExceptionBlackboxed(bool uncaught) {
  // Uncaught exception is blackboxed if all current frames are blackboxed,
  // caught exception if top frame is blackboxed.
  StackTraceFrameIterator it(isolate_);
  while (!it.done() && it.is_wasm()) it.Advance();
  bool is_top_frame_blackboxed =
      !it.done() ? IsFrameBlackboxed(it.javascript_frame()) : true;
  if (!uncaught || !is_top_frame_blackboxed) return is_top_frame_blackboxed;
  return AllFramesOnStackAreBlackboxed();
}

bool Debug::IsFrameBlackboxed(JavaScriptFrame* frame) {
  HandleScope scope(isolate_);
  std::vector<Handle<SharedFunctionInfo>> infos;
  frame->GetFunctions(&infos);
  for (const auto& info : infos) {
    if (!IsBlackboxed(info)) return false;
  }
  return true;
}

void Debug::OnException(Handle<Object> exception, Handle<Object> promise,
                        v8::debug::ExceptionType exception_type) {
  // TODO(kozyatinskiy): regress-662674.js test fails on arm without this.
  if (!AllowJavascriptExecution::IsAllowed(isolate_)) return;

  Isolate::CatchType catch_type = isolate_->PredictExceptionCatcher();

  // Don't notify listener of exceptions that are internal to a desugaring.
  if (catch_type == Isolate::CAUGHT_BY_DESUGARING) return;

  bool uncaught = catch_type == Isolate::NOT_CAUGHT;
  if (promise->IsJSObject()) {
    Handle<JSObject> jspromise = Handle<JSObject>::cast(promise);
    // Mark the promise as already having triggered a message.
    Handle<Symbol> key = isolate_->factory()->promise_debug_marker_symbol();
    Object::SetProperty(isolate_, jspromise, key, key, StoreOrigin::kMaybeKeyed,
                        Just(ShouldThrow::kThrowOnError))
        .Assert();
    // Check whether the promise reject is considered an uncaught exception.
    uncaught = !isolate_->PromiseHasUserDefinedRejectHandler(jspromise);
  }

  if (!debug_delegate_) return;

  // Bail out if exception breaks are not active
  if (uncaught) {
    // Uncaught exceptions are reported by either flags.
    if (!(break_on_uncaught_exception_ || break_on_exception_)) return;
  } else {
    // Caught exceptions are reported is activated.
    if (!break_on_exception_) return;
  }

  {
    JavaScriptFrameIterator it(isolate_);
    // Check whether the top frame is blackboxed or the break location is muted.
    if (!it.done() && (IsMutedAtCurrentLocation(it.frame()) ||
                       IsExceptionBlackboxed(uncaught))) {
      return;
    }
    if (it.done()) return;  // Do not trigger an event with an empty stack.
  }

  DebugScope debug_scope(this);
  HandleScope scope(isolate_);
  DisableBreak no_recursive_break(this);

  Handle<Context> native_context(isolate_->native_context());
  debug_delegate_->ExceptionThrown(
      v8::Utils::ToLocal(native_context), v8::Utils::ToLocal(exception),
      v8::Utils::ToLocal(promise), uncaught, exception_type);
}

void Debug::OnDebugBreak(Handle<FixedArray> break_points_hit) {
  DCHECK(!break_points_hit.is_null());
  // The caller provided for DebugScope.
  AssertDebugContext();
  // Bail out if there is no listener for this event
  if (ignore_events()) return;

#ifdef DEBUG
  PrintBreakLocation();
#endif  // DEBUG

  if (!debug_delegate_) return;
  DCHECK(in_debug_scope());
  HandleScope scope(isolate_);
  DisableBreak no_recursive_break(this);

  std::vector<int> inspector_break_points_hit;
  int inspector_break_points_count = 0;
  // This array contains breakpoints installed using JS debug API.
  for (int i = 0; i < break_points_hit->length(); ++i) {
    BreakPoint break_point = BreakPoint::cast(break_points_hit->get(i));
    inspector_break_points_hit.push_back(break_point->id());
    ++inspector_break_points_count;
  }

  Handle<Context> native_context(isolate_->native_context());
  debug_delegate_->BreakProgramRequested(v8::Utils::ToLocal(native_context),
                                         inspector_break_points_hit);
}

namespace {
debug::Location GetDebugLocation(Handle<Script> script, int source_position) {
  Script::PositionInfo info;
  Script::GetPositionInfo(script, source_position, &info, Script::WITH_OFFSET);
  // V8 provides ScriptCompiler::CompileFunctionInContext method which takes
  // expression and compile it as anonymous function like (function() ..
  // expression ..). To produce correct locations for stmts inside of this
  // expression V8 compile this function with negative offset. Instead of stmt
  // position blackboxing use function start position which is negative in
  // described case.
  return debug::Location(std::max(info.line, 0), std::max(info.column, 0));
}
}  // namespace

bool Debug::IsBlackboxed(Handle<SharedFunctionInfo> shared) {
  if (!debug_delegate_) return !shared->IsSubjectToDebugging();
  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);
  if (!debug_info->computed_debug_is_blackboxed()) {
    bool is_blackboxed =
        !shared->IsSubjectToDebugging() || !shared->script()->IsScript();
    if (!is_blackboxed) {
      SuppressDebug while_processing(this);
      HandleScope handle_scope(isolate_);
      PostponeInterruptsScope no_interrupts(isolate_);
      DisableBreak no_recursive_break(this);
      DCHECK(shared->script()->IsScript());
      Handle<Script> script(Script::cast(shared->script()), isolate_);
      DCHECK(script->IsUserJavaScript());
      debug::Location start = GetDebugLocation(script, shared->StartPosition());
      debug::Location end = GetDebugLocation(script, shared->EndPosition());
      is_blackboxed = debug_delegate_->IsFunctionBlackboxed(
          ToApiHandle<debug::Script>(script), start, end);
    }
    debug_info->set_debug_is_blackboxed(is_blackboxed);
    debug_info->set_computed_debug_is_blackboxed(true);
  }
  return debug_info->debug_is_blackboxed();
}

bool Debug::AllFramesOnStackAreBlackboxed() {
  HandleScope scope(isolate_);
  for (StackTraceFrameIterator it(isolate_); !it.done(); it.Advance()) {
    if (!it.is_javascript()) continue;
    if (!IsFrameBlackboxed(it.javascript_frame())) return false;
  }
  return true;
}

bool Debug::CanBreakAtEntry(Handle<SharedFunctionInfo> shared) {
  // Allow break at entry for builtin functions.
  if (shared->native() || shared->IsApiFunction()) {
    // Functions that are subject to debugging can have regular breakpoints.
    DCHECK(!shared->IsSubjectToDebugging());
    return true;
  }
  return false;
}

bool Debug::SetScriptSource(Handle<Script> script, Handle<String> source,
                            bool preview, debug::LiveEditResult* result) {
  DebugScope debug_scope(this);
  running_live_edit_ = true;
  LiveEdit::PatchScript(isolate_, script, source, preview, result);
  running_live_edit_ = false;
  return result->status == debug::LiveEditResult::OK;
}

void Debug::OnCompileError(Handle<Script> script) {
  ProcessCompileEvent(true, script);
}

void Debug::OnAfterCompile(Handle<Script> script) {
  ProcessCompileEvent(false, script);
}

void Debug::ProcessCompileEvent(bool has_compile_error, Handle<Script> script) {
  // TODO(kozyatinskiy): teach devtools to work with liveedit scripts better
  // first and then remove this fast return.
  if (running_live_edit_) return;
  // Attach the correct debug id to the script. The debug id is used by the
  // inspector to filter scripts by native context.
  script->set_context_data(isolate_->native_context()->debug_context_id());
  if (ignore_events()) return;
  if (!script->IsUserJavaScript() && script->type() != i::Script::TYPE_WASM) {
    return;
  }
  if (!debug_delegate_) return;
  SuppressDebug while_processing(this);
  DebugScope debug_scope(this);
  HandleScope scope(isolate_);
  DisableBreak no_recursive_break(this);
  AllowJavascriptExecution allow_script(isolate_);
  debug_delegate_->ScriptCompiled(ToApiHandle<debug::Script>(script),
                                  running_live_edit_, has_compile_error);
}

int Debug::CurrentFrameCount() {
  StackTraceFrameIterator it(isolate_);
  if (break_frame_id() != StackFrame::NO_ID) {
    // Skip to break frame.
    DCHECK(in_debug_scope());
    while (!it.done() && it.frame()->id() != break_frame_id()) it.Advance();
  }
  int counter = 0;
  while (!it.done()) {
    if (it.frame()->is_optimized()) {
      std::vector<SharedFunctionInfo> infos;
      OptimizedFrame::cast(it.frame())->GetFunctions(&infos);
      counter += infos.size();
    } else {
      counter++;
    }
    it.Advance();
  }
  return counter;
}

void Debug::SetDebugDelegate(debug::DebugDelegate* delegate) {
  debug_delegate_ = delegate;
  UpdateState();
}

void Debug::UpdateState() {
  bool is_active = debug_delegate_ != nullptr;
  if (is_active == is_active_) return;
  if (is_active) {
    // Note that the debug context could have already been loaded to
    // bootstrap test cases.
    isolate_->compilation_cache()->Disable();
    is_active = true;
    feature_tracker()->Track(DebugFeatureTracker::kActive);
  } else {
    isolate_->compilation_cache()->Enable();
    Unload();
  }
  is_active_ = is_active;
  isolate_->PromiseHookStateUpdated();
}

void Debug::UpdateHookOnFunctionCall() {
  STATIC_ASSERT(LastStepAction == StepIn);
  hook_on_function_call_ =
      thread_local_.last_step_action_ == StepIn ||
      isolate_->debug_execution_mode() == DebugInfo::kSideEffects ||
      thread_local_.break_on_next_function_call_;
}

void Debug::HandleDebugBreak(IgnoreBreakMode ignore_break_mode) {
  // Initialize LiveEdit.
  LiveEdit::InitializeThreadLocal(this);
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
    Object fun = it.frame()->function();
    if (fun->IsJSFunction()) {
      HandleScope scope(isolate_);
      Handle<JSFunction> function(JSFunction::cast(fun), isolate_);
      // Don't stop in builtin and blackboxed functions.
      Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
      bool ignore_break = ignore_break_mode == kIgnoreIfTopFrameBlackboxed
                              ? IsBlackboxed(shared)
                              : AllFramesOnStackAreBlackboxed();
      if (ignore_break) return;
      // Don't stop if the break location is muted.
      if (IsMutedAtCurrentLocation(it.frame())) return;
    }
  }

  // Clear stepping to avoid duplicate breaks.
  ClearStepping();

  HandleScope scope(isolate_);
  DebugScope debug_scope(this);

  OnDebugBreak(isolate_->factory()->empty_fixed_array());
}

#ifdef DEBUG
void Debug::PrintBreakLocation() {
  if (!FLAG_print_break_location) return;
  HandleScope scope(isolate_);
  StackTraceFrameIterator iterator(isolate_);
  if (iterator.done()) return;
  StandardFrame* frame = iterator.frame();
  FrameSummary summary = FrameSummary::GetTop(frame);
  int source_position = summary.SourcePosition();
  Handle<Object> script_obj = summary.script();
  PrintF("[debug] break in function '");
  summary.FunctionName()->PrintOn(stdout);
  PrintF("'.\n");
  if (script_obj->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_obj);
    Handle<String> source(String::cast(script->source()), isolate_);
    Script::InitLineEnds(script);
    int line =
        Script::GetLineNumber(script, source_position) - script->line_offset();
    int column = Script::GetColumnNumber(script, source_position) -
                 (line == 0 ? script->column_offset() : 0);
    Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()),
                                 isolate_);
    int line_start = line == 0 ? 0 : Smi::ToInt(line_ends->get(line - 1)) + 1;
    int line_end = Smi::ToInt(line_ends->get(line));
    DisallowHeapAllocation no_gc;
    String::FlatContent content = source->GetFlatContent(no_gc);
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
      prev_(reinterpret_cast<DebugScope*>(
          base::Relaxed_Load(&debug->thread_local_.current_debug_scope_))),
      no_interrupts_(debug_->isolate_) {
  // Link recursive debugger entry.
  base::Relaxed_Store(&debug_->thread_local_.current_debug_scope_,
                      reinterpret_cast<base::AtomicWord>(this));

  // Store the previous frame id and return value.
  break_frame_id_ = debug_->break_frame_id();

  // Create the new break info. If there is no proper frames there is no break
  // frame id.
  StackTraceFrameIterator it(isolate());
  bool has_frames = !it.done();
  debug_->thread_local_.break_frame_id_ =
      has_frames ? it.frame()->id() : StackFrame::NO_ID;

  debug_->UpdateState();
}


DebugScope::~DebugScope() {
  // Leaving this debugger entry.
  base::Relaxed_Store(&debug_->thread_local_.current_debug_scope_,
                      reinterpret_cast<base::AtomicWord>(prev_));

  // Restore to the previous break state.
  debug_->thread_local_.break_frame_id_ = break_frame_id_;

  debug_->UpdateState();
}

ReturnValueScope::ReturnValueScope(Debug* debug) : debug_(debug) {
  return_value_ = debug_->return_value_handle();
}

ReturnValueScope::~ReturnValueScope() {
  debug_->set_return_value(*return_value_);
}

void Debug::UpdateDebugInfosForExecutionMode() {
  // Walk all debug infos and update their execution mode if it is different
  // from the isolate execution mode.
  DebugInfoListNode* current = debug_info_list_;
  while (current != nullptr) {
    Handle<DebugInfo> debug_info = current->debug_info();
    if (debug_info->HasInstrumentedBytecodeArray() &&
        debug_info->DebugExecutionMode() != isolate_->debug_execution_mode()) {
      DCHECK(debug_info->shared()->HasBytecodeArray());
      if (isolate_->debug_execution_mode() == DebugInfo::kBreakpoints) {
        ClearSideEffectChecks(debug_info);
        ApplyBreakPoints(debug_info);
      } else {
        ClearBreakPoints(debug_info);
        ApplySideEffectChecks(debug_info);
      }
    }
    current = current->next();
  }
}

void Debug::StartSideEffectCheckMode() {
  DCHECK(isolate_->debug_execution_mode() != DebugInfo::kSideEffects);
  isolate_->set_debug_execution_mode(DebugInfo::kSideEffects);
  UpdateHookOnFunctionCall();
  side_effect_check_failed_ = false;

  DCHECK(!temporary_objects_);
  temporary_objects_.reset(new TemporaryObjectsTracker());
  isolate_->heap()->AddHeapObjectAllocationTracker(temporary_objects_.get());
  Handle<FixedArray> array(isolate_->native_context()->regexp_last_match_info(),
                           isolate_);
  regexp_match_info_ =
      Handle<RegExpMatchInfo>::cast(isolate_->factory()->CopyFixedArray(array));

  // Update debug infos to have correct execution mode.
  UpdateDebugInfosForExecutionMode();
}

void Debug::StopSideEffectCheckMode() {
  DCHECK(isolate_->debug_execution_mode() == DebugInfo::kSideEffects);
  if (side_effect_check_failed_) {
    DCHECK(isolate_->has_pending_exception());
    DCHECK_EQ(ReadOnlyRoots(isolate_).termination_exception(),
              isolate_->pending_exception());
    // Convert the termination exception into a regular exception.
    isolate_->CancelTerminateExecution();
    isolate_->Throw(*isolate_->factory()->NewEvalError(
        MessageTemplate::kNoSideEffectDebugEvaluate));
  }
  isolate_->set_debug_execution_mode(DebugInfo::kBreakpoints);
  UpdateHookOnFunctionCall();
  side_effect_check_failed_ = false;

  DCHECK(temporary_objects_);
  isolate_->heap()->RemoveHeapObjectAllocationTracker(temporary_objects_.get());
  temporary_objects_.reset();
  isolate_->native_context()->set_regexp_last_match_info(*regexp_match_info_);
  regexp_match_info_ = Handle<RegExpMatchInfo>::null();

  // Update debug infos to have correct execution mode.
  UpdateDebugInfosForExecutionMode();
}

void Debug::ApplySideEffectChecks(Handle<DebugInfo> debug_info) {
  DCHECK(debug_info->HasInstrumentedBytecodeArray());
  Handle<BytecodeArray> debug_bytecode(debug_info->DebugBytecodeArray(),
                                       isolate_);
  DebugEvaluate::ApplySideEffectChecks(debug_bytecode);
  debug_info->SetDebugExecutionMode(DebugInfo::kSideEffects);
}

void Debug::ClearSideEffectChecks(Handle<DebugInfo> debug_info) {
  DCHECK(debug_info->HasInstrumentedBytecodeArray());
  Handle<BytecodeArray> debug_bytecode(debug_info->DebugBytecodeArray(),
                                       isolate_);
  Handle<BytecodeArray> original(debug_info->OriginalBytecodeArray(), isolate_);
  for (interpreter::BytecodeArrayIterator it(debug_bytecode); !it.done();
       it.Advance()) {
    // Restore from original. This may copy only the scaling prefix, which is
    // correct, since we patch scaling prefixes to debug breaks if exists.
    debug_bytecode->set(it.current_offset(),
                        original->get(it.current_offset()));
  }
}

bool Debug::PerformSideEffectCheck(Handle<JSFunction> function,
                                   Handle<Object> receiver) {
  DCHECK_EQ(isolate_->debug_execution_mode(), DebugInfo::kSideEffects);
  DisallowJavascriptExecution no_js(isolate_);
  IsCompiledScope is_compiled_scope(function->shared()->is_compiled_scope());
  if (!function->is_compiled() &&
      !Compiler::Compile(function, Compiler::KEEP_EXCEPTION,
                         &is_compiled_scope)) {
    return false;
  }
  DCHECK(is_compiled_scope.is_compiled());
  Handle<SharedFunctionInfo> shared(function->shared(), isolate_);
  Handle<DebugInfo> debug_info = GetOrCreateDebugInfo(shared);
  DebugInfo::SideEffectState side_effect_state =
      debug_info->GetSideEffectState(isolate_);
  switch (side_effect_state) {
    case DebugInfo::kHasSideEffects:
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] Function %s failed side effect check.\n",
               function->shared()->DebugName()->ToCString().get());
      }
      side_effect_check_failed_ = true;
      // Throw an uncatchable termination exception.
      isolate_->TerminateExecution();
      return false;
    case DebugInfo::kRequiresRuntimeChecks: {
      if (!shared->HasBytecodeArray()) {
        return PerformSideEffectCheckForObject(receiver);
      }
      // If function has bytecode array then prepare function for debug
      // execution to perform runtime side effect checks.
      DCHECK(shared->is_compiled());
      PrepareFunctionForDebugExecution(shared);
      ApplySideEffectChecks(debug_info);
      return true;
    }
    case DebugInfo::kHasNoSideEffect:
      return true;
    case DebugInfo::kNotComputed:
      UNREACHABLE();
      return false;
  }
  UNREACHABLE();
  return false;
}

Handle<Object> Debug::return_value_handle() {
  return handle(thread_local_.return_value_, isolate_);
}

bool Debug::PerformSideEffectCheckForCallback(
    Handle<Object> callback_info, Handle<Object> receiver,
    Debug::AccessorKind accessor_kind) {
  DCHECK_EQ(!receiver.is_null(), callback_info->IsAccessorInfo());
  DCHECK_EQ(isolate_->debug_execution_mode(), DebugInfo::kSideEffects);
  if (!callback_info.is_null() && callback_info->IsCallHandlerInfo() &&
      i::CallHandlerInfo::cast(*callback_info)->NextCallHasNoSideEffect()) {
    return true;
  }
  // TODO(7515): always pass a valid callback info object.
  if (!callback_info.is_null()) {
    if (callback_info->IsAccessorInfo()) {
      // List of whitelisted internal accessors can be found in accessors.h.
      AccessorInfo info = AccessorInfo::cast(*callback_info);
      DCHECK_NE(kNotAccessor, accessor_kind);
      switch (accessor_kind == kSetter ? info->setter_side_effect_type()
                                       : info->getter_side_effect_type()) {
        case SideEffectType::kHasNoSideEffect:
          // We do not support setter accessors with no side effects, since
          // calling set accessors go through a store bytecode. Store bytecodes
          // are considered to cause side effects (to non-temporary objects).
          DCHECK_NE(kSetter, accessor_kind);
          return true;
        case SideEffectType::kHasSideEffectToReceiver:
          DCHECK(!receiver.is_null());
          if (PerformSideEffectCheckForObject(receiver)) return true;
          isolate_->OptionalRescheduleException(false);
          return false;
        case SideEffectType::kHasSideEffect:
          break;
      }
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] API Callback '");
        info->name()->ShortPrint();
        PrintF("' may cause side effect.\n");
      }
    } else if (callback_info->IsInterceptorInfo()) {
      InterceptorInfo info = InterceptorInfo::cast(*callback_info);
      if (info->has_no_side_effect()) return true;
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] API Interceptor may cause side effect.\n");
      }
    } else if (callback_info->IsCallHandlerInfo()) {
      CallHandlerInfo info = CallHandlerInfo::cast(*callback_info);
      if (info->IsSideEffectFreeCallHandlerInfo()) return true;
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] API CallHandlerInfo may cause side effect.\n");
      }
    }
  }
  side_effect_check_failed_ = true;
  // Throw an uncatchable termination exception.
  isolate_->TerminateExecution();
  isolate_->OptionalRescheduleException(false);
  return false;
}

bool Debug::PerformSideEffectCheckAtBytecode(InterpretedFrame* frame) {
  using interpreter::Bytecode;

  DCHECK_EQ(isolate_->debug_execution_mode(), DebugInfo::kSideEffects);
  SharedFunctionInfo shared = frame->function()->shared();
  BytecodeArray bytecode_array = shared->GetBytecodeArray();
  int offset = frame->GetBytecodeOffset();
  interpreter::BytecodeArrayAccessor bytecode_accessor(
      handle(bytecode_array, isolate_), offset);

  Bytecode bytecode = bytecode_accessor.current_bytecode();
  interpreter::Register reg;
  switch (bytecode) {
    case Bytecode::kStaCurrentContextSlot:
      reg = interpreter::Register::current_context();
      break;
    default:
      reg = bytecode_accessor.GetRegisterOperand(0);
      break;
  }
  Handle<Object> object =
      handle(frame->ReadInterpreterRegister(reg.index()), isolate_);
  return PerformSideEffectCheckForObject(object);
}

bool Debug::PerformSideEffectCheckForObject(Handle<Object> object) {
  DCHECK_EQ(isolate_->debug_execution_mode(), DebugInfo::kSideEffects);

  // We expect no side-effects for primitives.
  if (object->IsNumber()) return true;
  if (object->IsName()) return true;

  if (temporary_objects_->HasObject(Handle<HeapObject>::cast(object))) {
    return true;
  }

  if (FLAG_trace_side_effect_free_debug_evaluate) {
    PrintF("[debug-evaluate] failed runtime side effect check.\n");
  }
  side_effect_check_failed_ = true;
  // Throw an uncatchable termination exception.
  isolate_->TerminateExecution();
  return false;
}
}  // namespace internal
}  // namespace v8
