// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/debug-objects.h"
#include "src/objects/debug-objects-inl.h"

namespace v8 {
namespace internal {

bool DebugInfo::IsEmpty() const { return flags() == kNone; }

bool DebugInfo::HasBreakInfo() const { return (flags() & kHasBreakInfo) != 0; }

bool DebugInfo::IsPreparedForBreakpoints() const {
  DCHECK(HasBreakInfo());
  return (flags() & kPreparedForBreakpoints) != 0;
}

bool DebugInfo::ClearBreakInfo() {
  Isolate* isolate = GetIsolate();

  set_debug_bytecode_array(isolate->heap()->undefined_value());
  set_break_points(isolate->heap()->empty_fixed_array());

  int new_flags = flags() & ~kHasBreakInfo & ~kPreparedForBreakpoints;
  set_flags(new_flags);

  return new_flags == kNone;
}

// Check if there is a break point at this source position.
bool DebugInfo::HasBreakPoint(int source_position) {
  DCHECK(HasBreakInfo());
  // Get the break point info object for this code offset.
  Object* break_point_info = GetBreakPointInfo(source_position);

  // If there is no break point info object or no break points in the break
  // point info object there is no break point at this code offset.
  if (break_point_info->IsUndefined(GetIsolate())) return false;
  return BreakPointInfo::cast(break_point_info)->GetBreakPointCount() > 0;
}

// Get the break point info object for this source position.
Object* DebugInfo::GetBreakPointInfo(int source_position) {
  DCHECK(HasBreakInfo());
  Isolate* isolate = GetIsolate();
  if (!break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < break_points()->length(); i++) {
      if (!break_points()->get(i)->IsUndefined(isolate)) {
        BreakPointInfo* break_point_info =
            BreakPointInfo::cast(break_points()->get(i));
        if (break_point_info->source_position() == source_position) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->heap()->undefined_value();
}

bool DebugInfo::ClearBreakPoint(Handle<DebugInfo> debug_info,
                                Handle<Object> break_point_object) {
  DCHECK(debug_info->HasBreakInfo());
  Isolate* isolate = debug_info->GetIsolate();
  if (debug_info->break_points()->IsUndefined(isolate)) return false;

  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) continue;
    Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
        BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
    if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                            break_point_object)) {
      BreakPointInfo::ClearBreakPoint(break_point_info, break_point_object);
      return true;
    }
  }
  return false;
}

void DebugInfo::SetBreakPoint(Handle<DebugInfo> debug_info, int source_position,
                              Handle<Object> break_point_object) {
  DCHECK(debug_info->HasBreakInfo());
  Isolate* isolate = debug_info->GetIsolate();
  Handle<Object> break_point_info(
      debug_info->GetBreakPointInfo(source_position), isolate);
  if (!break_point_info->IsUndefined(isolate)) {
    BreakPointInfo::SetBreakPoint(
        Handle<BreakPointInfo>::cast(break_point_info), break_point_object);
    return;
  }

  // Adding a new break point for a code offset which did not have any
  // break points before. Try to find a free slot.
  static const int kNoBreakPointInfo = -1;
  int index = kNoBreakPointInfo;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) {
      index = i;
      break;
    }
  }
  if (index == kNoBreakPointInfo) {
    // No free slot - extend break point info array.
    Handle<FixedArray> old_break_points = Handle<FixedArray>(
        FixedArray::cast(debug_info->break_points()), isolate);
    Handle<FixedArray> new_break_points = isolate->factory()->NewFixedArray(
        old_break_points->length() +
        DebugInfo::kEstimatedNofBreakPointsInFunction);

    debug_info->set_break_points(*new_break_points);
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  DCHECK_NE(index, kNoBreakPointInfo);

  // Allocate new BreakPointInfo object and set the break point.
  Handle<BreakPointInfo> new_break_point_info =
      isolate->factory()->NewBreakPointInfo(source_position);
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}

// Get the break point objects for a source position.
Handle<Object> DebugInfo::GetBreakPointObjects(int source_position) {
  DCHECK(HasBreakInfo());
  Object* break_point_info = GetBreakPointInfo(source_position);
  Isolate* isolate = GetIsolate();
  if (break_point_info->IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return Handle<Object>(
      BreakPointInfo::cast(break_point_info)->break_point_objects(), isolate);
}

// Get the total number of break points.
int DebugInfo::GetBreakPointCount() {
  DCHECK(HasBreakInfo());
  Isolate* isolate = GetIsolate();
  if (break_points()->IsUndefined(isolate)) return 0;
  int count = 0;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined(isolate)) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      count += break_point_info->GetBreakPointCount();
    }
  }
  return count;
}

Handle<Object> DebugInfo::FindBreakPointInfo(
    Handle<DebugInfo> debug_info, Handle<Object> break_point_object) {
  DCHECK(debug_info->HasBreakInfo());
  Isolate* isolate = debug_info->GetIsolate();
  if (!debug_info->break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < debug_info->break_points()->length(); i++) {
      if (!debug_info->break_points()->get(i)->IsUndefined(isolate)) {
        Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
            BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
        if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                                break_point_object)) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->factory()->undefined_value();
}

bool DebugInfo::HasCoverageInfo() const {
  return (flags() & kHasCoverageInfo) != 0;
}

bool DebugInfo::ClearCoverageInfo() {
  if (HasCoverageInfo()) {
    Isolate* isolate = GetIsolate();

    set_coverage_info(isolate->heap()->undefined_value());

    int new_flags = flags() & ~kHasCoverageInfo;
    set_flags(new_flags);
  }
  return flags() == kNone;
}

