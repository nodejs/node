// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PROCESSED_FEEDBACK_H_
#define V8_COMPILER_PROCESSED_FEEDBACK_H_

#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"

namespace v8 {
namespace internal {
namespace compiler {

class BinaryOperationFeedback;
class CallFeedback;
class CompareOperationFeedback;
class ElementAccessFeedback;
class ForInFeedback;
class GlobalAccessFeedback;
class InstanceOfFeedback;
class LiteralFeedback;
class MinimorphicLoadPropertyAccessFeedback;
class NamedAccessFeedback;
class RegExpLiteralFeedback;
class TemplateObjectFeedback;

class ProcessedFeedback : public ZoneObject {
 public:
  enum Kind {
    kInsufficient,
    kBinaryOperation,
    kCall,
    kCompareOperation,
    kElementAccess,
    kForIn,
    kGlobalAccess,
    kInstanceOf,
    kLiteral,
    kMinimorphicPropertyAccess,
    kNamedAccess,
    kRegExpLiteral,
    kTemplateObject,
  };
  Kind kind() const { return kind_; }

  FeedbackSlotKind slot_kind() const { return slot_kind_; }
  bool IsInsufficient() const { return kind() == kInsufficient; }

  BinaryOperationFeedback const& AsBinaryOperation() const;
  CallFeedback const& AsCall() const;
  CompareOperationFeedback const& AsCompareOperation() const;
  ElementAccessFeedback const& AsElementAccess() const;
  ForInFeedback const& AsForIn() const;
  GlobalAccessFeedback const& AsGlobalAccess() const;
  InstanceOfFeedback const& AsInstanceOf() const;
  NamedAccessFeedback const& AsNamedAccess() const;
  MinimorphicLoadPropertyAccessFeedback const& AsMinimorphicPropertyAccess()
      const;
  LiteralFeedback const& AsLiteral() const;
  RegExpLiteralFeedback const& AsRegExpLiteral() const;
  TemplateObjectFeedback const& AsTemplateObject() const;

 protected:
  ProcessedFeedback(Kind kind, FeedbackSlotKind slot_kind);

 private:
  Kind const kind_;
  FeedbackSlotKind const slot_kind_;
};

class InsufficientFeedback final : public ProcessedFeedback {
 public:
  explicit InsufficientFeedback(FeedbackSlotKind slot_kind);
};

class GlobalAccessFeedback : public ProcessedFeedback {
 public:
  GlobalAccessFeedback(PropertyCellRef cell, FeedbackSlotKind slot_kind);
  GlobalAccessFeedback(ContextRef script_context, int slot_index,
                       bool immutable, FeedbackSlotKind slot_kind);
  explicit GlobalAccessFeedback(FeedbackSlotKind slot_kind);  // Megamorphic

  bool IsMegamorphic() const;

  bool IsPropertyCell() const;
  PropertyCellRef property_cell() const;

  bool IsScriptContextSlot() const;
  ContextRef script_context() const;
  int slot_index() const;
  bool immutable() const;

  base::Optional<ObjectRef> GetConstantHint() const;

 private:
  base::Optional<ObjectRef> const cell_or_context_;
  int const index_and_immutable_;
};

class KeyedAccessMode {
 public:
  static KeyedAccessMode FromNexus(FeedbackNexus const& nexus);

  AccessMode access_mode() const;
  bool IsLoad() const;
  bool IsStore() const;
  KeyedAccessLoadMode load_mode() const;
  KeyedAccessStoreMode store_mode() const;

 private:
  AccessMode const access_mode_;
  union LoadStoreMode {
    LoadStoreMode(KeyedAccessLoadMode load_mode);
    LoadStoreMode(KeyedAccessStoreMode store_mode);
    KeyedAccessLoadMode load_mode;
    KeyedAccessStoreMode store_mode;
  } const load_store_mode_;

  KeyedAccessMode(AccessMode access_mode, KeyedAccessLoadMode load_mode);
  KeyedAccessMode(AccessMode access_mode, KeyedAccessStoreMode store_mode);
};

class ElementAccessFeedback : public ProcessedFeedback {
 public:
  ElementAccessFeedback(Zone* zone, KeyedAccessMode const& keyed_mode,
                        FeedbackSlotKind slot_kind);

  KeyedAccessMode keyed_mode() const;

  // A transition group is a target and a possibly empty set of sources that can
  // transition to the target. It is represented as a non-empty vector with the
  // target at index 0.
  using TransitionGroup = ZoneVector<Handle<Map>>;
  ZoneVector<TransitionGroup> const& transition_groups() const;

  bool HasOnlyStringMaps(JSHeapBroker* broker) const;

  void AddGroup(TransitionGroup&& group);

