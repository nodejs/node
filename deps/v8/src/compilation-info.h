// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILATION_INFO_H_
#define V8_COMPILATION_INFO_H_

#include <memory>

#include "src/compilation-dependencies.h"
#include "src/feedback-vector.h"
#include "src/frames.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/source-position-table.h"
#include "src/utils.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

class CoverageInfo;
class DeclarationScope;
class DeferredHandles;
class FunctionLiteral;
class Isolate;
class JavaScriptFrame;
class ParseInfo;
class SourceRangeMap;
class Zone;

// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
// TODO(rmcilroy): Split CompilationInfo into two classes, one for unoptimized
// compilation and one for optimized compilation, since they don't share much.
class V8_EXPORT_PRIVATE CompilationInfo final {
 public:
  // Various configuration flags for a compilation, as well as some properties
  // of the compiled code produced by a compilation.
  enum Flag {
    kIsEval = 1 << 0,
    kIsNative = 1 << 1,
    kCollectTypeProfile = 1 << 2,
    kAccessorInliningEnabled = 1 << 3,
    kFunctionContextSpecializing = 1 << 4,
    kInliningEnabled = 1 << 5,
    kDisableFutureOptimization = 1 << 6,
    kSplittingEnabled = 1 << 7,
    kSourcePositionsEnabled = 1 << 8,
    kBailoutOnUninitialized = 1 << 9,
    kLoopPeelingEnabled = 1 << 10,
    kUntrustedCodeMitigations = 1 << 11,
  };

  // TODO(mtrofin): investigate if this might be generalized outside wasm, with
  // the goal of better separating the compiler from where compilation lands. At
  // that point, the Handle<Code> member of CompilationInfo would also be
  // removed.
  struct WasmCodeDesc {
    CodeDesc code_desc;
    size_t safepoint_table_offset = 0;
    uint32_t frame_slot_count = 0;
    Handle<ByteArray> source_positions_table;
    MaybeHandle<HandlerTable> handler_table;
  };

  // Construct a compilation info for unoptimized compilation.
  CompilationInfo(Zone* zone, ParseInfo* parse_info, FunctionLiteral* literal);
  // Construct a compilation info for optimized compilation.
  CompilationInfo(Zone* zone, Isolate* isolate,
                  Handle<SharedFunctionInfo> shared,
                  Handle<JSFunction> closure);
  // Construct a compilation info for stub compilation (or testing).
  CompilationInfo(Vector<const char> debug_name, Zone* zone,
                  Code::Kind code_kind);
  ~CompilationInfo();

  FunctionLiteral* literal() const { return literal_; }
  void set_literal(FunctionLiteral* literal) {
    DCHECK_NOT_NULL(literal);
    literal_ = literal;
  }

  bool has_source_range_map() const { return source_range_map_ != nullptr; }
  SourceRangeMap* source_range_map() const { return source_range_map_; }
  void set_source_range_map(SourceRangeMap* source_range_map) {
    source_range_map_ = source_range_map;
  }

