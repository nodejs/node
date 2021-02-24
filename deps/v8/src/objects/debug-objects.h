// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_H_
#define V8_OBJECTS_DEBUG_OBJECTS_H_

#include <memory>

#include "src/base/bit-field.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BreakPoint;
class BytecodeArray;

#include "torque-generated/src/objects/debug-objects-tq.inc"

// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo : public TorqueGeneratedDebugInfo<DebugInfo, Struct> {
 public:
  NEVER_READ_ONLY_SPACE
  DEFINE_TORQUE_GENERATED_DEBUG_INFO_FLAGS()

  // DebugInfo can be detached from the SharedFunctionInfo iff it is empty.
  bool IsEmpty() const;

  // --- Debug execution ---
  // -----------------------

  enum ExecutionMode { kBreakpoints = 0, kSideEffects = kDebugExecutionMode };

  // Returns current debug execution mode. Debug execution mode defines by
  // applied to bytecode patching. False for breakpoints, true for side effect
  // checks.
  ExecutionMode DebugExecutionMode() const;
  void SetDebugExecutionMode(ExecutionMode value);

  DECL_RELEASE_ACQUIRE_ACCESSORS(debug_bytecode_array, HeapObject)
  DECL_RELEASE_ACQUIRE_ACCESSORS(original_bytecode_array, HeapObject)

  // Specifies whether the associated function has an instrumented bytecode
  // array. If so, OriginalBytecodeArray returns the non-instrumented bytecode,
  // and DebugBytecodeArray returns the instrumented bytecode.
  inline bool HasInstrumentedBytecodeArray();

  inline BytecodeArray OriginalBytecodeArray();
  inline BytecodeArray DebugBytecodeArray();

  // --- Break points ---
  // --------------------

  bool HasBreakInfo() const;

  // Clears all fields related to break points.
  V8_EXPORT_PRIVATE void ClearBreakInfo(Isolate* isolate);

  // Accessors to flag whether to break before entering the function.
  // This is used to break for functions with no source, e.g. builtins.
  void SetBreakAtEntry();
  void ClearBreakAtEntry();
  bool BreakAtEntry() const;

  // Check if there is a break point at a source position.
  bool HasBreakPoint(Isolate* isolate, int source_position);
  // Attempt to clear a break point. Return true if successful.
  static bool ClearBreakPoint(Isolate* isolate, Handle<DebugInfo> debug_info,
                              Handle<BreakPoint> break_point);
  // Set a break point.
  static void SetBreakPoint(Isolate* isolate, Handle<DebugInfo> debug_info,
                            int source_position,
                            Handle<BreakPoint> break_point);
  // Get the break point objects for a source position.
  Handle<Object> GetBreakPoints(Isolate* isolate, int source_position);
  // Find the break point info holding this break point object.
  static Handle<Object> FindBreakPointInfo(Isolate* isolate,
                                           Handle<DebugInfo> debug_info,
                                           Handle<BreakPoint> break_point);
  // Get the number of break points for this function.
  int GetBreakPointCount(Isolate* isolate);

  // Returns whether we should be able to break before entering the function.
  // This is true for functions with no source, e.g. builtins.
  bool CanBreakAtEntry() const;

  // --- Debugger hint flags ---
  // ---------------------------

  // Indicates that the function should be skipped during stepping.
  DECL_BOOLEAN_ACCESSORS(debug_is_blackboxed)

  // Indicates that |debug_is_blackboxed| has been computed and set.
  DECL_BOOLEAN_ACCESSORS(computed_debug_is_blackboxed)

  // Indicates the side effect state.
  DECL_INT_ACCESSORS(side_effect_state)

  enum SideEffectState {
    kNotComputed = 0,
    kHasSideEffects = 1,
    kRequiresRuntimeChecks = 2,
    kHasNoSideEffect = 3,
  };

  SideEffectState GetSideEffectState(Isolate* isolate);

  // Id assigned to the function for debugging.
  // This could also be implemented as a weak hash table.
  DECL_INT_ACCESSORS(debugging_id)

  // Bit positions in |debugger_hints|.
  DEFINE_TORQUE_GENERATED_DEBUGGER_HINTS()

  static const int kNoDebuggingId = 0;

  // --- Block Coverage ---
  // ----------------------

  bool HasCoverageInfo() const;

  // Clears all fields related to block coverage.
  void ClearCoverageInfo(Isolate* isolate);

  static const int kEstimatedNofBreakPointsInFunction = 4;

 private:
  // Get the break point info object for a source position.
  Object GetBreakPointInfo(Isolate* isolate, int source_position);

  TQ_OBJECT_CONSTRUCTORS(DebugInfo)
};

// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo
    : public TorqueGeneratedBreakPointInfo<BreakPointInfo, Struct> {
 public:
  // Removes a break point.
  static void ClearBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                              Handle<BreakPoint> break_point);
  // Set a break point.
  static void SetBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                            Handle<BreakPoint> break_point);
  // Check if break point info has this break point.
  static bool HasBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                            Handle<BreakPoint> break_point);
  // Check if break point info has break point with this id.
  static MaybeHandle<BreakPoint> GetBreakPointById(Isolate* isolate,
                                                   Handle<BreakPointInfo> info,
                                                   int breakpoint_id);
  // Get the number of break points for this code offset.
  int GetBreakPointCount(Isolate* isolate);

  int GetStatementPosition(Handle<DebugInfo> debug_info);

  TQ_OBJECT_CONSTRUCTORS(BreakPointInfo)
};

// Holds information related to block code coverage.
class CoverageInfo
    : public TorqueGeneratedCoverageInfo<CoverageInfo, HeapObject> {
 public:
  int StartSourcePosition(int slot_index) const;
  int EndSourcePosition(int slot_index) const;
  int BlockCount(int slot_index) const;

  void InitializeSlot(int slot_index, int start_pos, int end_pos);
  void ResetBlockCount(int slot_index);

  // Computes the size for a CoverageInfo instance of a given length.
  static int SizeFor(int slot_count) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + slot_count * Slot::kSize);
  }

  // Print debug info.
  void CoverageInfoPrint(std::ostream& os,
                         std::unique_ptr<char[]> function_name = nullptr);

  class BodyDescriptor;  // GC visitor.

  // Description of layout within each slot.
  using Slot = TorqueGeneratedCoverageInfoSlotOffsets;

 private:
  int SlotFieldOffset(int slot_index, int field_offset) const;

  TQ_OBJECT_CONSTRUCTORS(CoverageInfo)
};

// Holds breakpoint related information. This object is used by inspector.
class BreakPoint : public TorqueGeneratedBreakPoint<BreakPoint, Struct> {
 public:
  TQ_OBJECT_CONSTRUCTORS(BreakPoint)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_H_
