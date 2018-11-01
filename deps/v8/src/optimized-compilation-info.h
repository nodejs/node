// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OPTIMIZED_COMPILATION_INFO_H_
#define V8_OPTIMIZED_COMPILATION_INFO_H_

#include <memory>

#include "src/bailout-reason.h"
#include "src/code-reference.h"
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
class JSGlobalObject;
class ParseInfo;
class SourceRangeMap;
class Zone;

// OptimizedCompilationInfo encapsulates the information needed to compile
// optimized code for a given function, and the results of the optimized
// compilation.
class V8_EXPORT_PRIVATE OptimizedCompilationInfo final {
 public:
  // Various configuration flags for a compilation, as well as some properties
  // of the compiled code produced by a compilation.
  enum Flag {
    kAccessorInliningEnabled = 1 << 0,
    kFunctionContextSpecializing = 1 << 1,
    kInliningEnabled = 1 << 2,
    kDisableFutureOptimization = 1 << 3,
    kSplittingEnabled = 1 << 4,
    kSourcePositionsEnabled = 1 << 5,
    kBailoutOnUninitialized = 1 << 6,
    kLoopPeelingEnabled = 1 << 7,
    kUntrustedCodeMitigations = 1 << 8,
    kSwitchJumpTableEnabled = 1 << 9,
    kCalledWithCodeStartRegister = 1 << 10,
    kPoisonRegisterArguments = 1 << 11,
    kAllocationFoldingEnabled = 1 << 12,
    kAnalyzeEnvironmentLiveness = 1 << 13,
    kTraceTurboJson = 1 << 14,
    kTraceTurboGraph = 1 << 15,
    kTraceTurboScheduled = 1 << 16,
    kWasmRuntimeExceptionSupport = 1 << 17
  };

  // Construct a compilation info for optimized compilation.
  OptimizedCompilationInfo(Zone* zone, Isolate* isolate,
                           Handle<SharedFunctionInfo> shared,
                           Handle<JSFunction> closure);
  // Construct a compilation info for stub compilation, Wasm, and testing.
  OptimizedCompilationInfo(Vector<const char> debug_name, Zone* zone,
                           Code::Kind code_kind);

  ~OptimizedCompilationInfo();

  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_offset_.IsNone(); }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool has_shared_info() const { return !shared_info().is_null(); }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<Code> code() const { return code_.as_js_code(); }

  wasm::WasmCode* wasm_code() const {
    return const_cast<wasm::WasmCode*>(code_.as_wasm_code());
  }
  Code::Kind code_kind() const { return code_kind_; }
  uint32_t stub_key() const { return stub_key_; }
  void set_stub_key(uint32_t stub_key) { stub_key_ = stub_key; }
  int32_t builtin_index() const { return builtin_index_; }
  void set_builtin_index(int32_t index) { builtin_index_ = index; }
  BailoutId osr_offset() const { return osr_offset_; }
  JavaScriptFrame* osr_frame() const { return osr_frame_; }

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

  void SetPoisoningMitigationLevel(PoisoningMitigationLevel poisoning_level) {
    poisoning_level_ = poisoning_level;
  }
  PoisoningMitigationLevel GetPoisoningMitigationLevel() const {
    return poisoning_level_;
  }

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

  bool switch_jump_table_enabled() const {
    return GetFlag(kSwitchJumpTableEnabled);
  }

  bool called_with_code_start_register() const {
    bool enabled = GetFlag(kCalledWithCodeStartRegister);
    return enabled;
  }

  void MarkAsPoisoningRegisterArguments() {
    DCHECK(has_untrusted_code_mitigations());
    SetFlag(kPoisonRegisterArguments);
  }
  bool is_poisoning_register_arguments() const {
    bool enabled = GetFlag(kPoisonRegisterArguments);
    DCHECK_IMPLIES(enabled, has_untrusted_code_mitigations());
    DCHECK_IMPLIES(enabled, called_with_code_start_register());
    return enabled;
  }

  void MarkAsAllocationFoldingEnabled() { SetFlag(kAllocationFoldingEnabled); }
  bool is_allocation_folding_enabled() const {
    return GetFlag(kAllocationFoldingEnabled);
  }

  void MarkAsAnalyzeEnvironmentLiveness() {
    SetFlag(kAnalyzeEnvironmentLiveness);
  }
  bool is_analyze_environment_liveness() const {
    return GetFlag(kAnalyzeEnvironmentLiveness);
  }

  void SetWasmRuntimeExceptionSupport() {
    SetFlag(kWasmRuntimeExceptionSupport);
  }

  bool wasm_runtime_exception_support() {
    return GetFlag(kWasmRuntimeExceptionSupport);
  }

  bool trace_turbo_json_enabled() const { return GetFlag(kTraceTurboJson); }

  bool trace_turbo_graph_enabled() const { return GetFlag(kTraceTurboGraph); }

  bool trace_turbo_scheduled_enabled() const {
    return GetFlag(kTraceTurboScheduled);
  }

  // Code getters and setters.

  template <typename T>
  void SetCode(T code) {
    code_ = CodeReference(code);
  }

  bool has_context() const;
  Context* context() const;

  bool has_native_context() const;
  Context* native_context() const;

  bool has_global_object() const;
  JSGlobalObject* global_object() const;

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return code_kind() == Code::OPTIMIZED_FUNCTION; }
  bool IsWasm() const { return code_kind() == Code::WASM_FUNCTION; }
  bool IsStub() const {
    return code_kind() != Code::OPTIMIZED_FUNCTION &&
           code_kind() != Code::WASM_FUNCTION;
  }
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

  void ReopenHandlesInNewHandleScope(Isolate* isolate);

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

  int optimization_id() const {
    DCHECK(IsOptimizing());
    return optimization_id_;
  }

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

  const char* trace_turbo_filename() const {
    return trace_turbo_filename_.get();
  }

  void set_trace_turbo_filename(std::unique_ptr<char[]> filename) {
    trace_turbo_filename_ = std::move(filename);
  }

 private:
  OptimizedCompilationInfo(Code::Kind code_kind, Zone* zone);
  void ConfigureFlags();

  void SetFlag(Flag flag) { flags_ |= flag; }
  bool GetFlag(Flag flag) const { return (flags_ & flag) != 0; }

  void SetTracingFlags(bool passes_filter);

  // Compilation flags.
  unsigned flags_ = 0;
  PoisoningMitigationLevel poisoning_level_ =
      PoisoningMitigationLevel::kDontPoison;

  Code::Kind code_kind_;
  uint32_t stub_key_ = 0;
  int32_t builtin_index_ = -1;

  Handle<SharedFunctionInfo> shared_info_;

  Handle<JSFunction> closure_;

  // The compiled code.
  CodeReference code_;

  // Entry point when compiling for OSR, {BailoutId::None} otherwise.
  BailoutId osr_offset_ = BailoutId::None();

  // The zone from which the compilation pipeline working on this
  // OptimizedCompilationInfo allocates.
  Zone* zone_;

  std::shared_ptr<DeferredHandles> deferred_handles_;

  BailoutReason bailout_reason_ = BailoutReason::kNoReason;

  InlinedFunctionList inlined_functions_;

  int optimization_id_ = -1;

  // The current OSR frame for specialization or {nullptr}.
  JavaScriptFrame* osr_frame_ = nullptr;

  Vector<const char> debug_name_;
  std::unique_ptr<char[]> trace_turbo_filename_;

  DISALLOW_COPY_AND_ASSIGN(OptimizedCompilationInfo);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OPTIMIZED_COMPILATION_INFO_H_