  DeclarationScope* scope() const;

  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_offset_.IsNone(); }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  void set_shared_info(Handle<SharedFunctionInfo> shared_info) {
    shared_info_ = shared_info;
  }
  bool has_shared_info() const { return !shared_info().is_null(); }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<Code> code() const { return code_; }
  Code::Kind code_kind() const { return code_kind_; }
  uint32_t stub_key() const { return stub_key_; }
  void set_stub_key(uint32_t stub_key) { stub_key_ = stub_key; }
  int32_t builtin_index() const { return builtin_index_; }
  void set_builtin_index(int32_t index) { builtin_index_ = index; }
  BailoutId osr_offset() const { return osr_offset_; }
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

  bool has_asm_wasm_data() const { return !asm_wasm_data_.is_null(); }
  Handle<FixedArray> asm_wasm_data() const { return asm_wasm_data_; }

  // Flags used by unoptimized compilation.

  void MarkAsEval() { SetFlag(kIsEval); }
  bool is_eval() const { return GetFlag(kIsEval); }

  void MarkAsNative() { SetFlag(kIsNative); }
  bool is_native() const { return GetFlag(kIsNative); }

  void MarkAsCollectTypeProfile() { SetFlag(kCollectTypeProfile); }
  bool collect_type_profile() const { return GetFlag(kCollectTypeProfile); }

  // Flags used by optimized compilation.

  void MarkAsFunctionContextSpecializing() {
    SetFlag(kFunctionContextSpecializing);
  }
  bool is_function_context_specializing() const {
    return GetFlag(kFunctionContextSpecializing);
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

  void MarkAsLoopPeelingEnabled() { SetFlag(kLoopPeelingEnabled); }
  bool is_loop_peeling_enabled() const { return GetFlag(kLoopPeelingEnabled); }

  bool has_untrusted_code_mitigations() const {
    return GetFlag(kUntrustedCodeMitigations);
  }

  // Code getters and setters.

  void SetCode(Handle<Code> code) { code_ = code; }

  void SetBytecodeArray(Handle<BytecodeArray> bytecode_array) {
    bytecode_array_ = bytecode_array;
  }

  void SetAsmWasmData(Handle<FixedArray> asm_wasm_data) {
    asm_wasm_data_ = asm_wasm_data;
  }

  FeedbackVectorSpec* feedback_vector_spec() { return &feedback_vector_spec_; }

  bool has_context() const;
  Context* context() const;

  bool has_native_context() const;
  Context* native_context() const;

  bool has_global_object() const;
  JSGlobalObject* global_object() const;

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return mode_ == OPTIMIZE; }
  bool IsStub() const { return mode_ == STUB; }
  bool IsWasm() const { return code_kind() == Code::WASM_FUNCTION; }
  void SetOptimizingForOsr(BailoutId osr_offset, JavaScriptFrame* osr_frame) {
    DCHECK(IsOptimizing());
    osr_offset_ = osr_offset;
    osr_frame_ = osr_frame;
  }

  void set_deferred_handles(std::shared_ptr<DeferredHandles> deferred_handles);
  void set_deferred_handles(DeferredHandles* deferred_handles);
  std::shared_ptr<DeferredHandles> deferred_handles() {
    return deferred_handles_;
  }

  void ReopenHandlesInNewHandleScope();

  void AbortOptimization(BailoutReason reason) {
    DCHECK_NE(reason, BailoutReason::kNoReason);
    if (bailout_reason_ == BailoutReason::kNoReason) bailout_reason_ = reason;
    SetFlag(kDisableFutureOptimization);
  }

  void RetryOptimization(BailoutReason reason) {
    DCHECK_NE(reason, BailoutReason::kNoReason);
    if (GetFlag(kDisableFutureOptimization)) return;
    bailout_reason_ = reason;
  }

  BailoutReason bailout_reason() const { return bailout_reason_; }

  CompilationDependencies* dependencies() { return dependencies_.get(); }

  int optimization_id() const {
    DCHECK(IsOptimizing());
    return optimization_id_;
  }

  bool has_simple_parameters();

  struct InlinedFunctionHolder {
    Handle<SharedFunctionInfo> shared_info;

    InliningPosition position;

    InlinedFunctionHolder(Handle<SharedFunctionInfo> inlined_shared_info,
                          SourcePosition pos)
        : shared_info(inlined_shared_info) {
      position.position = pos;
      // initialized when generating the deoptimization literals
      position.inlined_function_id = DeoptimizationData::kNotInlinedIndex;
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

  StackFrame::Type GetOutputStackFrameType() const;

  int GetDeclareGlobalsFlags() const;

  SourcePositionTableBuilder::RecordingMode SourcePositionRecordingMode() const;

  bool has_coverage_info() const { return !coverage_info_.is_null(); }
  Handle<CoverageInfo> coverage_info() const { return coverage_info_; }
  void set_coverage_info(Handle<CoverageInfo> coverage_info) {
    coverage_info_ = coverage_info;
  }

  WasmCodeDesc* wasm_code_desc() { return &wasm_code_desc_; }

 private:
  // Compilation mode.
  // BASE is generated by the full codegen, optionally prepared for bailouts.
  // OPTIMIZE is optimized code generated by the Hydrogen-based backend.
  enum Mode { BASE, OPTIMIZE, STUB };

  CompilationInfo(Vector<const char> debug_name, Code::Kind code_kind,
                  Mode mode, Zone* zone);

  void SetMode(Mode mode) { mode_ = mode; }

  void SetFlag(Flag flag) { flags_ |= flag; }

  void SetFlag(Flag flag, bool value) {
    flags_ = value ? flags_ | flag : flags_ & ~flag;
  }

  bool GetFlag(Flag flag) const { return (flags_ & flag) != 0; }

  FunctionLiteral* literal_;
  SourceRangeMap* source_range_map_;  // Used when block coverage is enabled.

  unsigned flags_;

  Code::Kind code_kind_;
  uint32_t stub_key_;
  int32_t builtin_index_;

  Handle<SharedFunctionInfo> shared_info_;

  Handle<JSFunction> closure_;

  // The compiled code.
  Handle<Code> code_;
  WasmCodeDesc wasm_code_desc_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  BailoutId osr_offset_;

  // Holds the bytecode array generated by the interpreter.
  // TODO(rmcilroy/mstarzinger): Temporary work-around until compiler.cc is
  // refactored to avoid us needing to carry the BytcodeArray around.
  Handle<BytecodeArray> bytecode_array_;

  // Holds the asm_wasm array generated by the asmjs compiler.
  Handle<FixedArray> asm_wasm_data_;

  // Holds the feedback vector spec generated during compilation
  FeedbackVectorSpec feedback_vector_spec_;

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  std::shared_ptr<DeferredHandles> deferred_handles_;

  // Dependencies for this compilation, e.g. stable maps.
  std::unique_ptr<CompilationDependencies> dependencies_;

  BailoutReason bailout_reason_;

  InlinedFunctionList inlined_functions_;

  // Number of parameters used for compilation of stubs that require arguments.
  int parameter_count_;

  int optimization_id_;

  // The current OSR frame for specialization or {nullptr}.
  JavaScriptFrame* osr_frame_ = nullptr;

  Vector<const char> debug_name_;

  // Encapsulates coverage information gathered by the bytecode generator.
  // Needs to be stored on the shared function info once compilation completes.
  Handle<CoverageInfo> coverage_info_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILATION_INFO_H_
