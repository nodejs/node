// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_DEOPTIMIZER_H_
#define V8_DEOPTIMIZER_DEOPTIMIZER_H_

#include <vector>

#include "src/builtins/builtins.h"
#include "src/codegen/source-position.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/deoptimizer/frame-description.h"
#include "src/deoptimizer/translated-state.h"
#include "src/diagnostics/code-tracer.h"
#include "src/objects/js-function.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

enum class BuiltinContinuationMode;

class DeoptimizedFrameInfo;
class Isolate;

class Deoptimizer : public Malloced {
 public:
  struct DeoptInfo {
    DeoptInfo(SourcePosition position, DeoptimizeReason deopt_reason,
              int deopt_id)
        : position(position), deopt_reason(deopt_reason), deopt_id(deopt_id) {}

    const SourcePosition position;
    const DeoptimizeReason deopt_reason;
    const int deopt_id;
  };

  static DeoptInfo GetDeoptInfo(Code code, Address from);

  static int ComputeSourcePositionFromBytecodeArray(
      Isolate* isolate, SharedFunctionInfo shared,
      BytecodeOffset bytecode_offset);

  static const char* MessageFor(DeoptimizeKind kind, bool reuse_code);

  Handle<JSFunction> function() const;
  Handle<Code> compiled_code() const;
  DeoptimizeKind deopt_kind() const { return deopt_kind_; }

  bool should_reuse_code() const;

  static Deoptimizer* New(Address raw_function, DeoptimizeKind kind,
                          unsigned deopt_exit_index, Address from,
                          int fp_to_sp_delta, Isolate* isolate);
  static Deoptimizer* Grab(Isolate* isolate);

  // The returned object with information on the optimized frame needs to be
  // freed before another one can be generated.
  static DeoptimizedFrameInfo* DebuggerInspectableFrame(JavaScriptFrame* frame,
                                                        int jsframe_index,
                                                        Isolate* isolate);

  // Deoptimize the function now. Its current optimized code will never be run
  // again and any activations of the optimized code will get deoptimized when
  // execution returns. If {code} is specified then the given code is targeted
  // instead of the function code (e.g. OSR code not installed on function).
  static void DeoptimizeFunction(JSFunction function, Code code = Code());

  // Deoptimize all code in the given isolate.
  V8_EXPORT_PRIVATE static void DeoptimizeAll(Isolate* isolate);

  // Deoptimizes all optimized code that has been previously marked
  // (via code->set_marked_for_deoptimization) and unlinks all functions that
  // refer to that code.
  static void DeoptimizeMarkedCode(Isolate* isolate);

  // Check the given address against a list of allowed addresses, to prevent a
  // potential attacker from using the frame creation process in the
  // deoptimizer, in particular the signing process, to gain control over the
  // program.
  // When building mksnapshot, always return false.
  static bool IsValidReturnAddress(Address address);

  ~Deoptimizer();

  void MaterializeHeapObjects();

  static void ComputeOutputFrames(Deoptimizer* deoptimizer);

  // Returns the builtin that will perform a check and either eagerly deopt with
  // |reason| or resume execution in the optimized code.
  V8_EXPORT_PRIVATE static Builtin GetDeoptWithResumeBuiltin(
      DeoptimizeReason reason);

  V8_EXPORT_PRIVATE static Builtin GetDeoptimizationEntry(DeoptimizeKind kind);

  // Returns true if {addr} is a deoptimization entry and stores its type in
  // {type_out}. Returns false if {addr} is not a deoptimization entry.
  static bool IsDeoptimizationEntry(Isolate* isolate, Address addr,
                                    DeoptimizeKind* type_out);

  // Code generation support.
  static int input_offset() { return offsetof(Deoptimizer, input_); }
  static int output_count_offset() {
    return offsetof(Deoptimizer, output_count_);
  }
  static int output_offset() { return offsetof(Deoptimizer, output_); }

  static int caller_frame_top_offset() {
    return offsetof(Deoptimizer, caller_frame_top_);
  }

  V8_EXPORT_PRIVATE static int GetDeoptimizedCodeCount(Isolate* isolate);

  Isolate* isolate() const { return isolate_; }

  static constexpr int kMaxNumberOfEntries = 16384;

  // This marker is passed to Deoptimizer::New as {deopt_exit_index} on
  // platforms that have fixed deopt sizes (see also
  // kSupportsFixedDeoptExitSizes). The actual deoptimization id is then
  // calculated from the return address.
  static constexpr unsigned kFixedExitSizeMarker = kMaxUInt32;

  // Set to true when the architecture supports deoptimization exit sequences
  // of a fixed size, that can be sorted so that the deoptimization index is
  // deduced from the address of the deoptimization exit.
  // TODO(jgruber): Remove this, and support for variable deopt exit sizes,
  // once all architectures use fixed exit sizes.
  V8_EXPORT_PRIVATE static const bool kSupportsFixedDeoptExitSizes;

