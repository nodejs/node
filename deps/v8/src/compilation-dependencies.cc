// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/zone.h"

#include "src/compilation-dependencies.h"

namespace v8 {
namespace internal {

DependentCode* CompilationDependencies::Get(Handle<Object> object) {
  if (object->IsMap()) {
    return Handle<Map>::cast(object)->dependent_code();
  } else if (object->IsPropertyCell()) {
    return Handle<PropertyCell>::cast(object)->dependent_code();
  } else if (object->IsAllocationSite()) {
    return Handle<AllocationSite>::cast(object)->dependent_code();
  }
  UNREACHABLE();
  return nullptr;
}


void CompilationDependencies::Set(Handle<Object> object,
                                  Handle<DependentCode> dep) {
  if (object->IsMap()) {
    Handle<Map>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsPropertyCell()) {
    Handle<PropertyCell>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsAllocationSite()) {
    Handle<AllocationSite>::cast(object)->set_dependent_code(*dep);
  } else {
    UNREACHABLE();
  }
}


void CompilationDependencies::Insert(DependentCode::DependencyGroup group,
                                     Handle<HeapObject> object) {
  if (groups_[group] == nullptr) {
    groups_[group] = new (zone_) ZoneList<Handle<HeapObject>>(2, zone_);
  }
  groups_[group]->Add(object, zone_);

  if (object_wrapper_.is_null()) {
    // Allocate the wrapper if necessary.
    object_wrapper_ =
        isolate_->factory()->NewForeign(reinterpret_cast<Address>(this));
  }

  // Get the old dependent code list.
  Handle<DependentCode> old_dependent_code =
      Handle<DependentCode>(Get(object), isolate_);
  Handle<DependentCode> new_dependent_code =
      DependentCode::InsertCompilationDependencies(old_dependent_code, group,
                                                   object_wrapper_);

  // Set the new dependent code list if the head of the list changed.
  if (!new_dependent_code.is_identical_to(old_dependent_code)) {
    Set(object, new_dependent_code);
  }
}


void CompilationDependencies::Commit(Handle<Code> code) {
  if (IsEmpty()) return;

  DCHECK(!object_wrapper_.is_null());
  Handle<WeakCell> cell = Code::WeakCellFor(code);
  AllowDeferredHandleDereference get_wrapper;
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ZoneList<Handle<HeapObject>>* group_objects = groups_[i];
    if (group_objects == nullptr) continue;
    DependentCode::DependencyGroup group =
        static_cast<DependentCode::DependencyGroup>(i);
    for (int j = 0; j < group_objects->length(); j++) {
      DependentCode* dependent_code = Get(group_objects->at(j));
      dependent_code->UpdateToFinishedCode(group, *object_wrapper_, *cell);
    }
    groups_[i] = nullptr;  // Zone-allocated, no need to delete.
  }
}


void CompilationDependencies::Rollback() {
  if (IsEmpty()) return;

  AllowDeferredHandleDereference get_wrapper;
  // Unregister from all dependent maps if not yet committed.
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ZoneList<Handle<HeapObject>>* group_objects = groups_[i];
    if (group_objects == nullptr) continue;
    DependentCode::DependencyGroup group =
        static_cast<DependentCode::DependencyGroup>(i);
    for (int j = 0; j < group_objects->length(); j++) {
      DependentCode* dependent_code = Get(group_objects->at(j));
      dependent_code->RemoveCompilationDependencies(group, *object_wrapper_);
    }
    groups_[i] = nullptr;  // Zone-allocated, no need to delete.
  }
}


void CompilationDependencies::AssumeTransitionStable(
    Handle<AllocationSite> site) {
  // Do nothing if the object doesn't have any useful element transitions left.
  ElementsKind kind =
      site->SitePointsToLiteral()
          ? JSObject::cast(site->transition_info())->GetElementsKind()
          : site->GetElementsKind();
  if (AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE) {
    Insert(DependentCode::kAllocationSiteTransitionChangedGroup, site);
  }
}
}  // namespace internal
}  // namespace v8
