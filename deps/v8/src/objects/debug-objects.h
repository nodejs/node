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
#include "src/objects/trusted-pointer.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class DebugInfo;
class BreakPoint;
class BreakPointInfo;
class StackFrameInfo;
class StackTraceInfo;
class BytecodeArray;
class StructBodyDescriptor;

#include "torque-generated/src/objects/debug-objects-tq.inc"

// The DebugInfo class holds additional information for a function being
// debugged.
V8_OBJECT class DebugInfo : public Struct {
 public:
  DEFINE_TORQUE_GENERATED_DEBUG_INFO_FLAGS()

  // DebugInfo can be detached from the SharedFunctionInfo iff it is empty.
  bool IsEmpty() const;

  // --- Debug execution ---
  // -----------------------

  enum ExecutionMode : uint8_t {
    kBreakpoints = 0,
    kSideEffects = kDebugExecutionMode
  };

  // Returns current debug execution mode. Debug execution mode defines by
  // applied to bytecode patching. False for breakpoints, true for side effect
  // checks.
  ExecutionMode DebugExecutionMode() const;
  void SetDebugExecutionMode(ExecutionMode value);

  // Specifies whether the associated function has an instrumented bytecode
  // array. If so, OriginalBytecodeArray returns the non-instrumented bytecode,
  // and DebugBytecodeArray returns the instrumented bytecode.
  inline bool HasInstrumentedBytecodeArray();

  inline Tagged<BytecodeArray> OriginalBytecodeArray(Isolate* isolate);
  inline Tagged<BytecodeArray> DebugBytecodeArray(Isolate* isolate);

  DECL_TRUSTED_POINTER_ACCESSORS(original_bytecode_array, BytecodeArray)
  DECL_TRUSTED_POINTER_ACCESSORS(debug_bytecode_array, BytecodeArray)

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
  static bool ClearBreakPoint(Isolate* isolate,
                              DirectHandle<DebugInfo> debug_info,
                              DirectHandle<BreakPoint> break_point);
  // Set a break point.
  static void SetBreakPoint(Isolate* isolate,
                            DirectHandle<DebugInfo> debug_info,
                            int source_position,
                            DirectHandle<BreakPoint> break_point);
  // Get the break point objects for a source position.
  DirectHandle<Object> GetBreakPoints(Isolate* isolate, int source_position);
  // Find the break point info holding this break point object.
  static DirectHandle<Object> FindBreakPointInfo(
      Isolate* isolate, DirectHandle<DebugInfo> debug_info,
      DirectHandle<BreakPoint> break_point);
  // Get the number of break points for this function.
  uint32_t GetBreakPointCount(Isolate* isolate);

  // Returns whether we should be able to break before entering the function.
  // This is true for functions with no source, e.g. builtins.
  bool CanBreakAtEntry() const;

  // --- Debugger hint flags ---
  // ---------------------------

  // Indicates that the function should be skipped during stepping.
  inline bool debug_is_blackboxed() const;
  inline void set_debug_is_blackboxed(bool value);

  // Indicates that |debug_is_blackboxed| has been computed and set.
  inline bool computed_debug_is_blackboxed() const;
  inline void set_computed_debug_is_blackboxed(bool value);

  // Indicates the side effect state.
  inline int side_effect_state() const;
  inline void set_side_effect_state(int value);

  enum SideEffectState {
    kNotComputed = 0,
    kHasSideEffects = 1,
    kRequiresRuntimeChecks = 2,
    kHasNoSideEffect = 3,
  };

  SideEffectState GetSideEffectState(Isolate* isolate);

  // Id assigned to the function for debugging.
  // This could also be implemented as a weak hash table.
  inline int debugging_id() const;
  inline void set_debugging_id(int value);

  // Bit positions in |debugger_hints|.
  DEFINE_TORQUE_GENERATED_DEBUGGER_HINTS()

  static const int kNoDebuggingId = 0;

  // --- Block Coverage ---
  // ----------------------

  bool HasCoverageInfo() const;

  // Clears all fields related to block coverage.
  void ClearCoverageInfo(Isolate* isolate);

  static const int kEstimatedNofBreakPointsInFunction = 4;

  DECL_PRINTER(DebugInfo)
  DECL_VERIFIER(DebugInfo)

  class BodyDescriptor;

  inline Tagged<SharedFunctionInfo> shared() const;
  inline void set_shared(Tagged<SharedFunctionInfo> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int debugger_hints() const;
  inline void set_debugger_hints(int value);

  inline Tagged<FixedArray> break_points() const;
  inline void set_break_points(Tagged<FixedArray> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Flags flags(RelaxedLoadTag) const;
  inline void set_flags(Flags value, RelaxedStoreTag);

  inline Tagged<UnionOf<CoverageInfo, Undefined>> coverage_info() const;
  inline void set_coverage_info(Tagged<UnionOf<CoverageInfo, Undefined>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

 private:
  // Get the break point info object for a source position.
  Tagged<Object> GetBreakPointInfo(Isolate* isolate, int source_position);

 public:
  TaggedMember<SharedFunctionInfo> shared_;
  TaggedMember<Smi> debugger_hints_;
  TaggedMember<FixedArray> break_points_;
  TaggedMember<Smi> flags_;
  TaggedMember<UnionOf<CoverageInfo, Undefined>> coverage_info_;
  TrustedPointerMember<BytecodeArray, kBytecodeArrayIndirectPointerTag>
      original_bytecode_array_;
  TrustedPointerMember<BytecodeArray, kBytecodeArrayIndirectPointerTag>
      debug_bytecode_array_;
} V8_OBJECT_END;

// The BreakPointInfo class holds information for break points set in a
// function. The DebugInfo object holds a BreakPointInfo object for each code
// position with one or more break points.
V8_OBJECT class BreakPointInfo : public Struct {
 public:
  // Accessors
  inline int source_position() const;
  inline void set_source_position(int value);

  inline Tagged<UnionOf<FixedArray, BreakPoint, Undefined>> break_points()
      const;
  inline void set_break_points(
      Tagged<UnionOf<FixedArray, BreakPoint, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Removes a break point.
  static void ClearBreakPoint(Isolate* isolate,
                              DirectHandle<BreakPointInfo> info,
                              DirectHandle<BreakPoint> break_point);
  // Set a break point.
  static void SetBreakPoint(Isolate* isolate, DirectHandle<BreakPointInfo> info,
                            DirectHandle<BreakPoint> break_point);
  // Check if break point info has this break point.
  static bool HasBreakPoint(Isolate* isolate, DirectHandle<BreakPointInfo> info,
                            DirectHandle<BreakPoint> break_point);
  // Check if break point info has break point with this id.
  static MaybeDirectHandle<BreakPoint> GetBreakPointById(
      Isolate* isolate, DirectHandle<BreakPointInfo> info, int breakpoint_id);
  // Get the number of break points for this code offset.
  uint32_t GetBreakPointCount(Isolate* isolate);

  int GetStatementPosition(Handle<DebugInfo> debug_info);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(BreakPointInfo)
  DECL_VERIFIER(BreakPointInfo)

  TaggedMember<Smi> source_position_;
  TaggedMember<UnionOf<FixedArray, BreakPoint, Undefined>> break_points_;
} V8_OBJECT_END;

// Layout of a single slot within CoverageInfo.
struct CoverageInfoSlot {
  int32_t start_source_position;
  int32_t end_source_position;
  int32_t block_count;
  int32_t padding;

  static const int kSize;
  static const int kStartSourcePositionOffset;
  static const int kEndSourcePositionOffset;
  static const int kBlockCountOffset;
  static const int kPaddingOffset;
};
static_assert(sizeof(CoverageInfoSlot) == 16);

inline constexpr int CoverageInfoSlot::kSize = sizeof(CoverageInfoSlot);
inline constexpr int CoverageInfoSlot::kStartSourcePositionOffset =
    offsetof(CoverageInfoSlot, start_source_position);
inline constexpr int CoverageInfoSlot::kEndSourcePositionOffset =
    offsetof(CoverageInfoSlot, end_source_position);
inline constexpr int CoverageInfoSlot::kBlockCountOffset =
    offsetof(CoverageInfoSlot, block_count);
inline constexpr int CoverageInfoSlot::kPaddingOffset =
    offsetof(CoverageInfoSlot, padding);

// Holds information related to block code coverage.
V8_OBJECT class CoverageInfo : public HeapObjectLayout {
 public:
  inline int32_t slot_count() const;
  inline void set_slot_count(int32_t value);

  inline int32_t slots_start_source_position(int i) const;
  inline void set_slots_start_source_position(int i, int32_t value);
  inline int32_t slots_end_source_position(int i) const;
  inline void set_slots_end_source_position(int i, int32_t value);
  inline int32_t slots_block_count(int i) const;
  inline void set_slots_block_count(int i, int32_t value);
  inline int32_t slots_padding(int i) const;
  inline void set_slots_padding(int i, int32_t value);

  void InitializeSlot(int slot_index, int start_pos, int end_pos);
  void ResetBlockCount(int slot_index);

  // Computes the size for a CoverageInfo instance of a given length.
  static constexpr int SizeFor(int slot_count);
  inline int AllocatedSize() const;

  // Print debug info.
  void CoverageInfoPrint(std::ostream& os,
                         std::unique_ptr<char[]> function_name = nullptr);

  DECL_VERIFIER(CoverageInfo)

  class BodyDescriptor;  // GC visitor.

  // Description of layout within each slot.
  using Slot = CoverageInfoSlot;

  // Back-compat offset/size constants.
  static const int kSlotCountOffset;

  int32_t slot_count_;
  FLEXIBLE_ARRAY_MEMBER(CoverageInfoSlot, slots);
} V8_OBJECT_END;

inline constexpr int CoverageInfo::kSlotCountOffset =
    offsetof(CoverageInfo, slot_count_);

constexpr int CoverageInfo::SizeFor(int slot_count) {
  return OBJECT_POINTER_ALIGN(OFFSET_OF_DATA_START(CoverageInfo) +
                              slot_count * sizeof(CoverageInfoSlot));
}

// Holds breakpoint related information. This object is used by inspector.
V8_OBJECT class BreakPoint : public Struct {
 public:
  // Accessors
  inline int id() const;
  inline void set_id(int value);

  inline Tagged<String> condition() const;
  inline void set_condition(Tagged<String> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(BreakPoint)
  DECL_VERIFIER(BreakPoint)

  TaggedMember<Smi> id_;
  TaggedMember<String> condition_;
} V8_OBJECT_END;

V8_OBJECT class StackFrameInfo : public Struct {
 public:
  static int GetSourcePosition(DirectHandle<StackFrameInfo> info);

  // The script for the stack frame.
  inline Tagged<Script> script() const;

  // The bytecode offset or source position for the stack frame.
  inline int bytecode_offset_or_source_position() const;
  inline void set_bytecode_offset_or_source_position(int value);

  // Indicates that the frame corresponds to a 'new' invocation.
  inline bool is_constructor() const;
  inline void set_is_constructor(bool value);

  inline Tagged<UnionOf<SharedFunctionInfo, Script>> shared_or_script() const;
  inline void set_shared_or_script(
      Tagged<UnionOf<SharedFunctionInfo, Script>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> function_name() const;
  inline void set_function_name(Tagged<String> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_STACK_FRAME_INFO_FLAGS()

  using BodyDescriptor = StructBodyDescriptor;

  DECL_VERIFIER(StackFrameInfo)
  DECL_PRINTER(StackFrameInfo)

  TaggedMember<UnionOf<SharedFunctionInfo, Script>> shared_or_script_;
  TaggedMember<String> function_name_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

V8_OBJECT class StackTraceInfo : public Struct {
 public:
  // Accessors
  inline int id() const;
  inline void set_id(int value);

  inline Tagged<FixedArray> frames() const;
  inline void set_frames(Tagged<FixedArray> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Access to the stack frames.
  int length() const;
  Tagged<StackFrameInfo> get(int index) const;

  // Dispatched behavior.
  DECL_VERIFIER(StackTraceInfo)
  DECL_PRINTER(StackTraceInfo)

  using BodyDescriptor = StructBodyDescriptor;

  TaggedMember<Smi> id_;
  TaggedMember<FixedArray> frames_;
} V8_OBJECT_END;

V8_OBJECT class ErrorStackData : public Struct {
 public:
  inline bool HasFormattedStack() const;
  inline Tagged<JSAny> formatted_stack() const;
  inline void set_formatted_stack(Tagged<JSAny> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool HasRawDataForCallSiteInfos() const;
  inline Tagged<FixedArray> raw_data_for_call_site_infos() const;
  inline void set_raw_data_for_call_site_infos(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<FixedArray, JSAny>>
  raw_data_for_call_site_infos_or_formatted_stack() const;
  inline void set_raw_data_for_call_site_infos_or_formatted_stack(
      Tagged<UnionOf<FixedArray, JSAny>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<StackTraceInfo> stack_trace() const;
  inline void set_stack_trace(Tagged<StackTraceInfo> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_VERIFIER(ErrorStackData)
  DECL_PRINTER(ErrorStackData)

  using BodyDescriptor = StructBodyDescriptor;

  TaggedMember<UnionOf<FixedArray, JSAny>>
      raw_data_for_call_site_infos_or_formatted_stack_;
  TaggedMember<StackTraceInfo> stack_trace_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_H_
