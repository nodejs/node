// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_INL_H_
#define V8_OBJECTS_DEBUG_OBJECTS_INL_H_

#include "src/objects/debug-objects.h"
// Include the non-inl header before the rest of the headers.

#include "src/codegen/optimized-compilation-info.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/objects/trusted-pointer-inl.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/debug-objects-tq-inl.inc"

int BreakPointInfo::source_position() const {
  return source_position_.load().value();
}
void BreakPointInfo::set_source_position(int value) {
  source_position_.store(this, Smi::FromInt(value));
}

Tagged<UnionOf<FixedArray, BreakPoint, Undefined>>
BreakPointInfo::break_points() const {
  return break_points_.load();
}
void BreakPointInfo::set_break_points(
    Tagged<UnionOf<FixedArray, BreakPoint, Undefined>> value,
    WriteBarrierMode mode) {
  break_points_.store(this, value, mode);
}

int BreakPoint::id() const { return id_.load().value(); }
void BreakPoint::set_id(int value) { id_.store(this, Smi::FromInt(value)); }

Tagged<String> BreakPoint::condition() const { return condition_.load(); }
void BreakPoint::set_condition(Tagged<String> value, WriteBarrierMode mode) {
  condition_.store(this, value, mode);
}

Tagged<SharedFunctionInfo> DebugInfo::shared() const { return shared_.load(); }
void DebugInfo::set_shared(Tagged<SharedFunctionInfo> value,
                           WriteBarrierMode mode) {
  shared_.store(this, value, mode);
}

int DebugInfo::debugger_hints() const { return debugger_hints_.load().value(); }
void DebugInfo::set_debugger_hints(int value) {
  debugger_hints_.store(this, Smi::FromInt(value));
}

Tagged<FixedArray> DebugInfo::break_points() const {
  return break_points_.load();
}
void DebugInfo::set_break_points(Tagged<FixedArray> value,
                                 WriteBarrierMode mode) {
  break_points_.store(this, value, mode);
}

DebugInfo::Flags DebugInfo::flags(RelaxedLoadTag) const {
  return Flags(flags_.Relaxed_Load().value());
}
void DebugInfo::set_flags(Flags value, RelaxedStoreTag) {
  flags_.Relaxed_Store(this, Smi::From31BitPattern(value));
}

Tagged<UnionOf<CoverageInfo, Undefined>> DebugInfo::coverage_info() const {
  return coverage_info_.load();
}
void DebugInfo::set_coverage_info(
    Tagged<UnionOf<CoverageInfo, Undefined>> value, WriteBarrierMode mode) {
  coverage_info_.store(this, value, mode);
}

Tagged<BytecodeArray> DebugInfo::original_bytecode_array(
    IsolateForSandbox isolate) const {
  return original_bytecode_array_.load(isolate);
}
Tagged<BytecodeArray> DebugInfo::original_bytecode_array(
    IsolateForSandbox isolate, AcquireLoadTag tag) const {
  return original_bytecode_array_.Acquire_Load(isolate);
}
void DebugInfo::set_original_bytecode_array(Tagged<BytecodeArray> value,
                                            WriteBarrierMode mode) {
  original_bytecode_array_.store(this, value, mode);
}
void DebugInfo::set_original_bytecode_array(Tagged<BytecodeArray> value,
                                            ReleaseStoreTag tag,
                                            WriteBarrierMode mode) {
  original_bytecode_array_.Release_Store(this, value, mode);
}
bool DebugInfo::has_original_bytecode_array() const {
  return !original_bytecode_array_.is_empty();
}
void DebugInfo::clear_original_bytecode_array() {
  original_bytecode_array_.clear(this);
}

Tagged<BytecodeArray> DebugInfo::debug_bytecode_array(
    IsolateForSandbox isolate) const {
  return debug_bytecode_array_.load(isolate);
}
Tagged<BytecodeArray> DebugInfo::debug_bytecode_array(
    IsolateForSandbox isolate, AcquireLoadTag tag) const {
  return debug_bytecode_array_.Acquire_Load(isolate);
}
void DebugInfo::set_debug_bytecode_array(Tagged<BytecodeArray> value,
                                         WriteBarrierMode mode) {
  debug_bytecode_array_.store(this, value, mode);
}
void DebugInfo::set_debug_bytecode_array(Tagged<BytecodeArray> value,
                                         ReleaseStoreTag tag,
                                         WriteBarrierMode mode) {
  debug_bytecode_array_.Release_Store(this, value, mode);
}
bool DebugInfo::has_debug_bytecode_array() const {
  return !debug_bytecode_array_.is_empty();
}
void DebugInfo::clear_debug_bytecode_array() {
  debug_bytecode_array_.clear(this);
}

BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, side_effect_state,
                    DebugInfo::SideEffectStateBits)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, debug_is_blackboxed,
                    DebugInfo::DebugIsBlackboxedBit)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, computed_debug_is_blackboxed,
                    DebugInfo::ComputedDebugIsBlackboxedBit)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, debugging_id,
                    DebugInfo::DebuggingIdBits)

bool DebugInfo::HasInstrumentedBytecodeArray() {
  return has_debug_bytecode_array();
}

Tagged<BytecodeArray> DebugInfo::OriginalBytecodeArray(Isolate* isolate) {
  DCHECK(HasInstrumentedBytecodeArray());
  return original_bytecode_array(isolate, kAcquireLoad);
}

