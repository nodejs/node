// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_H_
#define V8_OBJECTS_DEBUG_OBJECTS_H_

#include "src/objects.h"
#include "src/objects/fixed-array.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BreakPoint;
class BytecodeArray;

// The DebugInfo class holds additional information for a function being
// debugged.
class DebugInfo : public Struct {
 public:
  NEVER_READ_ONLY_SPACE
  enum Flag {
    kNone = 0,
    kHasBreakInfo = 1 << 0,
    kPreparedForDebugExecution = 1 << 1,
    kHasCoverageInfo = 1 << 2,
    kBreakAtEntry = 1 << 3,
    kCanBreakAtEntry = 1 << 4,
    kDebugExecutionMode = 1 << 5
  };

  typedef base::Flags<Flag> Flags;

  // A bitfield that lists uses of the current instance.
  DECL_INT_ACCESSORS(flags)

  // The shared function info for the source being debugged.
  DECL_ACCESSORS(shared, SharedFunctionInfo)

  // Bit field containing various information collected for debugging.
  DECL_INT_ACCESSORS(debugger_hints)

  // Script field from shared function info.
  DECL_ACCESSORS(script, Object)

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
  void ClearBreakInfo(Isolate* isolate);

  // Accessors to flag whether to break before entering the function.
  // This is used to break for functions with no source, e.g. builtins.
  void SetBreakAtEntry();
  void ClearBreakAtEntry();
  bool BreakAtEntry() const;

  // The original uninstrumented bytecode array for functions with break
  // points - the instrumented bytecode is held in the shared function info.
  DECL_ACCESSORS(original_bytecode_array, Object)

  // The debug instrumented bytecode array for functions with break points
  // - also pointed to by the shared function info.
  DECL_ACCESSORS(debug_bytecode_array, Object)

  // Fixed array holding status information for each active break point.
  DECL_ACCESSORS(break_points, FixedArray)

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
  DECL_INT_ACCESSORS(debugging_id);

// Bit positions in |debugger_hints|.
#define DEBUGGER_HINTS_BIT_FIELDS(V, _)       \
  V(SideEffectStateBits, int, 2, _)           \
  V(DebugIsBlackboxedBit, bool, 1, _)         \
  V(ComputedDebugIsBlackboxedBit, bool, 1, _) \
  V(DebuggingIdBits, int, 20, _)

  DEFINE_BIT_FIELDS(DEBUGGER_HINTS_BIT_FIELDS)
#undef DEBUGGER_HINTS_BIT_FIELDS

  static const int kNoDebuggingId = 0;

  // --- Block Coverage ---
  // ----------------------

  bool HasCoverageInfo() const;

  // Clears all fields related to block coverage.
  void ClearCoverageInfo(Isolate* isolate);
  DECL_ACCESSORS(coverage_info, Object)

  DECL_CAST(DebugInfo)

  // Dispatched behavior.
  DECL_PRINTER(DebugInfo)
  DECL_VERIFIER(DebugInfo)

// Layout description.
#define DEBUG_INFO_FIELDS(V)                   \
  V(kSharedFunctionInfoOffset, kTaggedSize)    \
  V(kDebuggerHintsOffset, kTaggedSize)         \
  V(kScriptOffset, kTaggedSize)                \
  V(kOriginalBytecodeArrayOffset, kTaggedSize) \
  V(kDebugBytecodeArrayOffset, kTaggedSize)    \
  V(kBreakPointsStateOffset, kTaggedSize)      \
  V(kFlagsOffset, kTaggedSize)                 \
  V(kCoverageInfoOffset, kTaggedSize)          \
  /* Total size. */                            \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize, DEBUG_INFO_FIELDS)
#undef DEBUG_INFO_FIELDS

  static const int kEstimatedNofBreakPointsInFunction = 4;

 private:
  // Get the break point info object for a source position.
  Object GetBreakPointInfo(Isolate* isolate, int source_position);

  OBJECT_CONSTRUCTORS(DebugInfo, Struct);
};

// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
class BreakPointInfo : public Tuple2 {
 public:
  // The position in the source for the break position.
  DECL_INT_ACCESSORS(source_position)
  // List of related JavaScript break points.
  DECL_ACCESSORS(break_points, Object)

  // Removes a break point.
  static void ClearBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                              Handle<BreakPoint> break_point);
  // Set a break point.
  static void SetBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                            Handle<BreakPoint> break_point);
  // Check if break point info has this break point.
  static bool HasBreakPoint(Isolate* isolate, Handle<BreakPointInfo> info,
                            Handle<BreakPoint> break_point);
  // Get the number of break points for this code offset.
  int GetBreakPointCount(Isolate* isolate);

  int GetStatementPosition(Handle<DebugInfo> debug_info);

  DECL_CAST(BreakPointInfo)

  static const int kSourcePositionOffset = kValue1Offset;
  static const int kBreakPointsOffset = kValue2Offset;

  OBJECT_CONSTRUCTORS(BreakPointInfo, Tuple2);
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
  void Print(std::unique_ptr<char[]> function_name);

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

  OBJECT_CONSTRUCTORS(CoverageInfo, FixedArray);
};

// Holds breakpoint related information. This object is used by inspector.
class BreakPoint : public Tuple2 {
 public:
  DECL_INT_ACCESSORS(id)
  DECL_ACCESSORS(condition, String)

  DECL_CAST(BreakPoint)

  static const int kIdOffset = kValue1Offset;
  static const int kConditionOffset = kValue2Offset;

  OBJECT_CONSTRUCTORS(BreakPoint, Tuple2);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_H_
