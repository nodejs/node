// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_INL_H_
#define V8_OBJECTS_ALLOCATION_SITE_INL_H_

#include "src/objects/allocation-site.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/dependent-code-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/object-conversions-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

inline Tagged<UnionOf<Smi, JSObject>>
AllocationSite::transition_info_or_boilerplate() const {
  return transition_info_or_boilerplate_.load();
}

inline Tagged<UnionOf<Smi, AllocationSite>> AllocationSite::nested_site()
    const {
  return nested_site_.load();
}
inline void AllocationSite::set_nested_site(
    Tagged<UnionOf<Smi, AllocationSite>> value, WriteBarrierMode mode) {
  nested_site_.store(this, value, mode);
}

inline Tagged<DependentCode> AllocationSite::dependent_code() const {
  return dependent_code_.load();
}
void AllocationSite::set_dependent_code(Tagged<DependentCode> value,
                                        WriteBarrierMode mode) {
  dependent_code_.store(this, value, mode);
}

inline Tagged<JSObject> AllocationSite::boilerplate() const {
  DCHECK(PointsToLiteral());
  return Cast<JSObject>(transition_info_or_boilerplate_.load());
}

inline Tagged<JSObject> AllocationSite::boilerplate(AcquireLoadTag tag) const {
  DCHECK(PointsToLiteral());
  return Cast<JSObject>(transition_info_or_boilerplate_.Acquire_Load());
}

inline void AllocationSite::set_boilerplate(Tagged<JSObject> value,
                                            ReleaseStoreTag tag,
                                            WriteBarrierMode mode) {
  transition_info_or_boilerplate_.Release_Store(this, value, mode);
}

inline int AllocationSite::transition_info() const {
  DCHECK(!PointsToLiteral());
  return Cast<Smi>(transition_info_or_boilerplate_.Acquire_Load()).value();
}

void AllocationSite::set_transition_info(int value) {
  DCHECK(!PointsToLiteral());
  transition_info_or_boilerplate_.Release_Store(this, Smi::FromInt(value),
                                                SKIP_WRITE_BARRIER);
}

inline void AllocationSite::Initialize() {
  transition_info_or_boilerplate_.store(this, Smi::zero(), SKIP_WRITE_BARRIER);
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::zero());
  set_dependent_code(DependentCode::empty_dependent_code(GetReadOnlyRoots()),
                     SKIP_WRITE_BARRIER);
}

inline ElementsKind AllocationSite::GetElementsKind() const {
  return ElementsKindBits::decode(transition_info());
}

inline void AllocationSite::SetElementsKind(ElementsKind kind) {
  set_transition_info(ElementsKindBits::update(transition_info(), kind));
}

inline bool AllocationSite::IsSpeculationDisabled() const {
  return SpeculationDisabledBit::decode(transition_info()) == 1;
}

inline void AllocationSite::SetSpeculationDisabled() {
  set_transition_info(SpeculationDisabledBit::update(transition_info(), true));
}

inline bool AllocationSite::PointsToLiteral() const {
  Tagged<Object> raw_value = transition_info_or_boilerplate_.Acquire_Load();
  DCHECK_EQ(!IsSmi(raw_value), IsJSArray(raw_value) || IsJSObject(raw_value));
  return !IsSmi(raw_value);
}

// We only need to create allocation site info if the boilerplate elements kind
// can transition to a more general fast elements kind.
inline bool AllocationSite::ShouldTrack(
    ElementsKind boilerplate_elements_kind) {
  return ShouldTrack(boilerplate_elements_kind, TERMINAL_FAST_ELEMENTS_KIND);
}

inline bool AllocationSite::CanTrack(InstanceType type) {
  if (!V8_ALLOCATION_SITE_TRACKING_BOOL) return false;
  return type == JS_ARRAY_TYPE;
}

void AllocationMemento::set_allocation_site(Tagged<AllocationSite> value,
                                            WriteBarrierMode mode) {
  allocation_site_.store(this, value, mode);
}
inline Tagged<AllocationSite> AllocationMemento::GetAllocationSite() const {
  Tagged<AllocationSite> site = allocation_site_.load();
  CHECK(IsAllocationSite(site));
  return site;
}

template <AllocationSiteUpdateMode update_or_check>
bool AllocationSite::DigestTransitionFeedback(Isolate* isolate,
                                              DirectHandle<AllocationSite> site,
                                              ElementsKind to_kind) {
  bool result = false;

  if (site->PointsToLiteral() && IsJSArray(site->boilerplate())) {
    DirectHandle<JSArray> boilerplate(Cast<JSArray>(site->boilerplate()),
                                      isolate);
    ElementsKind kind = boilerplate->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      // If the array is huge, it's not likely to be defined in a local
      // function, so we shouldn't make new instances of it very often.
      uint32_t length = 0;
      CHECK(Object::ToArrayLength(boilerplate->length(), &length));
      if (length <= kMaximumArrayBytesToPretransition) {
        if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) {
          return true;
        }
        if (v8_flags.trace_track_allocation_sites) {
          PrintF("AllocationSite: JSArray %p boilerplate updated %s->%s\n",
                 reinterpret_cast<void*>(site->ptr()),
                 ElementsKindToString(kind), ElementsKindToString(to_kind));
        }
        CHECK_NE(to_kind, DICTIONARY_ELEMENTS);
        JSObject::TransitionElementsKind(isolate, boilerplate, to_kind);
        DependentCode::DeoptimizeDependencyGroups(
            isolate, *site,
            DependentCode::kAllocationSiteTransitionChangedGroup);
        result = true;
      }
    }
  } else {
    // The AllocationSite is for a constructed Array.
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) return true;
      if (v8_flags.trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(site->ptr()), ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->SetElementsKind(to_kind);
      DependentCode::DeoptimizeDependencyGroups(
          isolate, *site, DependentCode::kAllocationSiteTransitionChangedGroup);
      result = true;
    }
  }
  return result;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ALLOCATION_SITE_INL_H_
