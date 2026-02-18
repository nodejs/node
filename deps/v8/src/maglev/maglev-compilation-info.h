// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_COMPILATION_INFO_H_
#define V8_MAGLEV_MAGLEV_COMPILATION_INFO_H_

#include <memory>
#include <optional>

#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/utils/utils.h"
#include "src/zone/zone.h"

namespace v8 {

namespace base {
class DefaultAllocationPolicy;
}

namespace internal {

class Isolate;
class PersistentHandles;
class SharedFunctionInfo;
class TranslationArrayBuilder;

namespace compiler {
class JSHeapBroker;
}

namespace maglev {

class MaglevCompilationUnit;
class MaglevGraphLabeller;
class MaglevCodeGenerator;

inline bool FlagsMightEnableMaglevTracing() {
  return v8_flags.code_comments || v8_flags.print_maglev_code ||
         v8_flags.print_maglev_graph || v8_flags.print_maglev_graphs ||
         v8_flags.trace_maglev_escape_analysis ||
         v8_flags.trace_maglev_graph_building ||
         v8_flags.trace_maglev_inlining || v8_flags.trace_turbo_inlining ||
         v8_flags.trace_maglev_object_tracking ||
         v8_flags.trace_maglev_phi_untagging ||
         v8_flags.trace_maglev_regalloc || v8_flags.trace_maglev_truncation;
}

struct CompilationFlags {
  // If the feedback suggests that the result of an addition will be a safe
  // integer (where Float64 can represent integers exactly), then we can
  // speculate its range. During the truncation pass:
  //
  // 1. If Float64SpeculateSafeAdd CANNOT be truncated to Int32 (i.e., its
  //    result is used as a Float64 or Tagged value), it's lowered to a
  //    standard Float64Add.
  // 2. If it CAN be truncated to Int32, we check if we should speculate:
  //    - If the result is already known to be a safe integer (via range
  //      analysis), we lower it to Int32Add (non-speculative).
  //    - If we decide to speculate (e.g., at least one input is already a safe
  //      integer or a Phi), we insert speculative truncations for the inputs
  //      (which may eager deopt if an input isn't a safe integer) and
  //      lower to Int32Add.
  //    - Otherwise, we fall back to Float64Add.
  const bool can_speculative_additive_safe_int;

  const bool trace_inlining;
  const bool is_non_eager_inlining_enabled;
  const bool is_inline_api_calls_enabled;
  const int max_eager_inlined_bytecode;
  const int max_inlined_bytecode_size;
  const int max_inlined_bytecode_size_small;
  const int max_inlined_bytecode_size_small_with_heapnum_in_out;
  const int max_inlined_bytecode_size_cumulative;
  const int max_inlined_bytecode_size_small_total;
  const int max_inline_depth;
  const int max_inline_depth_small;
  const double min_inlining_frequency;

  static CompilationFlags ForMaglev() {
    return {
        /* can_speculative_additive_safe_int */ false,
        v8_flags.trace_maglev_inlining,
        v8_flags.maglev_non_eager_inlining,
        v8_flags.maglev_inline_api_calls,
        v8_flags.max_maglev_eager_inlined_bytecode_size,
        v8_flags.max_maglev_inlined_bytecode_size,
        v8_flags.max_maglev_inlined_bytecode_size_small,
        v8_flags.max_maglev_inlined_bytecode_size_small_with_heapnum_in_out,
        v8_flags.max_maglev_inlined_bytecode_size_cumulative,
        v8_flags.max_maglev_inlined_bytecode_size_small_total,
        v8_flags.max_maglev_inline_depth,
        v8_flags.max_maglev_hard_inline_depth,
        v8_flags.min_maglev_inlining_frequency,
    };
  }

  static CompilationFlags ForTurbolev() {
    return {
        /* can_speculative_additive_safe_int */ Is64() &&
            v8_flags.turbolev_additive_safe_int_feedback &&
            v8_flags.turbolev_non_eager_inlining,
        v8_flags.trace_turbo_inlining,
        v8_flags.turbolev_non_eager_inlining,
        // TODO(victorgomes): Inline API calls are still not supported by
        // Turbolev.
        /* is_inline_api_calls_enabled */ false,
        v8_flags.max_turbolev_eager_inlined_bytecode_size,
        v8_flags.max_inlined_bytecode_size,
        v8_flags.max_inlined_bytecode_size_small,
        v8_flags.max_inlined_bytecode_size_small_with_heapnum_in_out,
        v8_flags.max_inlined_bytecode_size_cumulative,
        v8_flags.max_inlined_bytecode_size_small_total,
        v8_flags.max_turbolev_inline_depth,
        v8_flags.max_turbolev_inline_depth,
        v8_flags.min_inlining_frequency,
    };
  }
};

class MaglevCompilationInfo final {
 public:
  static std::unique_ptr<MaglevCompilationInfo> NewForTurbolev(
      Isolate* isolate, compiler::JSHeapBroker* broker,
      IndirectHandle<JSFunction> function, BytecodeOffset osr_offset,
      bool specialize_to_function_context) {
    // Doesn't use make_unique due to the private ctor.
    return std::unique_ptr<MaglevCompilationInfo>(new MaglevCompilationInfo(
        isolate, function, osr_offset, broker, specialize_to_function_context,
        /*is_turbolev*/ true));
  }
  static std::unique_ptr<MaglevCompilationInfo> New(
      Isolate* isolate, IndirectHandle<JSFunction> function,
      BytecodeOffset osr_offset) {
    // Doesn't use make_unique due to the private ctor.
    return std::unique_ptr<MaglevCompilationInfo>(
        new MaglevCompilationInfo(isolate, function, osr_offset));
  }
  V8_EXPORT_PRIVATE ~MaglevCompilationInfo();