Tagged<BytecodeArray> DebugInfo::DebugBytecodeArray(Isolate* isolate) {
  DCHECK(HasInstrumentedBytecodeArray());
  Tagged<BytecodeArray> result = debug_bytecode_array(isolate, kAcquireLoad);
  DCHECK_EQ(shared()->GetActiveBytecodeArray(isolate), result);
  return result;
}

Tagged<UnionOf<SharedFunctionInfo, Script>> StackFrameInfo::shared_or_script()
    const {
  return shared_or_script_.load();
}
void StackFrameInfo::set_shared_or_script(
    Tagged<UnionOf<SharedFunctionInfo, Script>> value, WriteBarrierMode mode) {
  shared_or_script_.store(this, value, mode);
}

Tagged<String> StackFrameInfo::function_name() const {
  return function_name_.load();
}
void StackFrameInfo::set_function_name(Tagged<String> value,
                                       WriteBarrierMode mode) {
  function_name_.store(this, value, mode);
}

int StackFrameInfo::flags() const { return flags_.load().value(); }
void StackFrameInfo::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

Tagged<Script> StackFrameInfo::script() const {
  Tagged<HeapObject> object = shared_or_script();
  if (IsSharedFunctionInfo(object)) {
    object = Cast<SharedFunctionInfo>(object)->script();
  }
  return Cast<Script>(object);
}

BIT_FIELD_ACCESSORS(StackFrameInfo, flags, bytecode_offset_or_source_position,
                    StackFrameInfo::BytecodeOffsetOrSourcePositionBits)
BIT_FIELD_ACCESSORS(StackFrameInfo, flags, is_constructor,
                    StackFrameInfo::IsConstructorBit)

int StackTraceInfo::id() const { return id_.load().value(); }
void StackTraceInfo::set_id(int value) { id_.store(this, Smi::FromInt(value)); }

Tagged<FixedArray> StackTraceInfo::frames() const { return frames_.load(); }
void StackTraceInfo::set_frames(Tagged<FixedArray> value,
                                WriteBarrierMode mode) {
  frames_.store(this, value, mode);
}

inline int StackTraceInfo::length() const {
  // TODO(375937549): Convert to uint32_t.
  return static_cast<int>(frames()->ulength().value());
}

inline Tagged<StackFrameInfo> StackTraceInfo::get(int index) const {
  return Cast<StackFrameInfo>(frames()->get(index));
}

bool ErrorStackData::HasFormattedStack() const {
  return !IsFixedArray(raw_data_for_call_site_infos_or_formatted_stack());
}

Tagged<JSAny> ErrorStackData::formatted_stack() const {
  DCHECK(HasFormattedStack());
  return Cast<JSAny>(
      raw_data_for_call_site_infos_or_formatted_stack_.Relaxed_Load());
}
void ErrorStackData::set_formatted_stack(Tagged<JSAny> value,
                                         WriteBarrierMode mode) {
  raw_data_for_call_site_infos_or_formatted_stack_.Relaxed_Store(this, value,
                                                                 mode);
}

bool ErrorStackData::HasRawDataForCallSiteInfos() const {
  return !HasFormattedStack();
}

Tagged<FixedArray> ErrorStackData::raw_data_for_call_site_infos() const {
  DCHECK(HasRawDataForCallSiteInfos());
  return Cast<FixedArray>(raw_data_for_call_site_infos_or_formatted_stack());
}

void ErrorStackData::set_raw_data_for_call_site_infos(Tagged<FixedArray> value,
                                                      WriteBarrierMode mode) {
  DCHECK(HasRawDataForCallSiteInfos());
  set_raw_data_for_call_site_infos_or_formatted_stack(value, mode);
}

Tagged<UnionOf<FixedArray, JSAny>>
ErrorStackData::raw_data_for_call_site_infos_or_formatted_stack() const {
  return raw_data_for_call_site_infos_or_formatted_stack_.load();
}
void ErrorStackData::set_raw_data_for_call_site_infos_or_formatted_stack(
    Tagged<UnionOf<FixedArray, JSAny>> value, WriteBarrierMode mode) {
  raw_data_for_call_site_infos_or_formatted_stack_.store(this, value, mode);
}

Tagged<StackTraceInfo> ErrorStackData::stack_trace() const {
  return stack_trace_.load();
}
void ErrorStackData::set_stack_trace(Tagged<StackTraceInfo> value,
                                     WriteBarrierMode mode) {
  stack_trace_.store(this, value, mode);
}

// CoverageInfo.
int32_t CoverageInfo::slot_count() const { return slot_count_; }
void CoverageInfo::set_slot_count(int32_t value) { slot_count_ = value; }

int32_t CoverageInfo::slots_start_source_position(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  return slots()[i].start_source_position;
}
void CoverageInfo::set_slots_start_source_position(int i, int32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  slots()[i].start_source_position = value;
}
int32_t CoverageInfo::slots_end_source_position(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  return slots()[i].end_source_position;
}
void CoverageInfo::set_slots_end_source_position(int i, int32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  slots()[i].end_source_position = value;
}
int32_t CoverageInfo::slots_block_count(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  return slots()[i].block_count;
}
void CoverageInfo::set_slots_block_count(int i, int32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  slots()[i].block_count = value;
}
int32_t CoverageInfo::slots_padding(int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  return slots()[i].padding;
}
void CoverageInfo::set_slots_padding(int i, int32_t value) {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, slot_count());
  slots()[i].padding = value;
}
int CoverageInfo::AllocatedSize() const { return SizeFor(slot_count()); }

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_INL_H_
