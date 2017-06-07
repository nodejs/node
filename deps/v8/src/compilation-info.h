// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILATION_INFO_H_
#define V8_COMPILATION_INFO_H_

#include <memory>

#include "src/compilation-dependencies.h"
#include "src/frames.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/source-position-table.h"
#include "src/utils.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

class DeclarationScope;
class DeferredHandles;
class FunctionLiteral;
class JavaScriptFrame;
class ParseInfo;
class Isolate;
class Zone;

// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class V8_EXPORT_PRIVATE CompilationInfo final {
 public:
  // Various configuration flags for a compilation, as well as some properties
  // of the compiled code produced by a compilation.
  enum Flag {
    kDeferredCalling = 1 << 0,
    kNonDeferredCalling = 1 << 1,
    kSavesCallerDoubles = 1 << 2,
    kRequiresFrame = 1 << 3,
    kDeoptimizationSupport = 1 << 4,
    kAccessorInliningEnabled = 1 << 5,
    kSerializing = 1 << 6,
    kFunctionContextSpecializing = 1 << 7,
    kFrameSpecializing = 1 << 8,
    kInliningEnabled = 1 << 9,
    kDisableFutureOptimization = 1 << 10,
    kSplittingEnabled = 1 << 11,
    kDeoptimizationEnabled = 1 << 12,
    kSourcePositionsEnabled = 1 << 13,
    kBailoutOnUninitialized = 1 << 14,
    kOptimizeFromBytecode = 1 << 15,
    kLoopPeelingEnabled = 1 << 16,
  };

  CompilationInfo(Zone* zone, ParseInfo* parse_info, Isolate* isolate,
                  Handle<JSFunction> closure);
  CompilationInfo(Vector<const char> debug_name, Isolate* isolate, Zone* zone,
                  Code::Flags code_flags);
  ~CompilationInfo();

  ParseInfo* parse_info() const { return parse_info_; }

  // -----------------------------------------------------------
  // TODO(titzer): inline and delete accessors of ParseInfo
  // -----------------------------------------------------------
  Handle<Script> script() const;
  FunctionLiteral* literal() const;
  DeclarationScope* scope() const;
  Handle<SharedFunctionInfo> shared_info() const;
  bool has_shared_info() const;
  // -----------------------------------------------------------

  Isolate* isolate() const { return isolate_; }
  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_ast_id_.IsNone(); }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<Code> code() const { return code_; }
  Code::Flags code_flags() const { return code_flags_; }
  BailoutId osr_ast_id() const { return osr_ast_id_; }
  JavaScriptFrame* osr_frame() const { return osr_frame_; }
  int num_parameters() const;
  int num_parameters_including_this() const;
  bool is_this_defined() const;

  void set_parameter_count(int parameter_count) {
    DCHECK(IsStub());
    parameter_count_ = parameter_count;
  }

  bool has_bytecode_array() const { return !bytecode_array_.is_null(); }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }

  bool is_calling() const {
    return GetFlag(kDeferredCalling) || GetFlag(kNonDeferredCalling);
  }

  void MarkAsDeferredCalling() { SetFlag(kDeferredCalling); }

  bool is_deferred_calling() const { return GetFlag(kDeferredCalling); }

  void MarkAsNonDeferredCalling() { SetFlag(kNonDeferredCalling); }

  bool is_non_deferred_calling() const { return GetFlag(kNonDeferredCalling); }

  void MarkAsSavesCallerDoubles() { SetFlag(kSavesCallerDoubles); }

  bool saves_caller_doubles() const { return GetFlag(kSavesCallerDoubles); }

  void MarkAsRequiresFrame() { SetFlag(kRequiresFrame); }

  bool requires_frame() const { return GetFlag(kRequiresFrame); }

  // Compiles marked as debug produce unoptimized code with debug break slots.
  // Inner functions that cannot be compiled w/o context are compiled eagerly.
  // Always include deoptimization support to avoid having to recompile again.
  void MarkAsDebug() {
    set_is_debug();
    SetFlag(kDeoptimizationSupport);
  }

  bool is_debug() const;

  void PrepareForSerializing();

  bool will_serialize() const { return GetFlag(kSerializing); }

  void MarkAsFunctionContextSpecializing() {
    SetFlag(kFunctionContextSpecializing);
  }

  bool is_function_context_specializing() const {
    return GetFlag(kFunctionContextSpecializing);
  }

  void MarkAsFrameSpecializing() { SetFlag(kFrameSpecializing); }

  bool is_frame_specializing() const { return GetFlag(kFrameSpecializing); }

  void MarkAsDeoptimizationEnabled() { SetFlag(kDeoptimizationEnabled); }

  bool is_deoptimization_enabled() const {
    return GetFlag(kDeoptimizationEnabled);
  }

  void MarkAsAccessorInliningEnabled() { SetFlag(kAccessorInliningEnabled); }

  bool is_accessor_inlining_enabled() const {
    return GetFlag(kAccessorInliningEnabled);
  }

  void MarkAsSourcePositionsEnabled() { SetFlag(kSourcePositionsEnabled); }

  bool is_source_positions_enabled() const {
    return GetFlag(kSourcePositionsEnabled);
  }

  void MarkAsInliningEnabled() { SetFlag(kInliningEnabled); }

  bool is_inlining_enabled() const { return GetFlag(kInliningEnabled); }

  void MarkAsSplittingEnabled() { SetFlag(kSplittingEnabled); }

  bool is_splitting_enabled() const { return GetFlag(kSplittingEnabled); }

  void MarkAsBailoutOnUninitialized() { SetFlag(kBailoutOnUninitialized); }

  bool is_bailout_on_uninitialized() const {
    return GetFlag(kBailoutOnUninitialized);
  }

  void MarkAsOptimizeFromBytecode() { SetFlag(kOptimizeFromBytecode); }

  bool is_optimizing_from_bytecode() const {
    return GetFlag(kOptimizeFromBytecode);
  }

  void MarkAsLoopPeelingEnabled() { SetFlag(kLoopPeelingEnabled); }

  bool is_loop_peeling_enabled() const { return GetFlag(kLoopPeelingEnabled); }

  bool GeneratePreagedPrologue() const {
    // Generate a pre-aged prologue if we are optimizing for size, which
    // will make code flushing more aggressive. Only apply to Code::FUNCTION,
    // since StaticMarkingVisitor::IsFlushable only flushes proper functions.
    return FLAG_optimize_for_size && FLAG_age_code && !is_debug() &&
           output_code_kind() == Code::FUNCTION;
  }

  void SetCode(Handle<Code> code) { code_ = code; }

  void SetBytecodeArray(Handle<BytecodeArray> bytecode_array) {
    bytecode_array_ = bytecode_array;
  }

  bool ShouldTrapOnDeopt() const {
    return (FLAG_trap_on_deopt && IsOptimizing()) ||
           (FLAG_trap_on_stub_deopt && IsStub());
  }

  bool has_context() const;
  Context* context() const;

  bool has_native_context() const;
  Context* native_context() const;

  bool has_global_object() const;
  JSGlobalObject* global_object() const;

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return mode_ == OPTIMIZE; }
  bool IsStub() const { return mode_ == STUB; }
  bool IsWasm() const { return output_code_kind() == Code::WASM_FUNCTION; }
  void SetOptimizing();
  void SetOptimizingForOsr(BailoutId osr_ast_id, JavaScriptFrame* osr_frame) {
    SetOptimizing();
    osr_ast_id_ = osr_ast_id;
    osr_frame_ = osr_frame;
  }

  // Deoptimization support.
  bool HasDeoptimizationSupport() const {
    return GetFlag(kDeoptimizationSupport);
  }
  void EnableDeoptimizationSupport() {
    DCHECK_EQ(BASE, mode_);
    SetFlag(kDeoptimizationSupport);
  }
  bool ShouldEnsureSpaceForLazyDeopt() { return !IsStub(); }

  bool ExpectsJSReceiverAsReceiver();

  // Determines whether or not to insert a self-optimization header.
  bool ShouldSelfOptimize();

  void set_deferred_handles(std::shared_ptr<DeferredHandles> deferred_handles);
  void set_deferred_handles(DeferredHandles* deferred_handles);
  std::shared_ptr<DeferredHandles> deferred_handles() {
    return deferred_handles_;
  }

  void ReopenHandlesInNewHandleScope();

  void AbortOptimization(BailoutReason reason) {
    DCHECK(reason != kNoReason);
    if (bailout_reason_ == kNoReason) bailout_reason_ = reason;
    SetFlag(kDisableFutureOptimization);
  }

  void RetryOptimization(BailoutReason reason) {
    DCHECK(reason != kNoReason);
    if (GetFlag(kDisableFutureOptimization)) return;
    bailout_reason_ = reason;
  }

  BailoutReason bailout_reason() const { return bailout_reason_; }

  int prologue_offset() const {
    DCHECK_NE(Code::kPrologueOffsetNotSet, prologue_offset_);
    return prologue_offset_;
  }

  void set_prologue_offset(int prologue_offset) {
    DCHECK_EQ(Code::kPrologueOffsetNotSet, prologue_offset_);
    prologue_offset_ = prologue_offset;
  }

  CompilationDependencies* dependencies() { return &dependencies_; }

  int optimization_id() const { return optimization_id_; }

  int osr_expr_stack_height() { return osr_expr_stack_height_; }
  void set_osr_expr_stack_height(int height) {
    DCHECK(height >= 0);
    osr_expr_stack_height_ = height;
  }

  bool has_simple_parameters();

  struct InlinedFunctionHolder {
    Handle<SharedFunctionInfo> shared_info;

    // Root that holds the unoptimized code of the inlined function alive
    // (and out of reach of code flushing) until we finish compilation.
    // Do not remove.
    Handle<Code> inlined_code_object_root;

    InliningPosition position;

    InlinedFunctionHolder(Handle<SharedFunctionInfo> inlined_shared_info,
                          Handle<Code> inlined_code_object_root,
                          SourcePosition pos)
        : shared_info(inlined_shared_info),
          inlined_code_object_root(inlined_code_object_root) {
      position.position = pos;
      // initialized when generating the deoptimization literals
      position.inlined_function_id = DeoptimizationInputData::kNotInlinedIndex;
    }

    void RegisterInlinedFunctionId(size_t inlined_function_id) {
      position.inlined_function_id = static_cast<int>(inlined_function_id);
    }
  };

  typedef std::vector<InlinedFunctionHolder> InlinedFunctionList;
  InlinedFunctionList& inlined_functions() { return inlined_functions_; }

  // Returns the inlining id for source position tracking.
  int AddInlinedFunction(Handle<SharedFunctionInfo> inlined_function,
                         SourcePosition pos);

  std::unique_ptr<char[]> GetDebugName() const;

  Code::Kind output_code_kind() const;

  StackFrame::Type GetOutputStackFrameType() const;

  int GetDeclareGlobalsFlags() const;

  SourcePositionTableBuilder::RecordingMode SourcePositionRecordingMode() const;

 private:
  // Compilation mode.
  // BASE is generated by the full codegen, optionally prepared for bailouts.
  // OPTIMIZE is optimized code generated by the Hydrogen-based backend.
  enum Mode { BASE, OPTIMIZE, STUB };

  CompilationInfo(ParseInfo* parse_info, Vector<const char> debug_name,
                  Code::Flags code_flags, Mode mode, Isolate* isolate,
                  Zone* zone);

  ParseInfo* parse_info_;
  Isolate* isolate_;

  void SetMode(Mode mode) { mode_ = mode; }

  void SetFlag(Flag flag) { flags_ |= flag; }

  void SetFlag(Flag flag, bool value) {
    flags_ = value ? flags_ | flag : flags_ & ~flag;
  }

  bool GetFlag(Flag flag) const { return (flags_ & flag) != 0; }

  void set_is_debug();

  unsigned flags_;

  Code::Flags code_flags_;

  Handle<JSFunction> closure_;

  // The compiled code.
  Handle<Code> code_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  BailoutId osr_ast_id_;

  // Holds the bytecode array generated by the interpreter.
  // TODO(rmcilroy/mstarzinger): Temporary work-around until compiler.cc is
  // refactored to avoid us needing to carry the BytcodeArray around.
  Handle<BytecodeArray> bytecode_array_;

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  std::shared_ptr<DeferredHandles> deferred_handles_;

  // Dependencies for this compilation, e.g. stable maps.
  CompilationDependencies dependencies_;

  BailoutReason bailout_reason_;

  int prologue_offset_;

  InlinedFunctionList inlined_functions_;

  // Number of parameters used for compilation of stubs that require arguments.
  int parameter_count_;

  int optimization_id_;

  int osr_expr_stack_height_;

  // The current OSR frame for specialization or {nullptr}.
  JavaScriptFrame* osr_frame_ = nullptr;

  Vector<const char> debug_name_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILATION_INFO_H_