namespace {
bool IsEqual(Object* break_point1, Object* break_point2) {
  // TODO(kozyatinskiy): remove non-BreakPoint logic once the JS debug API has
  // been removed.
  if (break_point1->IsBreakPoint() != break_point2->IsBreakPoint())
    return false;
  if (!break_point1->IsBreakPoint()) return break_point1 == break_point2;
  return BreakPoint::cast(break_point1)->id() ==
         BreakPoint::cast(break_point2)->id();
}
}  // namespace

// Remove the specified break point object.
void BreakPointInfo::ClearBreakPoint(Handle<BreakPointInfo> break_point_info,
                                     Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) return;
  // If there is a single break point clear it if it is the same.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    if (IsEqual(break_point_info->break_point_objects(), *break_point_object)) {
      break_point_info->set_break_point_objects(
          isolate->heap()->undefined_value());
    }
    return;
  }
  // If there are multiple break points shrink the array
  DCHECK(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array = Handle<FixedArray>(
      FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() - 1);
  int found_count = 0;
  for (int i = 0; i < old_array->length(); i++) {
    if (IsEqual(old_array->get(i), *break_point_object)) {
      DCHECK_EQ(found_count, 0);
      found_count++;
    } else {
      new_array->set(i - found_count, old_array->get(i));
    }
  }
  // If the break point was found in the list change it.
  if (found_count > 0) break_point_info->set_break_point_objects(*new_array);
}

// Add the specified break point object.
void BreakPointInfo::SetBreakPoint(Handle<BreakPointInfo> break_point_info,
                                   Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();

  // If there was no break point objects before just set it.
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
    break_point_info->set_break_point_objects(*break_point_object);
    return;
  }
  // If the break point object is the same as before just ignore.
  if (break_point_info->break_point_objects() == *break_point_object) return;
  // If there was one break point object before replace with array.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(2);
    array->set(0, break_point_info->break_point_objects());
    array->set(1, *break_point_object);
    break_point_info->set_break_point_objects(*array);
    return;
  }
  // If there was more than one break point before extend array.
  Handle<FixedArray> old_array = Handle<FixedArray>(
      FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() + 1);
  for (int i = 0; i < old_array->length(); i++) {
    // If the break point was there before just ignore.
    if (IsEqual(old_array->get(i), *break_point_object)) return;
    new_array->set(i, old_array->get(i));
  }
  // Add the new break point.
  new_array->set(old_array->length(), *break_point_object);
  break_point_info->set_break_point_objects(*new_array);
}

bool BreakPointInfo::HasBreakPointObject(
    Handle<BreakPointInfo> break_point_info,
    Handle<Object> break_point_object) {
  // No break point.
  Isolate* isolate = break_point_info->GetIsolate();
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
    return false;
  }
  // Single break point.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    return IsEqual(break_point_info->break_point_objects(),
                   *break_point_object);
  }
  // Multiple break points.
  FixedArray* array = FixedArray::cast(break_point_info->break_point_objects());
  for (int i = 0; i < array->length(); i++) {
    if (IsEqual(array->get(i), *break_point_object)) {
      return true;
    }
  }
  return false;
}

// Get the number of break points.
int BreakPointInfo::GetBreakPointCount() {
  // No break point.
  if (break_point_objects()->IsUndefined(GetIsolate())) return 0;
  // Single break point.
  if (!break_point_objects()->IsFixedArray()) return 1;
  // Multiple break points.
  return FixedArray::cast(break_point_objects())->length();
}

int CoverageInfo::SlotCount() const {
  DCHECK_EQ(kFirstSlotIndex, length() % kSlotIndexCount);
  return (length() - kFirstSlotIndex) / kSlotIndexCount;
}

int CoverageInfo::StartSourcePosition(int slot_index) const {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  return Smi::ToInt(get(slot_start + kSlotStartSourcePositionIndex));
}

int CoverageInfo::EndSourcePosition(int slot_index) const {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  return Smi::ToInt(get(slot_start + kSlotEndSourcePositionIndex));
}

int CoverageInfo::BlockCount(int slot_index) const {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  return Smi::ToInt(get(slot_start + kSlotBlockCountIndex));
}

void CoverageInfo::InitializeSlot(int slot_index, int from_pos, int to_pos) {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  set(slot_start + kSlotStartSourcePositionIndex, Smi::FromInt(from_pos));
  set(slot_start + kSlotEndSourcePositionIndex, Smi::FromInt(to_pos));
  set(slot_start + kSlotBlockCountIndex, Smi::kZero);
}

void CoverageInfo::IncrementBlockCount(int slot_index) {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  const int old_count = BlockCount(slot_index);
  set(slot_start + kSlotBlockCountIndex, Smi::FromInt(old_count + 1));
}

void CoverageInfo::ResetBlockCount(int slot_index) {
  DCHECK_LT(slot_index, SlotCount());
  const int slot_start = CoverageInfo::FirstIndexForSlot(slot_index);
  set(slot_start + kSlotBlockCountIndex, Smi::kZero);
}

void CoverageInfo::Print(String* function_name) {
  DCHECK(FLAG_trace_block_coverage);
  DisallowHeapAllocation no_gc;

  OFStream os(stdout);
  os << "Coverage info (";
  if (function_name->length() > 0) {
    auto function_name_cstr = function_name->ToCString();
    os << function_name_cstr.get();
  } else {
    os << "{anonymous}";
  }
  os << "):" << std::endl;

  for (int i = 0; i < SlotCount(); i++) {
    os << "{" << StartSourcePosition(i) << "," << EndSourcePosition(i) << "}"
       << std::endl;
  }
}

}  // namespace internal
}  // namespace v8
