// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_COMPILATION_INFO_H_
#define V8_MAGLEV_MAGLEV_COMPILATION_INFO_H_

#include <memory>

#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"

namespace v8 {

namespace base {
class DefaultAllocationPolicy;
}

namespace internal {

class Isolate;
class PersistentHandles;
class SharedFunctionInfo;
class TranslationArrayBuilder;
class Zone;

namespace compiler {
class JSHeapBroker;
}

namespace maglev {

class Graph;
class MaglevCompilationUnit;
class MaglevGraphLabeller;

// A list of v8_flag values copied into the MaglevCompilationInfo for
// guaranteed {immutable,threadsafe} access.
#define MAGLEV_COMPILATION_FLAG_LIST(V) \
  V(code_comments)                      \
  V(maglev)                             \
  V(print_maglev_code)                  \
  V(print_maglev_graph)                 \
  V(trace_maglev_regalloc)

class MaglevCompilationInfo final {
 public:
  static std::unique_ptr<MaglevCompilationInfo> New(
      Isolate* isolate, Handle<JSFunction> function) {
    // Doesn't use make_unique due to the private ctor.
    return std::unique_ptr<MaglevCompilationInfo>(
        new MaglevCompilationInfo(isolate, function));
  }
  ~MaglevCompilationInfo();

  Zone* zone() { return &zone_; }
  compiler::JSHeapBroker* broker() const { return broker_.get(); }
  MaglevCompilationUnit* toplevel_compilation_unit() const {
    return toplevel_compilation_unit_;
  }

  bool has_graph_labeller() const { return !!graph_labeller_; }
  void set_graph_labeller(MaglevGraphLabeller* graph_labeller);
  MaglevGraphLabeller* graph_labeller() const {
    DCHECK(has_graph_labeller());
    return graph_labeller_.get();
  }

  void set_graph(Graph* graph) { graph_ = graph; }
  Graph* graph() const { return graph_; }

  void set_translation_array_builder(
      std::unique_ptr<TranslationArrayBuilder> translation_array_builder,
      std::unique_ptr<IdentityMap<int, base::DefaultAllocationPolicy>>
          deopt_literals);
  TranslationArrayBuilder& translation_array_builder() const {
    DCHECK(translation_array_builder_);
    return *translation_array_builder_;
  }
  IdentityMap<int, base::DefaultAllocationPolicy>& deopt_literals() const {
    DCHECK(deopt_literals_);
    return *deopt_literals_;
  }

  // Flag accessors (for thread-safe access to global flags).
  // TODO(v8:7700): Consider caching these.
#define V(Name) \
  bool Name() const { return Name##_; }
  MAGLEV_COMPILATION_FLAG_LIST(V)
#undef V

  bool specialize_to_function_context() const {
    return specialize_to_function_context_;
  }

  // Must be called from within a MaglevCompilationHandleScope. Transfers owned
  // handles (e.g. shared_, function_) to the new scope.
  void ReopenHandlesInNewHandleScope(Isolate* isolate);

  // Persistent and canonical handles are passed back and forth between the
  // Isolate, this info, and the LocalIsolate.
  void set_persistent_handles(
      std::unique_ptr<PersistentHandles>&& persistent_handles);
  std::unique_ptr<PersistentHandles> DetachPersistentHandles();
  void set_canonical_handles(
      std::unique_ptr<CanonicalHandlesMap>&& canonical_handles);
  std::unique_ptr<CanonicalHandlesMap> DetachCanonicalHandles();

 private:
  MaglevCompilationInfo(Isolate* isolate, Handle<JSFunction> function);

  Zone zone_;
  const std::unique_ptr<compiler::JSHeapBroker> broker_;
  // Must be initialized late since it requires an initialized heap broker.
  MaglevCompilationUnit* toplevel_compilation_unit_ = nullptr;

  std::unique_ptr<MaglevGraphLabeller> graph_labeller_;

  // Produced off-thread during ExecuteJobImpl.
  Graph* graph_ = nullptr;

  std::unique_ptr<TranslationArrayBuilder> translation_array_builder_;
  std::unique_ptr<IdentityMap<int, base::DefaultAllocationPolicy>>
      deopt_literals_;

#define V(Name) const bool Name##_;
  MAGLEV_COMPILATION_FLAG_LIST(V)
#undef V

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
