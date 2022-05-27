// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compilation-info.h"

#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-heap-broker.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/handles/persistent-handles.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-compiler.h"
#include "src/maglev/maglev-concurrent-dispatcher.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/objects/js-function-inl.h"
#include "src/utils/identity-map.h"
#include "src/utils/locked-queue-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

constexpr char kMaglevZoneName[] = "maglev-compilation-job-zone";

class V8_NODISCARD MaglevCompilationHandleScope final {
 public:
  MaglevCompilationHandleScope(Isolate* isolate,
                               maglev::MaglevCompilationInfo* info)
      : info_(info),
        persistent_(isolate),
        exported_info_(info),
        canonical_(isolate, &exported_info_) {
    info->ReopenHandlesInNewHandleScope(isolate);
  }

  ~MaglevCompilationHandleScope() {
    info_->set_persistent_handles(persistent_.Detach());
  }

 private:
  maglev::MaglevCompilationInfo* const info_;
  PersistentHandlesScope persistent_;
  ExportedMaglevCompilationInfo exported_info_;
  CanonicalHandleScopeForMaglev canonical_;
};

}  // namespace

MaglevCompilationInfo::MaglevCompilationInfo(Isolate* isolate,
                                             Handle<JSFunction> function)
    : zone_(isolate->allocator(), kMaglevZoneName),
      isolate_(isolate),
      broker_(new compiler::JSHeapBroker(
          isolate, zone(), FLAG_trace_heap_broker, CodeKind::MAGLEV))
#define V(Name) , Name##_(FLAG_##Name)
          MAGLEV_COMPILATION_FLAG_LIST(V)
#undef V
{
  DCHECK(FLAG_maglev);

  MaglevCompilationHandleScope compilation(isolate, this);

  compiler::CompilationDependencies* deps =
      zone()->New<compiler::CompilationDependencies>(broker(), zone());
  USE(deps);  // The deps register themselves in the heap broker.

  // Heap broker initialization may already use IsPendingAllocation.
  isolate->heap()->PublishPendingAllocations();

  broker()->SetTargetNativeContextRef(
      handle(function->native_context(), isolate));
  broker()->InitializeAndStartSerializing();
  broker()->StopSerializing();

  // Serialization may have allocated.
  isolate->heap()->PublishPendingAllocations();

  toplevel_compilation_unit_ =
      MaglevCompilationUnit::New(zone(), this, function);
}

MaglevCompilationInfo::~MaglevCompilationInfo() = default;

void MaglevCompilationInfo::set_graph_labeller(
    MaglevGraphLabeller* graph_labeller) {
  graph_labeller_.reset(graph_labeller);
}

void MaglevCompilationInfo::ReopenHandlesInNewHandleScope(Isolate* isolate) {}

void MaglevCompilationInfo::set_persistent_handles(
    std::unique_ptr<PersistentHandles>&& persistent_handles) {
  DCHECK_NULL(ph_);
  ph_ = std::move(persistent_handles);
  DCHECK_NOT_NULL(ph_);
}

std::unique_ptr<PersistentHandles>
MaglevCompilationInfo::DetachPersistentHandles() {
  DCHECK_NOT_NULL(ph_);
  return std::move(ph_);
}

void MaglevCompilationInfo::set_canonical_handles(
    std::unique_ptr<CanonicalHandlesMap>&& canonical_handles) {
  DCHECK_NULL(canonical_handles_);
  canonical_handles_ = std::move(canonical_handles);
  DCHECK_NOT_NULL(canonical_handles_);
}

std::unique_ptr<CanonicalHandlesMap>
MaglevCompilationInfo::DetachCanonicalHandles() {
  DCHECK_NOT_NULL(canonical_handles_);
  return std::move(canonical_handles_);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