  // Size of deoptimization exit sequence. This is only meaningful when
  // kSupportsFixedDeoptExitSizes is true.
  V8_EXPORT_PRIVATE static const int kNonLazyDeoptExitSize;
  V8_EXPORT_PRIVATE static const int kLazyDeoptExitSize;
  V8_EXPORT_PRIVATE static const int kEagerWithResumeBeforeArgsSize;
  V8_EXPORT_PRIVATE static const int kEagerWithResumeDeoptExitSize;
  V8_EXPORT_PRIVATE static const int kEagerWithResumeImmedArgs1PcOffset;
  V8_EXPORT_PRIVATE static const int kEagerWithResumeImmedArgs2PcOffset;

  // Tracing.
  static void TraceMarkForDeoptimization(Code code, const char* reason);
  static void TraceEvictFromOptimizedCodeCache(SharedFunctionInfo sfi,
                                               const char* reason);

 private:
  void QueueValueForMaterialization(Address output_address, Object obj,
                                    const TranslatedFrame::iterator& iterator);

  Deoptimizer(Isolate* isolate, JSFunction function, DeoptimizeKind kind,
              unsigned deopt_exit_index, Address from, int fp_to_sp_delta);
  Code FindOptimizedCode();
  void DeleteFrameDescriptions();

  void DoComputeOutputFrames();
  void DoComputeUnoptimizedFrame(TranslatedFrame* translated_frame,
                                 int frame_index, bool goto_catch_handler);
  void DoComputeArgumentsAdaptorFrame(TranslatedFrame* translated_frame,
                                      int frame_index);
  void DoComputeConstructStubFrame(TranslatedFrame* translated_frame,
                                   int frame_index);

  static Builtin TrampolineForBuiltinContinuation(BuiltinContinuationMode mode,
                                                  bool must_handle_result);

#if V8_ENABLE_WEBASSEMBLY
  TranslatedValue TranslatedValueForWasmReturnKind(
      base::Optional<wasm::ValueKind> wasm_call_return_kind);
#endif  // V8_ENABLE_WEBASSEMBLY

  void DoComputeBuiltinContinuation(TranslatedFrame* translated_frame,
                                    int frame_index,
                                    BuiltinContinuationMode mode);

  unsigned ComputeInputFrameAboveFpFixedSize() const;
  unsigned ComputeInputFrameSize() const;

  static unsigned ComputeIncomingArgumentSize(SharedFunctionInfo shared);

  static void MarkAllCodeForContext(NativeContext native_context);
  static void DeoptimizeMarkedCodeForContext(NativeContext native_context);
  // Searches the list of known deoptimizing code for a Code object
  // containing the given address (which is supposedly faster than
  // searching all code objects).
  Code FindDeoptimizingCode(Address addr);

  // Tracing.
  bool tracing_enabled() const { return static_cast<bool>(trace_scope_); }
  bool verbose_tracing_enabled() const {
    return FLAG_trace_deopt_verbose && trace_scope_;
  }
  CodeTracer::Scope* trace_scope() const { return trace_scope_.get(); }
  CodeTracer::Scope* verbose_trace_scope() const {
    return FLAG_trace_deopt_verbose ? trace_scope() : nullptr;
  }
  void TraceDeoptBegin(int optimization_id, BytecodeOffset bytecode_offset);
  void TraceDeoptEnd(double deopt_duration);
#ifdef DEBUG
  static void TraceFoundActivation(Isolate* isolate, JSFunction function);
#endif
  static void TraceDeoptAll(Isolate* isolate);
  static void TraceDeoptMarked(Isolate* isolate);

  Isolate* isolate_;
  JSFunction function_;
  Code compiled_code_;
  unsigned deopt_exit_index_;
  DeoptimizeKind deopt_kind_;
  Address from_;
  int fp_to_sp_delta_;
  bool deoptimizing_throw_;
  int catch_handler_data_;
  int catch_handler_pc_offset_;

  // Input frame description.
  FrameDescription* input_;
  // Number of output frames.
  int output_count_;
  // Array of output frame descriptions.
  FrameDescription** output_;

  // Caller frame details computed from input frame.
  intptr_t caller_frame_top_;
  intptr_t caller_fp_;
  intptr_t caller_pc_;
  intptr_t caller_constant_pool_;

  // The argument count of the bottom most frame.
  int actual_argument_count_;

  // Key for lookup of previously materialized objects.
  intptr_t stack_fp_;

  TranslatedState translated_state_;
  struct ValueToMaterialize {
    Address output_slot_address_;
    TranslatedFrame::iterator value_;
  };
  std::vector<ValueToMaterialize> values_to_materialize_;

#ifdef DEBUG
  DisallowGarbageCollection* disallow_garbage_collection_;
#endif  // DEBUG

  std::unique_ptr<CodeTracer::Scope> trace_scope_;

  friend class DeoptimizedFrameInfo;
  friend class FrameDescription;
  friend class FrameWriter;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_DEOPTIMIZER_H_
