// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_H_
#define V8_OBJECTS_ALLOCATION_SITE_H_

#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

enum InstanceType : uint16_t;

V8_OBJECT class AllocationSite : public HeapObject {
  V8_IT_OWN_TYPE;
  V8_IT_NO_AUTO_DISPATCH;

 public:
  static const uint32_t kMaximumArrayBytesToPretransition = 8 * 1024;

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

  inline Tagged<DependentCode> dependent_code() const;
  inline void set_dependent_code(Tagged<DependentCode> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void Initialize();

  // transition_info bitfields, for constructed array transition info.
  using ElementsKindBits = base::BitField<ElementsKind, 0, 6>;
  using SpeculationDisabledBit = base::BitField<bool, 6, 1>;
  // Unused bits 7-30.

  inline ElementsKind GetElementsKind() const;
  inline void SetElementsKind(ElementsKind kind);

  // The IsSpeculationDisabled flag is only used for the Array constructor, and
  // protects against deopt loops due to speculative optimizations.
  inline bool IsSpeculationDisabled() const;
  inline void SetSpeculationDisabled();

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
  friend class CodeStubAssembler;
  friend class ArrayBuiltinsAssembler;
  friend class V8HeapExplorer;

  // Contains either a Smi-encoded bitfield or a boilerplate. If it's a Smi the
  // AllocationSite is for a constructed Array.
  TaggedMember<UnionOf<Smi, JSObject>> transition_info_or_boilerplate_;
  TaggedMember<UnionOf<Smi, AllocationSite>> nested_site_;
  TaggedMember<DependentCode> dependent_code_;
} V8_OBJECT_END;

V8_OBJECT class AllocationMemento : public Struct {
 public:
  inline void set_allocation_site(Tagged<AllocationSite> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<AllocationSite> GetAllocationSite() const;

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
