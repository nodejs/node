// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEPENDENCIES_H_
#define V8_DEPENDENCIES_H_

#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Collects dependencies for this compilation, e.g. assumptions about
// stable maps, constant globals, etc.
class CompilationDependencies {
 public:
  CompilationDependencies(Isolate* isolate, Zone* zone)
      : isolate_(isolate),
        zone_(zone),
        object_wrapper_(Handle<Foreign>::null()),
        aborted_(false) {
    std::fill_n(groups_, DependentCode::kGroupCount, nullptr);
  }

  void Insert(DependentCode::DependencyGroup group, Handle<HeapObject> handle);

  void AssumeInitialMapCantChange(Handle<Map> map) {
    Insert(DependentCode::kInitialMapChangedGroup, map);
  }
  void AssumeFieldType(Handle<Map> map) {
    Insert(DependentCode::kFieldTypeGroup, map);
  }
  void AssumeMapStable(Handle<Map> map);
  void AssumePrototypeMapsStable(
      Handle<Map> map,
      MaybeHandle<JSReceiver> prototype = MaybeHandle<JSReceiver>());
  void AssumeMapNotDeprecated(Handle<Map> map);
  void AssumePropertyCell(Handle<PropertyCell> cell) {
    Insert(DependentCode::kPropertyCellChangedGroup, cell);
  }
  void AssumeTenuringDecision(Handle<AllocationSite> site) {
    Insert(DependentCode::kAllocationSiteTenuringChangedGroup, site);
  }
  void AssumeTransitionStable(Handle<AllocationSite> site);

  void Commit(Handle<Code> code);
  void Rollback();
  void Abort() { aborted_ = true; }
  bool HasAborted() const { return aborted_; }

  bool IsEmpty() const {
    for (int i = 0; i < DependentCode::kGroupCount; i++) {
      if (groups_[i]) return false;
    }
    return true;
  }

 private:
  Isolate* isolate_;
  Zone* zone_;
  Handle<Foreign> object_wrapper_;
  bool aborted_;
  ZoneList<Handle<HeapObject> >* groups_[DependentCode::kGroupCount];

  DependentCode* Get(Handle<Object> object);
  void Set(Handle<Object> object, Handle<DependentCode> dep);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_DEPENDENCIES_H_
