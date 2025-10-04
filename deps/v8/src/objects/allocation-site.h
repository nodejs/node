// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_H_
#define V8_OBJECTS_ALLOCATION_SITE_H_

#include <atomic>

#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum InstanceType : uint16_t;

#include "torque-generated/src/objects/allocation-site-tq.inc"

V8_OBJECT class AllocationSite : public HeapObjectLayout {
 public:
  static const uint32_t kMaximumArrayBytesToPretransition = 8 * 1024;

  // Values for pretenure decision field.
  enum PretenureDecision {
    kUndecided = 0,
    kDontTenure = 1,
    kMaybeTenure = 2,
    kTenure = 3,
    kZombie = 4,  // See comment to IsZombie() for documentation.
    kLastPretenureDecisionValue = kZombie
  };

  const char* PretenureDecisionName(PretenureDecision decision);

  inline Tagged<UnionOf<Smi, JSObject>> transition_info_or_boilerplate() const;

  inline Tagged<JSObject> boilerplate() const;
  inline Tagged<JSObject> boilerplate(AcquireLoadTag tag) const;
  inline void set_boilerplate(Tagged<JSObject> value, ReleaseStoreTag,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int transition_info() const;
  inline void set_transition_info(int value);

  // nested_site threads a list of sites that represent nested literals
  // walked in a particular order. So [[1, 2], 1, 2] will have one
  // nested_site, but [[1, 2], 3, [4]] will have a list of two.
  inline Tagged<UnionOf<Smi, AllocationSite>> nested_site() const;
  inline void set_nested_site(Tagged<UnionOf<Smi, AllocationSite>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Bitfield containing pretenuring information.
  inline int32_t pretenure_data(RelaxedLoadTag) const;
  inline void set_pretenure_data(int32_t value, RelaxedStoreTag);

  inline int32_t pretenure_create_count() const;
  inline void set_pretenure_create_count(int32_t value);

  inline Tagged<DependentCode> dependent_code() const;
  inline void set_dependent_code(Tagged<DependentCode> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void Initialize();

  // Checks if the allocation site contain weak_next field;
  inline bool HasWeakNext() const;

  // This method is expensive, it should only be called for reporting.
  bool IsNested();

  // transition_info bitfields, for constructed array transition info.
  using ElementsKindBits = base::BitField<ElementsKind, 0, 6>;
  using DoNotInlineBit = base::BitField<bool, 6, 1>;
  // Unused bits 7-30.

  // Bitfields for pretenure_data
  using MementoFoundCountBits = base::BitField<int, 0, 26>;
  using PretenureDecisionBits = base::BitField<PretenureDecision, 26, 3>;
  using DeoptDependentCodeBit = base::BitField<bool, 29, 1>;
  static_assert(PretenureDecisionBits::kMax >= kLastPretenureDecisionValue);

  // Increments the mementos found counter and returns the new count.
  inline int IncrementMementoFoundCount(int increment = 1);

  inline void IncrementMementoCreateCount();

  AllocationType GetAllocationType() const;

  void ResetPretenureDecision();

  inline PretenureDecision pretenure_decision() const;
  inline void set_pretenure_decision(PretenureDecision decision);

  inline bool deopt_dependent_code() const;
  inline void set_deopt_dependent_code(bool deopt);

  inline int memento_found_count() const;
  inline void set_memento_found_count(int count);

  inline int memento_create_count() const;
  inline void set_memento_create_count(int count);

  // A "zombie" AllocationSite is one which has no more strong roots to
  // it, and yet must be maintained until the next GC. The reason is that
  // it may be that in new space there are AllocationMementos hanging around
  // which point to the AllocationSite. If we scavenge these AllocationSites
  // too soon, those AllocationMementos will end up pointing to garbage
  // addresses. The concrete case happens when evacuating new space in the full
  // GC which happens after sweeping has been started already. To mitigate this
  // problem the garbage collector marks such AllocationSites as zombies when it
  // discovers there are no roots, allowing the subsequent collection pass to
  // recognize zombies and discard them later.
  inline bool IsZombie() const;

  inline bool IsMaybeTenure() const;

  inline void MarkZombie();

  inline bool MakePretenureDecision(PretenureDecision current_decision,
                                    double ratio, bool maximum_size_scavenge);

  inline bool DigestPretenuringFeedback(bool maximum_size_scavenge);

  inline ElementsKind GetElementsKind() const;
  inline void SetElementsKind(ElementsKind kind);

  inline bool CanInlineCall() const;
  inline void SetDoNotInlineCall();

  inline bool PointsToLiteral() const;

  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool DigestTransitionFeedback(Isolate* isolate,
                                       DirectHandle<AllocationSite> site,
                                       ElementsKind to_kind);

  DECL_PRINTER(AllocationSite)
  DECL_VERIFIER(AllocationSite)

  static inline bool ShouldTrack(ElementsKind boilerplate_elements_kind);
  static bool ShouldTrack(ElementsKind from, ElementsKind to);
  static inline bool CanTrack(InstanceType type);

  class BodyDescriptor;

 private:
  inline bool PretenuringDecisionMade() const;

 private:
  friend class CodeStubAssembler;
  friend class ArrayBuiltinsAssembler;
  friend class ObjectBuiltinsAssembler;
  friend class V8HeapExplorer;

  // Contains either a Smi-encoded bitfield or a boilerplate. If it's a Smi the
  // AllocationSite is for a constructed Array.
  TaggedMember<UnionOf<Smi, JSObject>> transition_info_or_boilerplate_;
  TaggedMember<UnionOf<Smi, AllocationSite>> nested_site_;
  TaggedMember<DependentCode> dependent_code_;
  std::atomic<int32_t> pretenure_data_;
  int32_t pretenure_create_count_;
} V8_OBJECT_END;

V8_OBJECT class AllocationSiteWithWeakNext : public AllocationSite {
 public:
  // heap->allocation_site_list() points to the last AllocationSite which form
  // a linked list through the weak_next property. The GC might remove elements
  // from the list by updating weak_next.
  inline Tagged<UnionOf<Undefined, AllocationSiteWithWeakNext>> weak_next()
      const;
  inline void set_weak_next(
      Tagged<UnionOf<Undefined, AllocationSiteWithWeakNext>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

 private:
  friend class CodeStubAssembler;
  friend class AllocationSite::BodyDescriptor;
  template <typename T>
  friend struct WeakListVisitor;
  friend class V8HeapExplorer;

  TaggedMember<UnionOf<Undefined, AllocationSiteWithWeakNext>> weak_next_;
} V8_OBJECT_END;

V8_OBJECT class AllocationMemento : public StructLayout {
 public:
  inline void set_allocation_site(Tagged<AllocationSite> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool IsValid() const;
  inline Tagged<AllocationSite> GetAllocationSite() const;
  inline Address GetAllocationSiteUnchecked() const;

  DECL_PRINTER(AllocationMemento)
  DECL_VERIFIER(AllocationMemento)

 private:
  friend class CodeStubAssembler;
  friend class TorqueGeneratedAllocationMementoAsserts;

  TaggedMember<AllocationSite> allocation_site_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ALLOCATION_SITE_H_
