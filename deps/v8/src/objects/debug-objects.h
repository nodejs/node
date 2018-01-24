// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_H_
#define V8_OBJECTS_DEBUG_OBJECTS_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BytecodeArray;

// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo : public Struct {
 public:
  enum Flag {
    kNone = 0,
    kHasBreakInfo = 1 << 0,
    kPreparedForBreakpoints = 1 << 1,
    kHasCoverageInfo = 2 << 1,
  };
  typedef base::Flags<Flag> Flags;

  // A bitfield that lists uses of the current instance.
  DECL_INT_ACCESSORS(flags)

  // The shared function info for the source being debugged.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  // Bit field containing various information collected for debugging.
  DECL_INT_ACCESSORS(debugger_hints)

  // DebugInfo can be detached from the SharedFunctionInfo iff it is empty.
  bool IsEmpty() const;

  // --- Break points ---
  // --------------------

  bool HasBreakInfo() const;

  bool IsPreparedForBreakpoints() const;

  // Clears all fields related to break points. Returns true iff the
  // DebugInfo is now empty.
  bool ClearBreakInfo();

  // The instrumented bytecode array for functions with break points.
  DECL_ACCESSORS(debug_bytecode_array, Object)

  // Fixed array holding status information for each active break point.
  DECL_ACCESSORS(break_points, FixedArray)

  // Check if there is a break point at a source position.
  bool HasBreakPoint(int source_position);
  // Attempt to clear a break point. Return true if successful.
  static bool ClearBreakPoint(Handle<DebugInfo> debug_info,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<DebugInfo> debug_info, int source_position,
                            Handle<Object> break_point_object);
  // Get the break point objects for a source position.
  Handle<Object> GetBreakPointObjects(int source_position);
  // Find the break point info holding this break point object.
  static Handle<Object> FindBreakPointInfo(Handle<DebugInfo> debug_info,
                                           Handle<Object> break_point_object);
  // Get the number of break points for this function.
  int GetBreakPointCount();

  inline bool HasDebugBytecodeArray();

  inline BytecodeArray* OriginalBytecodeArray();
  inline BytecodeArray* DebugBytecodeArray();

  // --- Block Coverage ---
  // ----------------------

  bool HasCoverageInfo() const;

  // Clears all fields related to block coverage. Returns true iff the
  // DebugInfo is now empty.
  bool ClearCoverageInfo();
  DECL_ACCESSORS(coverage_info, Object)

  DECL_CAST(DebugInfo)

  // Dispatched behavior.
  DECL_PRINTER(DebugInfo)
  DECL_VERIFIER(DebugInfo)

  static const int kSharedFunctionInfoOffset = Struct::kHeaderSize;
  static const int kDebuggerHintsOffset =
      kSharedFunctionInfoOffset + kPointerSize;
  static const int kDebugBytecodeArrayOffset =
      kDebuggerHintsOffset + kPointerSize;
  static const int kBreakPointsStateOffset =
      kDebugBytecodeArrayOffset + kPointerSize;
  static const int kFlagsOffset = kBreakPointsStateOffset + kPointerSize;
  static const int kCoverageInfoOffset = kFlagsOffset + kPointerSize;
  static const int kSize = kCoverageInfoOffset + kPointerSize;

  static const int kEstimatedNofBreakPointsInFunction = 4;

 private:
  // Get the break point info object for a source position.
  Object* GetBreakPointInfo(int source_position);

  DISALLOW_IMPLICIT_CONSTRUCTORS(DebugInfo);
};

// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo : public Tuple2 {
 public:
  // The position in the source for the break position.
  DECL_INT_ACCESSORS(source_position)
  // List of related JavaScript break points.
  DECL_ACCESSORS(break_point_objects, Object)

  // Removes a break point.
  static void ClearBreakPoint(Handle<BreakPointInfo> info,
                              Handle<Object> break_point_object);
  // Set a break point.
  static void SetBreakPoint(Handle<BreakPointInfo> info,
                            Handle<Object> break_point_object);
  // Check if break point info has this break point object.
  static bool HasBreakPointObject(Handle<BreakPointInfo> info,
                                  Handle<Object> break_point_object);
  // Get the number of break points for this code offset.
  int GetBreakPointCount();

  int GetStatementPosition(Handle<DebugInfo> debug_info);

  DECL_CAST(BreakPointInfo)

  static const int kSourcePositionOffset = kValue1Offset;
  static const int kBreakPointObjectsOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BreakPointInfo);
};

// Holds information related to block code coverage.
class CoverageInfo : public FixedArray {
 public:
  int SlotCount() const;

  int StartSourcePosition(int slot_index) const;
  int EndSourcePosition(int slot_index) const;
  int BlockCount(int slot_index) const;

  void InitializeSlot(int slot_index, int start_pos, int end_pos);
  void IncrementBlockCount(int slot_index);
  void ResetBlockCount(int slot_index);

  static int FixedArrayLengthForSlotCount(int slot_count) {
    return slot_count * kSlotIndexCount + kFirstSlotIndex;
  }

  DECL_CAST(CoverageInfo)

  // Print debug info.
  void Print(String* function_name);

 private:
  static int FirstIndexForSlot(int slot_index) {
    return kFirstSlotIndex + slot_index * kSlotIndexCount;
  }

  static const int kFirstSlotIndex = 0;

  // Each slot is assigned a group of indices starting at kFirstSlotIndex.
  // Within this group, semantics are as follows:
  static const int kSlotStartSourcePositionIndex = 0;
  static const int kSlotEndSourcePositionIndex = 1;
  static const int kSlotBlockCountIndex = 2;
  static const int kSlotIndexCount = 3;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CoverageInfo);
};

// Holds breakpoint related information. This object is used by inspector.
class BreakPoint : public Tuple2 {
 public:
  DECL_INT_ACCESSORS(id)
  DECL_ACCESSORS(condition, String)

  DECL_CAST(BreakPoint)

  static const int kIdOffset = kValue1Offset;
  static const int kConditionOffset = kValue2Offset;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BreakPoint);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_H_