  Zone* zone() { return &zone_; }
  compiler::JSHeapBroker* broker() const { return broker_; }
  MaglevCompilationUnit* toplevel_compilation_unit() const {
    return toplevel_compilation_unit_;
  }
  IndirectHandle<JSFunction> toplevel_function() const {
    return toplevel_function_;
  }
  BytecodeOffset toplevel_osr_offset() const { return osr_offset_; }
  bool toplevel_is_osr() const { return osr_offset_ != BytecodeOffset::None(); }
  void set_code(IndirectHandle<Code> code) {
    DCHECK(code_.is_null());
    code_ = code;
  }
  MaybeIndirectHandle<Code> get_code() { return code_; }

  bool is_turbolev() const { return is_turbolev_; }
  bool is_tracing_enabled() const { return is_tracing_enabled_; }

  bool has_graph_labeller() const { return !!graph_labeller_; }
  void set_graph_labeller(MaglevGraphLabeller* graph_labeller);
  MaglevGraphLabeller* graph_labeller() const {
    DCHECK(has_graph_labeller());
    return graph_labeller_.get();
  }

#ifdef V8_ENABLE_MAGLEV
  void set_code_generator(std::unique_ptr<MaglevCodeGenerator> code_generator);
  MaglevCodeGenerator* code_generator() const { return code_generator_.get(); }
#endif

  bool collect_source_positions() const { return collect_source_positions_; }

  bool specialize_to_function_context() const {
    return specialize_to_function_context_;
  }

  // Must be called from within a MaglevCompilationHandleScope. Transfers owned
  // handles (e.g. shared_, function_) to the new scope.
  void ReopenAndCanonicalizeHandlesInNewScope(Isolate* isolate);

  // Persistent and canonical handles are passed back and forth between the
  // Isolate, this info, and the LocalIsolate.
  void set_persistent_handles(
      std::unique_ptr<PersistentHandles>&& persistent_handles);
  std::unique_ptr<PersistentHandles> DetachPersistentHandles();
  void set_canonical_handles(
      std::unique_ptr<CanonicalHandlesMap>&& canonical_handles);
  std::unique_ptr<CanonicalHandlesMap> DetachCanonicalHandles();

  bool is_detached();

  const CompilationFlags& flags() const { return flags_; }

  uint16_t trace_id() const { return trace_id_; }

  bool could_not_inline_all_candidates() {
    return could_not_inline_all_candidates_;
  }
  void set_could_not_inline_all_candidates() {
    could_not_inline_all_candidates_ = true;
  }

 private:
  V8_EXPORT_PRIVATE MaglevCompilationInfo(
      Isolate* isolate, IndirectHandle<JSFunction> function,
      BytecodeOffset osr_offset,
      std::optional<compiler::JSHeapBroker*> broker = std::nullopt,
      std::optional<bool> specialize_to_function_context = std::nullopt,
      bool is_turbolev = false);

  // Storing the raw pointer to the CanonicalHandlesMap is generally not safe.
  // Use DetachCanonicalHandles() to transfer ownership instead.
  // We explicitly allow the JSHeapBroker to store the raw pointer as it is
  // guaranteed that the MaglevCompilationInfo's lifetime exceeds the lifetime
  // of the broker.
  CanonicalHandlesMap* canonical_handles() { return canonical_handles_.get(); }
  friend compiler::JSHeapBroker;

  Zone zone_;
  compiler::JSHeapBroker* broker_;
  // Must be initialized late since it requires an initialized heap broker.
  MaglevCompilationUnit* toplevel_compilation_unit_ = nullptr;
  IndirectHandle<JSFunction> toplevel_function_;
  IndirectHandle<Code> code_;
  BytecodeOffset osr_offset_;
  const uint16_t trace_id_;

  // True if this MaglevCompilationInfo owns its broker and false otherwise. In
  // particular, when used as Turboshaft front-end, this will use Turboshaft's
  // broker.
  bool owns_broker_ = true;

  // When this MaglevCompilationInfo is created to be used in Turboshaft's
  // frontend, {is_turbolev} is true.
  bool is_turbolev_ = false;

  // True if some inlinees were skipped due to total size constraints.
  bool could_not_inline_all_candidates_ = false;

  bool is_tracing_enabled_ = false;

  std::unique_ptr<MaglevGraphLabeller> graph_labeller_;

#ifdef V8_ENABLE_MAGLEV
  // Produced off-thread during ExecuteJobImpl.
  std::unique_ptr<MaglevCodeGenerator> code_generator_;
#endif

  const CompilationFlags flags_;

  bool collect_source_positions_;

  // If enabled, the generated code can rely on the function context to be a
  // constant (known at compile-time). This opens new optimization
  // opportunities, but prevents code sharing between different function
  // contexts.
  const bool specialize_to_function_context_;

  // 1) PersistentHandles created via PersistentHandlesScope inside of
  //    CompilationHandleScope.
  // 2) Owned by MaglevCompilationInfo.
  // 3) Owned by the broker's LocalHeap when entering the LocalHeapScope.
  // 4) Back to MaglevCompilationInfo when exiting the LocalHeapScope.
  //
  // TODO(jgruber,v8:7700): Update this comment:
  //
  // In normal execution it gets destroyed when PipelineData gets destroyed.
  // There is a special case in GenerateCodeForTesting where the JSHeapBroker
  // will not be retired in that same method. In this case, we need to re-attach
  // the PersistentHandles container to the JSHeapBroker.
  std::unique_ptr<PersistentHandles> ph_;

  // Canonical handles follow the same path as described by the persistent
  // handles above. The only difference is that is created in the
  // CanonicalHandleScope(i.e step 1) is different).
  std::unique_ptr<CanonicalHandlesMap> canonical_handles_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILATION_INFO_H_
