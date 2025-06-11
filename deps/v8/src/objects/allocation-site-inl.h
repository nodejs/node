// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_INL_H_
#define V8_OBJECTS_ALLOCATION_SITE_INL_H_

#include "src/objects/allocation-site.h"
// Include the non-inl header before the rest of the headers.

#include <atomic>

#include "src/common/globals.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/dependent-code-inl.h"
#include "src/objects/js-objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/allocation-site-tq-inl.inc"

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

inline int32_t AllocationSite::pretenure_data(RelaxedLoadTag) const {
  return pretenure_data_.load(std::memory_order_relaxed);
}
inline void AllocationSite::set_pretenure_data(int32_t value, RelaxedStoreTag) {
  pretenure_data_.store(value, std::memory_order_relaxed);
}

inline int32_t AllocationSite::pretenure_create_count() const {
  return pretenure_create_count_;
}
inline void AllocationSite::set_pretenure_create_count(int32_t value) {
  pretenure_create_count_ = value;
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

inline bool AllocationSite::HasWeakNext() const {
  return map() == GetReadOnlyRoots().allocation_site_map();
}

inline void AllocationSite::Initialize() {
  transition_info_or_boilerplate_.store(this, Smi::zero(), SKIP_WRITE_BARRIER);
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::zero());
  set_pretenure_data(0, kRelaxedStore);
  set_pretenure_create_count(0);
  set_dependent_code(DependentCode::empty_dependent_code(GetReadOnlyRoots()),
                     SKIP_WRITE_BARRIER);
}

inline bool AllocationSite::IsZombie() const {
  return pretenure_decision() == kZombie;
}

inline bool AllocationSite::IsMaybeTenure() const {
  return pretenure_decision() == kMaybeTenure;
}

inline bool AllocationSite::PretenuringDecisionMade() const {
  return pretenure_decision() != kUndecided;
}

inline void AllocationSite::MarkZombie() {
  DCHECK(!IsZombie());
  Initialize();
  set_pretenure_decision(kZombie);
}

inline ElementsKind AllocationSite::GetElementsKind() const {
  return ElementsKindBits::decode(transition_info());
}

inline void AllocationSite::SetElementsKind(ElementsKind kind) {
  set_transition_info(ElementsKindBits::update(transition_info(), kind));
}

inline bool AllocationSite::CanInlineCall() const {
  return DoNotInlineBit::decode(transition_info()) == 0;
}

inline void AllocationSite::SetDoNotInlineCall() {
  set_transition_info(DoNotInlineBit::update(transition_info(), true));
}

inline bool AllocationSite::PointsToLiteral() const {
  Tagged<Object> raw_value = transition_info_or_boilerplate_.Acquire_Load();
  DCHECK_EQ(!IsSmi(raw_value), IsJSArray(raw_value) || IsJSObject(raw_value));
  return !IsSmi(raw_value);
}

// Heuristic: We only need to create allocation site info if the boilerplate
// elements kind is the initial elements kind.
inline bool AllocationSite::ShouldTrack(
    ElementsKind boilerplate_elements_kind) {
  if (!V8_ALLOCATION_SITE_TRACKING_BOOL) return false;
  return IsSmiElementsKind(boilerplate_elements_kind);
}

inline bool AllocationSite::CanTrack(InstanceType type) {
  if (!V8_ALLOCATION_SITE_TRACKING_BOOL) return false;
  if (v8_flags.allocation_site_pretenuring) {
    // TurboFan doesn't care at all about String pretenuring feedback,
    // so don't bother even trying to track that.
    return type == JS_ARRAY_TYPE || type == JS_OBJECT_TYPE;
  }
  return type == JS_ARRAY_TYPE;
}

inline AllocationSite::PretenureDecision AllocationSite::pretenure_decision()
    const {
  return PretenureDecisionBits::decode(pretenure_data(kRelaxedLoad));
}

inline void AllocationSite::set_pretenure_decision(PretenureDecision decision) {
  int32_t value = pretenure_data(kRelaxedLoad);
  set_pretenure_data(PretenureDecisionBits::update(value, decision),
                     kRelaxedStore);
}

inline bool AllocationSite::deopt_dependent_code() const {
  return DeoptDependentCodeBit::decode(pretenure_data(kRelaxedLoad));
}

inline void AllocationSite::set_deopt_dependent_code(bool deopt) {
  int32_t value = pretenure_data(kRelaxedLoad);
  set_pretenure_data(DeoptDependentCodeBit::update(value, deopt),
                     kRelaxedStore);
}

inline int AllocationSite::memento_found_count() const {
  return MementoFoundCountBits::decode(pretenure_data(kRelaxedLoad));
}

inline void AllocationSite::set_memento_found_count(int count) {
  int32_t value = pretenure_data(kRelaxedLoad);
  // Verify that we can count more mementos than we can possibly find in one
  // new space collection.
  DCHECK((Isolate::Current()->heap()->MaxSemiSpaceSize() /
          (Heap::kMinObjectSizeInTaggedWords * kTaggedSize +
           sizeof(AllocationSiteWithWeakNext))) < MementoFoundCountBits::kMax);
  DCHECK_LT(count, MementoFoundCountBits::kMax);
  set_pretenure_data(MementoFoundCountBits::update(value, count),
                     kRelaxedStore);
}

inline int AllocationSite::memento_create_count() const {
  return pretenure_create_count();
}

inline void AllocationSite::set_memento_create_count(int count) {
  set_pretenure_create_count(count);
}

int AllocationSite::IncrementMementoFoundCount(int increment) {
  DCHECK(!IsZombie());

  int new_value = memento_found_count() + increment;
  set_memento_found_count(new_value);
  return new_value;
}

inline void AllocationSite::IncrementMementoCreateCount() {
  DCHECK(v8_flags.allocation_site_pretenuring);
  int value = memento_create_count();
  set_memento_create_count(value + 1);
}

inline Tagged<UnionOf<Undefined, AllocationSiteWithWeakNext>>
AllocationSiteWithWeakNext::weak_next() const {
  return weak_next_.load();
}
inline void AllocationSiteWithWeakNext::set_weak_next(
    Tagged<UnionOf<Undefined, AllocationSiteWithWeakNext>> value,
    WriteBarrierMode mode) {
  weak_next_.store(this, value, mode);
}

inline bool AllocationMemento::IsValid() const {
  return !allocation_site_.load()->IsZombie();
}

void AllocationMemento::set_allocation_site(Tagged<AllocationSite> value,
                                            WriteBarrierMode mode) {
  allocation_site_.store(this, value, mode);
}
inline Tagged<AllocationSite> AllocationMemento::GetAllocationSite() const {
  DCHECK(IsValid());
  return allocation_site_.load();
}

inline Address AllocationMemento::GetAllocationSiteUnchecked() const {
  return allocation_site_.load().ptr();
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
          bool is_nested = site->IsNested();
          PrintF("AllocationSite: JSArray %p boilerplate %supdated %s->%s\n",
                 reinterpret_cast<void*>(site->ptr()),
                 is_nested ? "(nested)" : " ", ElementsKindToString(kind),
                 ElementsKindToString(to_kind));
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