  // Refine {this} by trying to restrict it to the maps in {inferred_maps}. A
  // transition group's target is kept iff it is in {inferred_maps} or if more
  // than one of its sources is in {inferred_maps}. Here's an (unrealistic)
  // example showing all the possible situations:
  //
  // inferred_maps = [a0, a2, c1, c2, d1, e0, e1]
  //
  // Groups before:                     Groups after:
  // [a0, a1, a2]                       [a0, a2]
  // [b0]
  // [c0, c1, c2, c3]                   [c0, c1, c2]
  // [d0, d1]                           [d1]
  // [e0, e1]                           [e0, e1]
  //
  ElementAccessFeedback const& Refine(
      ZoneVector<Handle<Map>> const& inferred_maps, Zone* zone) const;

 private:
  KeyedAccessMode const keyed_mode_;
  ZoneVector<TransitionGroup> transition_groups_;
};

class NamedAccessFeedback : public ProcessedFeedback {
 public:
  NamedAccessFeedback(NameRef const& name, ZoneVector<Handle<Map>> const& maps,
                      FeedbackSlotKind slot_kind);

  NameRef const& name() const { return name_; }
  ZoneVector<Handle<Map>> const& maps() const { return maps_; }

 private:
  NameRef const name_;
  ZoneVector<Handle<Map>> const maps_;
};

class MinimorphicLoadPropertyAccessFeedback : public ProcessedFeedback {
 public:
  MinimorphicLoadPropertyAccessFeedback(NameRef const& name,
                                        FeedbackSlotKind slot_kind,
                                        Handle<Object> handler,
                                        MaybeHandle<Map> maybe_map,
                                        bool has_migration_target_maps);

  NameRef const& name() const { return name_; }
  bool is_monomorphic() const { return !maybe_map_.is_null(); }
  Handle<Object> handler() const { return handler_; }
  MaybeHandle<Map> map() const { return maybe_map_; }
  bool has_migration_target_maps() const { return has_migration_target_maps_; }

 private:
  NameRef const name_;
  Handle<Object> const handler_;
  MaybeHandle<Map> const maybe_map_;
  bool const has_migration_target_maps_;
};

class CallFeedback : public ProcessedFeedback {
 public:
  CallFeedback(base::Optional<HeapObjectRef> target, float frequency,
               SpeculationMode mode, FeedbackSlotKind slot_kind)
      : ProcessedFeedback(kCall, slot_kind),
        target_(target),
        frequency_(frequency),
        mode_(mode) {}

  base::Optional<HeapObjectRef> target() const { return target_; }
  float frequency() const { return frequency_; }
  SpeculationMode speculation_mode() const { return mode_; }

 private:
  base::Optional<HeapObjectRef> const target_;
  float const frequency_;
  SpeculationMode const mode_;
};

template <class T, ProcessedFeedback::Kind K>
class SingleValueFeedback : public ProcessedFeedback {
 public:
  explicit SingleValueFeedback(T value, FeedbackSlotKind slot_kind)
      : ProcessedFeedback(K, slot_kind), value_(value) {
    DCHECK(
        (K == kBinaryOperation && slot_kind == FeedbackSlotKind::kBinaryOp) ||
        (K == kCompareOperation && slot_kind == FeedbackSlotKind::kCompareOp) ||
        (K == kForIn && slot_kind == FeedbackSlotKind::kForIn) ||
        (K == kInstanceOf && slot_kind == FeedbackSlotKind::kInstanceOf) ||
        ((K == kLiteral || K == kRegExpLiteral || K == kTemplateObject) &&
         slot_kind == FeedbackSlotKind::kLiteral));
  }

  T value() const { return value_; }

 private:
  T const value_;
};

class InstanceOfFeedback
    : public SingleValueFeedback<base::Optional<JSObjectRef>,
                                 ProcessedFeedback::kInstanceOf> {
  using SingleValueFeedback::SingleValueFeedback;
};

class LiteralFeedback
    : public SingleValueFeedback<AllocationSiteRef,
                                 ProcessedFeedback::kLiteral> {
  using SingleValueFeedback::SingleValueFeedback;
};

class RegExpLiteralFeedback
    : public SingleValueFeedback<JSRegExpRef,
                                 ProcessedFeedback::kRegExpLiteral> {
  using SingleValueFeedback::SingleValueFeedback;
};

class TemplateObjectFeedback
    : public SingleValueFeedback<JSArrayRef,
                                 ProcessedFeedback::kTemplateObject> {
  using SingleValueFeedback::SingleValueFeedback;
};

class BinaryOperationFeedback
    : public SingleValueFeedback<BinaryOperationHint,
                                 ProcessedFeedback::kBinaryOperation> {
  using SingleValueFeedback::SingleValueFeedback;
};

class CompareOperationFeedback
    : public SingleValueFeedback<CompareOperationHint,
                                 ProcessedFeedback::kCompareOperation> {
  using SingleValueFeedback::SingleValueFeedback;
};

class ForInFeedback
    : public SingleValueFeedback<ForInHint, ProcessedFeedback::kForIn> {
  using SingleValueFeedback::SingleValueFeedback;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PROCESSED_FEEDBACK_H_
