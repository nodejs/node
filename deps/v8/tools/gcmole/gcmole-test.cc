// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/local-heap.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/managed.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-inl.h"
#include "src/objects/visitors.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// GCMole should not be confused by forward declarations.
class ConservativePinningScope;

// ------- Test simple argument evaluation order problems ---------

void Safepoint() { LocalHeap::Current()->Safepoint(); }

Handle<Object> CauseGC(Handle<Object> obj, Isolate* isolate) {
  isolate->heap()->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

  return obj;
}

Tagged<Object> CauseGCRaw(Tagged<Object> obj, Isolate* isolate) {
  isolate->heap()->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

  return obj;
}

Tagged<Managed<int>> CauseGCManaged(int i, Isolate* isolate) {
  isolate->heap()->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

  return Cast<Managed<int>>(Smi::FromInt(i));
}

void TwoArgumentsFunction(Tagged<Object> a, Tagged<Object> b) {
  Print(a);
  Print(b);
}

void TestTwoArguments(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Handle<JSObject> obj2 = isolate->factory()->NewJSObjectWithNullProto();
  // Should cause warning.
  TwoArgumentsFunction(*CauseGC(obj1, isolate), *CauseGC(obj2, isolate));
}

void TwoSizeTArgumentsFunction(size_t a, size_t b) {
  USE(a);
  USE(b);
}

void TestTwoSizeTArguments(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Handle<JSObject> obj2 = isolate->factory()->NewJSObjectWithNullProto();
  // Should cause warning.
  TwoSizeTArgumentsFunction(sizeof(*CauseGC(obj1, isolate)),
                            sizeof(*CauseGC(obj2, isolate)));
}

// --------- Test problFems with method arguments ----------

class SomeObject : public HeapObject {
 public:
  void Method(Tagged<Object> a) { Print(a); }
};

void TestMethodCall(Isolate* isolate) {
  Tagged<SomeObject> obj;
  Handle<SomeObject> so = handle(obj, isolate);
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  // Should cause warning.
  so->Method(*CauseGC(obj1, isolate));
  // Should cause warning.
  so->Method(CauseGCRaw(*obj1, isolate));
}

void TestOperatorCall(Isolate* isolate) {
  Tagged<SomeObject> obj;
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  // Should not cause warning.
  obj = UncheckedCast<SomeObject>(*CauseGC(obj1, isolate));
}

// --------- Test for templated sub-classes of Object ----------

void TestFollowingTemplates(Isolate* isolate) {
  // Should cause warning.
  CauseGCManaged(42, isolate);
}

// --------- Test for correctly resolving virtual methods ----------

class BaseObject {
 public:
  virtual Handle<Object> VirtualCauseGC(Handle<Object> obj, Isolate* isolate) {
    return obj;
  }
};

class DerivedObject : public BaseObject {
 public:
  Handle<Object> VirtualCauseGC(Handle<Object> obj, Isolate* isolate) override {
    isolate->heap()->CollectGarbage(OLD_SPACE,
                                    GarbageCollectionReason::kTesting);

    return obj;
  }
};

void TestFollowingVirtualFunctions(Isolate* isolate) {
  DerivedObject derived;
  BaseObject* base = &derived;
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();

  Tagged<SomeObject> so;
  Handle<SomeObject> so_handle = handle(so, isolate);
  // Should cause warning.
  so_handle->Method(*derived.VirtualCauseGC(obj1, isolate));
  // Should cause warning.
  so_handle->Method(*base->VirtualCauseGC(obj1, isolate));
}

// --------- Test for correctly resolving static methods ----------

class SomeClass {
 public:
  static Handle<Object> StaticCauseGC(Handle<Object> obj, Isolate* isolate) {
    isolate->heap()->CollectGarbage(OLD_SPACE,
                                    GarbageCollectionReason::kTesting);

    return obj;
  }
};

void TestFollowingStaticFunctions(Isolate* isolate) {
  Tagged<SomeObject> so;
  Handle<SomeObject> so_handle = handle(so, isolate);

  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  // Should cause warning.
  so_handle->Method(*SomeClass::StaticCauseGC(obj1, isolate));
}

// --------- Test basic dead variable analysis ----------

void TestDeadVarAnalysis(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);

  // Should cause warning.
  Print(raw_obj);
}

void TestDeadVarBecauseOfSafepointAnalysis(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  Safepoint();

  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysis(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();

  // Note: having DisableGCMole with the same function as CauseGC
  // normally doesn't make sense, but we want to test whether the guards
  // are recognized by GCMole.
  DisableGCMole no_gc_mole;
  CauseGCRaw(raw_obj, isolate);

  // Shouldn't cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysis2(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();

  // Note: having DisallowGarbageCollection with the same function as CauseGC
  // normally doesn't make sense, but we want to test whether the guards
  // are recognized by GCMole.
  DisallowGarbageCollection no_gc;
  CauseGCRaw(raw_obj, isolate);

  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedAgainstSafepointDeadVarAnalysis(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();

  // Note: having DisableGCMole with the same function as CauseGC
  // normally doesn't make sense, but we want to test whether the guards
  // are recognized by GCMole.
  DisableGCMole no_gc_mole;
  Safepoint();

  // Shouldn't cause warning.
  Print(raw_obj);
}

void TestGuardedAgainstSafepointDeadVarAnalysis2(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();

  // Note: having DisallowGarbageCollection with the same function as CauseGC
  // normally doesn't make sense, but we want to test whether the guards
  // are recognized by GCMole.
  DisallowGarbageCollection no_gc;
  Safepoint();

  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedAgainstSafepointDeadVarAnalysis3(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  // Note: having DisallowGarbageCollection with the same function as CauseGC
  // normally doesn't make sense, but we want to test whether the guards
  // are recognized by GCMole.
  DisallowGarbageCollection no_gc;
  Safepoint();
  // Should cause warning.
  Print(raw_obj);
  {
    DisableGCMole no_gc_mole;
    // Shouldn't cause warning.
    Print(raw_obj);
  }
  // Should cause warning.
  Print(raw_obj);
}

void TestOnlyHeapGuardedDeadVarAnalysisInCompound(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  // {DisallowHeapAccess} has a {DisallowHeapAllocation}, but no
  // {DisallowSafepoints}, so it could see objects move due to safepoints.
  DisallowHeapAccess no_gc;
  CauseGCRaw(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

void TestOnlyHeapGuardedDeadVarAnalysisInCompound2(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  // {DisallowHeapAccess} has a {DisallowHeapAllocation}, but no
  // {DisallowSafepoints}, so it could see objects move due to safepoints.
  DisallowHeapAccess no_gc;
  CauseGCRaw(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
  DisableGCMole no_gc_mole;
  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisNested(Tagged<JSObject> raw_obj,
                                      Isolate* isolate) {
  CauseGCRaw(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisCaller(Isolate* isolate) {
  DisableGCMole no_gc_mole;
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  TestGuardedDeadVarAnalysisNested(raw_obj, isolate);
  // Shouldn't cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisCaller2(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  TestGuardedDeadVarAnalysisNested(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisCaller3(Isolate* isolate) {
  DisallowHeapAccess no_gc;
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  TestGuardedDeadVarAnalysisNested(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisCaller4(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  TestGuardedDeadVarAnalysisNested(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

Tagged<JSObject> GuardedAllocation(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  return *isolate->factory()->NewJSObjectWithNullProto();
}

Tagged<JSObject> GuardedAllocation2(Isolate* isolate) {
  DisableGCMole no_gc_mole;
  return *isolate->factory()->NewJSObjectWithNullProto();
}

void TestNestedDeadVarAnalysis(Isolate* isolate) {
  Tagged<JSObject> raw_obj = GuardedAllocation(isolate);
  CauseGCRaw(raw_obj, isolate);
  // Should cause warning.
  Print(raw_obj);
}

void TestNestedDeadVarAnalysis2(Isolate* isolate) {
  DisableGCMole no_gc_mole;
  Tagged<JSObject> raw_obj = GuardedAllocation(isolate);
  CauseGCRaw(raw_obj, isolate);
  // Shouldn't cause warning.
  Print(raw_obj);
}

// Test that putting a guard in the middle of the function doesn't
// mistakenly cover the whole scope of the raw variable.
void TestGuardedDeadVarAnalysisMidFunction(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  // Guarding the rest of the function from triggering a GC.
  DisallowGarbageCollection no_gc;
  // Should cause warning.
  Print(raw_obj);
}

// Test that putting a guard in the middle of the function doesn't
// mistakenly cover the whole scope of the raw variable.
void TestGuardedDeadVarAnalysisMidFunction2(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  // Guarding the rest of the function from triggering a GC.
  DisableGCMole no_gc_mole;
  // Should cause warning.
  Print(raw_obj);
}

void TestGuardedDeadVarAnalysisMultipleSafepoints(Isolate* isolate) {
  // TODO(https://crbug.com/v8/13536): The analysis points to this safepoint,
  // while it should point to the one below.
  Safepoint();
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  DisallowGarbageCollection no_gc;
  Safepoint();
  Print(raw_obj);
}

void TestVariableScopeInsideIf(Isolate* isolate) {
  Safepoint();
  Tagged<SomeObject> raw_obj;
  if (Tagged<Map> raw_map = raw_obj->map(); !raw_map.is_null()) {
    Print(raw_map);
  }
}

void TestConservativePinningScope(Isolate* isolate) {
  ConservativePinningScope pinning_scope(isolate->heap());
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  Print(raw_obj);
}

void TestConservativePinningScopeConst(Isolate* isolate) {
  const ConservativePinningScope pinning_scope(isolate->heap());
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  Print(raw_obj);
}

void TestConservativePinningScopeWitness(
    Isolate* isolate, ConservativePinningScope& pinning_scope_witness) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  Print(raw_obj);
}

void TestConservativePinningScopeConstWitness(
    Isolate* isolate, const ConservativePinningScope& pinning_scope_witness) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  CauseGCRaw(raw_obj, isolate);
  Print(raw_obj);
}

void OutParameterFunction(Tagged<JSObject>* out_obj, Isolate* isolate) {
  CauseGCRaw(*out_obj, isolate);
  *out_obj = *isolate->factory()->NewJSObjectWithNullProto();
}

void TestOutParameter(Isolate* isolate) {
  Tagged<JSObject> raw_obj;
  OutParameterFunction(&raw_obj, isolate);
}

class JSDispatchHandleMember;
class CppHeapPointerMember;
template <typename T, typename CompressionScheme>
class TaggedMember;

JSDispatchHandleMember* ReturnRawJSDispatchHandleMember(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return nullptr;
}

JSDispatchHandle ReturnRawJSDispatchHandle(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return kNullJSDispatchHandle;
}

ExternalPointerHandle ReturnRawExternalPointerHandle(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return kNullExternalPointerHandle;
}

void DummyTakePointers(JSDispatchHandleMember* a, JSDispatchHandleMember* b) {}
void DummyTakeHandles(JSDispatchHandle a, JSDispatchHandle b) {}
void DummyTakeExternalPointerHandles(ExternalPointerHandle a,
                                     ExternalPointerHandle b) {}

void TestJSDispatchHandleMemberEvalOrder(Isolate* isolate) {
  // Should cause warning.
  DummyTakePointers(ReturnRawJSDispatchHandleMember(isolate),
                    ReturnRawJSDispatchHandleMember(isolate));
}

void TestJSDispatchHandleEvalOrder(Isolate* isolate) {
  // Should cause warning.
  DummyTakeHandles(ReturnRawJSDispatchHandle(isolate),
                   ReturnRawJSDispatchHandle(isolate));
}

void TestExternalPointerHandleEvalOrder(Isolate* isolate) {
  // Should cause warning.
  DummyTakeExternalPointerHandles(ReturnRawExternalPointerHandle(isolate),
                                  ReturnRawExternalPointerHandle(isolate));
}

void TestTaggedMemberDeadVar(TaggedMember<Object, void> raw_member,
                             Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestJSDispatchHandleMemberDeadVar(JSDispatchHandleMember* raw_member,
                                       Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestJSDispatchHandleDeadVar(JSDispatchHandle raw_handle,
                                 Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_handle);
}

void TestExternalPointerHandleDeadVar(ExternalPointerHandle raw_handle,
                                      Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_handle);
}

void TestCppHeapPointerHandleDeadVar(CppHeapPointerHandle raw_handle,
                                     Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_handle);
}

void TestTrustedPointerHandleDeadVar(TrustedPointerHandle raw_handle,
                                     Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_handle);
}

void TestExternalPointerSlotDeadVar(ExternalPointerSlot raw_slot,
                                    Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_slot);
}

void TestCppHeapPointerSlotDeadVar(CppHeapPointerSlot raw_slot,
                                   Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_slot);
}

void TestExternalPointerMemberDeadVar(
    ExternalPointerMember<kAnyExternalPointerTagRange>* raw_member,
    Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestTrustedPointerMemberDeadVar(
    TrustedPointerMember<ExposedTrustedObject, kAllTrustedPointerTags>*
        raw_member,
    Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestCppHeapPointerMemberDeadVar(CppHeapPointerMember* raw_member,
                                     Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestTableHandleSafePatterns(ExternalPointerHandle raw_handle,
                                 ExternalPointerHandle* out_handle,
                                 JSDispatchHandle* out_dispatch_handle,
                                 Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Reassigning a table handle after GC should make it live again.
  raw_handle = kNullExternalPointerHandle;
  USE(raw_handle);
  // Global constexpr handle constants and Handle* out-parameters should not
  // warn.
  USE(kNullExternalPointerHandle);
  USE(kNullJSDispatchHandle);
  USE(out_handle);
  USE(out_dispatch_handle);
}

void TestTaggedMemberPtrDeadVar(TaggedMember<Object, void>* raw_member,
                                Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_member);
}

void TestHeapObjectPtrDeadVar(HeapObject* raw_obj, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_obj);
}

[[noreturn]] void NoReturnCauseGC(Isolate* isolate) {
  CauseGCRaw(Tagged<Object>(), isolate);
  // Infinite loop to satisfy [[noreturn]] without extra headers
  while (true) {
  }
}

void TestNoReturnGC(Isolate* isolate) {
  Tagged<JSObject> raw_obj = *isolate->factory()->NewJSObjectWithNullProto();
  NoReturnCauseGC(isolate);
  Print(raw_obj);
}

class TestVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                     ObjectSlot end) override {
    isolate_->heap()->CollectGarbage(OLD_SPACE,
                                     GarbageCollectionReason::kTesting);
  }
  void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {}
  void VisitInstructionStreamPointer(Tagged<Code> host,
                                     InstructionStreamSlot slot) override {}

  TestVisitor(Isolate* isolate) : isolate_(isolate) {}

 private:
  Isolate* isolate_;
};

void TestVisitorVisitor(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<JSObject> raw_obj = *obj1;

  TestVisitor visitor(isolate);
  Address addr = reinterpret_cast<Address>(&raw_obj);
  ObjectSlot slot(addr);

  visitor.VisitPointer(raw_obj, slot);

  Print(raw_obj);
}

void TestBaseVisitorVisitor(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<JSObject> raw_obj = *obj1;

  TestVisitor visitor(isolate);
  ObjectVisitor* base_visitor = &visitor;
  Address addr = reinterpret_cast<Address>(&raw_obj);
  ObjectSlot slot(addr);

  base_visitor->VisitPointer(raw_obj, slot);

  Print(raw_obj);
}

class SafeVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                     ObjectSlot end) override {}
  void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {}
  void VisitInstructionStreamPointer(Tagged<Code> host,
                                     InstructionStreamSlot slot) override {}
};

void TestSafeVisitorVisitor(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<JSObject> raw_obj = *obj1;

  SafeVisitor visitor;
  Address addr = reinterpret_cast<Address>(&raw_obj);
  ObjectSlot slot(addr);

  visitor.VisitPointer(raw_obj, slot);

  Print(raw_obj);
}

void TestBaseVisitorPolymorphic(ObjectVisitor* base_visitor, Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<JSObject> raw_obj = *obj1;
  Address addr = reinterpret_cast<Address>(&raw_obj);
  ObjectSlot slot(addr);
  // Should cause warning polymorphically because TestVisitor::VisitPointers
  // causes GC!
  base_visitor->VisitPointer(raw_obj, slot);
  Print(raw_obj);
}

class SubTestVisitor : public TestVisitor {
 public:
  SubTestVisitor(Isolate* isolate) : TestVisitor(isolate) {}
};

void TestSubVisitorNonOverride(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<JSObject> raw_obj = *obj1;
  SubTestVisitor visitor(isolate);
  Address addr = reinterpret_cast<Address>(&raw_obj);
  ObjectSlot slot(addr);
  // Should cause warning because parent TestVisitor::VisitPointers causes GC!
  visitor.VisitPointer(raw_obj, slot);
  Print(raw_obj);
}

// ------- Test expression sequencing and statement AST traversal -------

void TestConditionalAndShortCircuitSequencing(Isolate* isolate, bool cond) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();

  Tagged<JSObject> raw_cond = *obj1;
  int v = (CauseGCRaw(raw_cond, isolate), cond) ? 1 : 2;
  USE(v);
  // Should cause warning: GC in `?:` condition kills `raw_cond`.
  Print(raw_cond);

  Tagged<JSObject> raw_and = *obj1;
  // Should cause warning: GC in `&&` LHS kills `raw_and` before RHS executes.
  bool b_and = (CauseGCRaw(raw_and, isolate), true) && (Print(raw_and), true);
  USE(b_and);

  Tagged<JSObject> raw_or = *obj1;
  // Should cause warning: GC in `||` LHS kills `raw_or` before RHS executes.
  bool b_or = (CauseGCRaw(raw_or, isolate), false) || (Print(raw_or), true);
  USE(b_or);
}

struct RawObjectPair {
  Tagged<Object> first;
  Tagged<Object> second;
};

struct ConstructedRawObjectPair {
  ConstructedRawObjectPair(Tagged<Object> a, Tagged<Object> b)
      : first(a), second(b) {}
  Tagged<Object> first;
  Tagged<Object> second;
};

struct ConstructedIntPair {
  ConstructedIntPair(int a, int b) {
    USE(a);
    USE(b);
  }
};

void TestSequencedInitLists(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Handle<JSObject> obj2 = isolate->factory()->NewJSObjectWithNullProto();

  // Safe: brace-enclosed initializer lists evaluate left-to-right, so the GC
  // in the first element runs before `*obj2` is dereferenced in the second.
  RawObjectPair safe_agg{*CauseGC(obj1, isolate), *obj2};
  ConstructedRawObjectPair safe_ctor{*CauseGC(obj1, isolate), *obj2};
  USE(safe_agg);
  USE(safe_ctor);

  // Should cause warning: `*obj1` in the first element is unrooted across the
  // GC in the second element.
  RawObjectPair unsafe_agg{*obj1, *CauseGC(obj2, isolate)};
  // Should cause warning: list-initialized constructor also evaluates
  // left-to-right, so a GC in the first element kills `raw_for_ctor` before the
  // second element uses it.
  Tagged<JSObject> raw_for_ctor = *obj1;
  ConstructedIntPair unsafe_ctor{(CauseGCRaw(raw_for_ctor, isolate), 1),
                                 (Print(raw_for_ctor), 2)};
  USE(unsafe_agg);
  USE(unsafe_ctor);
}

class SomeObjectWithCompoundAssign : public HeapObject {
 public:
  SomeObjectWithCompoundAssign& operator+=(int offset) {
    USE(offset);
    return *this;
  }
};

void TestOverloadedCompoundAssignment(SomeObjectWithCompoundAssign raw_val,
                                      Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning: overloaded `operator+=` (`OO_PlusEqual`) mutates
  // `raw_val` in place rather than redefining it like `operator=` (`OO_Equal`).
  raw_val += 1;
}

struct StructWithDefaultInitGC {
  Isolate* isolate;
  Tagged<Managed<int>> member = CauseGCManaged(42, isolate);
};

void TestRangeForAndStmtWrappers(Isolate* isolate, bool cond) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();

  Tagged<JSObject> raw_range = *obj1;
  int indices[2] = {0, 1};
  for (int idx : indices) {
    USE(idx);
    // Should cause warning on the second loop iteration after `CauseGCRaw`.
    Print(raw_range);
    CauseGCRaw(raw_range, isolate);
  }

  Tagged<JSObject> raw_label = *obj1;
  CauseGCRaw(raw_label, isolate);
my_label:
  // Should cause warning inside `LabelStmt`.
  Print(raw_label);
  if (!cond) goto my_label;

  Tagged<JSObject> raw_attr = *obj1;
  CauseGCRaw(raw_attr, isolate);
  if (cond) [[likely]] {
    // Should cause warning inside `AttributedStmt`.
    Print(raw_attr);
  }

  Tagged<JSObject> raw_default_init = *obj1;
  StructWithDefaultInitGC s{isolate};
  USE(s);
  // Should cause warning: `StructWithDefaultInitGC`'s default member
  // initializer (`CXXDefaultInitExpr`) triggered a GC.
  Print(raw_default_init);
}

// ------- Test raw member addresses and references across GCs -------

class SomeObjectWithMembers : public HeapObject {
 public:
  int (&values())[2] { return values_; }
  ObjectSlot RawField(int offset) {
    return ObjectSlot(reinterpret_cast<Address>(this) + offset);
  }

  int primitive_field_;
  int* ptr_field_;
  ObjectSlot slot_field_;
  void (*fn_field_)();
  uint8_t inline_bytes_[4];
  TaggedMember<Object> tagged_field_;
  ExternalPointerMember<kAnyForeignExternalPointerTagRange> ext_field_;
  UnalignedValueMember<double> unaligned_field_;
  struct Nested {
    void MethodThatGCs(Isolate* isolate) { CauseGCManaged(42, isolate); }
    static void StaticMethodThatGCs(Isolate* isolate) {
      CauseGCManaged(42, isolate);
    }
    static void StaticMethodNoGC() {}
    void NoOp() {}
    void operator+=(Isolate* isolate) { CauseGCManaged(42, isolate); }

    int inner_field_;
  } nested_;
  struct PtrBox {
    int* ptr;
    int count;
  } ptr_box_field_;
  void (Nested::*pmf_field_)();
  int values_[2];
};

int CauseGCInt(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return 7;
}

void TakeIntPtrAndObj(int* p, Tagged<Object> obj) {
  USE(p);
  Print(obj);
}

void TakeIntAndObj(int v, Tagged<Object> obj) {
  USE(v);
  Print(obj);
}

void TestRawMemberAddressesAcrossGC(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  int* p_field = &obj->primitive_field_;
  int* p_addressof = std::addressof(obj->primitive_field_);
  Address ext_addr = obj->ext_field_.storage_address();
  double* unaligned_addr = obj->unaligned_field_.address();
  int* p_nested = &obj->nested_.inner_field_;

  CauseGCManaged(42, isolate);

  // Should cause warnings.
  USE(p_field);
  USE(p_addressof);
  USE(ext_addr);
  USE(unaligned_addr);
  USE(p_nested);
}

void TestRawMemberArraysAndSubscriptsAcrossGC(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  uint8_t* decayed_bytes = obj->inline_bytes_;
  int* decayed_values = obj->values();
  uint8_t* elem_ptr = &obj->inline_bytes_[1];
  int* ptr_array[2] = {&obj->primitive_field_, nullptr};
  int* from_local_array = ptr_array[0];

  CauseGCManaged(42, isolate);

  // Should cause warnings.
  USE(decayed_bytes);
  USE(decayed_values);
  USE(elem_ptr);
  USE(from_local_array);

  // Range-based `for` over an on-heap array binds the synthetic `__begin`
  // iterator to a raw interior pointer, which becomes stale across GC.
  // Deduplication suppresses repeated fixpoint warnings on the loop header.
  for (uint8_t b : obj->inline_bytes_) {
    USE(b);
    CauseGCManaged(42, isolate);
  }
}

void TestRawMemberReferencesAcrossGC(Isolate* isolate,
                                     TaggedMember<Object>& member_ref_param) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  int& ref_read = obj->primitive_field_;
  int& ref_write = obj->primitive_field_;
  int& ref_for_addr = obj->primitive_field_;
  int* ptr_from_ref = &ref_for_addr;

  CauseGCManaged(42, isolate);

  // Should cause warnings (including write-through assignment `ref_write = 1`).
  USE(ref_read);
  ref_write = 1;
  USE(ptr_from_ref);
  USE(member_ref_param);
}

void TestRawMemberArithmeticCastsAndRebinding(Isolate* isolate, bool cond) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  int stack_int = 0;

  int* p_arith = &obj->primitive_field_ + 1;
  uint8_t* p_reinterp = reinterpret_cast<uint8_t*>(&obj->primitive_field_);
  uintptr_t addr_bitcast =
      base::bit_cast<uintptr_t>(obj->ext_field_.storage_address());
  int* p_rebound_to_heap = &stack_int;
  p_rebound_to_heap = &obj->primitive_field_;

  // Safe patterns that must not warn after GC:
  int* p_rebound_to_stack = &obj->primitive_field_;
  p_rebound_to_stack = &stack_int;
  ptrdiff_t ptr_diff = (&obj->primitive_field_ + 1) - &obj->primitive_field_;
  Address addr_diff =
      obj->ext_field_.storage_address() - obj->ext_field_.storage_address();
  uint16_t narrowed_addr =
      static_cast<uint16_t>(obj->ext_field_.storage_address());

  CauseGCManaged(42, isolate);

  // Should cause warnings.
  USE(p_arith);
  USE(p_reinterp);
  USE(addr_bitcast);
  USE(p_rebound_to_heap);

  // Should not cause warnings.
  USE(p_rebound_to_stack);
  USE(ptr_diff);
  USE(addr_diff);
  USE(narrowed_addr);

  // Safe one-sided branch initialization (`CanRescueOneSidedLive`): `p_branch`
  // is still `nullptr` on the GC branch and only receives a member address on
  // the non-GC branch.
  int* p_branch = nullptr;
  if (cond) {
    CauseGCManaged(42, isolate);
  } else {
    p_branch = &obj->primitive_field_;
  }
  USE(p_branch);
}

void TestRawMemberEvalOrder(Isolate* isolate, bool cond) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();

  // Each case uses fresh live bindings in its own branch so earlier GCs do not
  // mask the evaluation-order check behind a stale-variable warning.
  if (cond) {
    int* p = &obj->primitive_field_;
    // Should cause warning: unsequenced raw member pointer `p` and `CauseGC`.
    TakeIntPtrAndObj(p, *CauseGC(obj1, isolate));
  } else {
    int& ref = obj->primitive_field_;
    // Should cause warning: unsequenced by-value load through member reference
    // `ref` and `CauseGC`.
    TakeIntAndObj(ref, *CauseGC(obj1, isolate));
  }

  if (cond) {
    int& ref = obj->primitive_field_;
    // Should cause warning: unsequenced member reference LHS and GC-causing
    // RHS.
    ref = CauseGCInt(isolate);
  } else {
    int* p = &obj->primitive_field_;
    int* p_sibling = nullptr;
    // Should cause evaluation-order warning on the call AND mark `p_sibling`
    // stale for the subsequent `USE(p_sibling)`.
    TakeIntPtrAndObj(p_sibling = p, *CauseGC(obj1, isolate));
    USE(p_sibling);
  }
}

// ------- Test SlotBase wrappers and proxy references -------

void TestSlotBaseAndProxyReferences(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Tagged<Object> stack_tagged;

  ObjectSlot slot_from_member(&obj->tagged_field_);
  ObjectSlot slot_from_method = obj->RawField(0);
  Address addr_from_slot = obj->RawField(0).address();
  ObjectSlot slot_rebound_to_heap(&stack_tagged);
  slot_rebound_to_heap = obj->RawField(0);

  UnalignedSlot<int> unaligned_slot(&obj->primitive_field_);
  auto proxy_ref = unaligned_slot[0];

  // Safe stack-backed slots (should not warn after GC):
  FullObjectSlot safe_stack_slot(&stack_tagged);
  ObjectSlot slot_rebound_to_stack = obj->RawField(0);
  slot_rebound_to_stack = ObjectSlot(&stack_tagged);

  CauseGCManaged(42, isolate);

  // Should cause warnings.
  USE(slot_from_member);
  USE(slot_from_method);
  USE(addr_from_slot);
  USE(slot_rebound_to_heap);
  // Proxy reference `operator=` writes through the stale slot address.
  proxy_ref = 42;

  // Should not cause warnings.
  USE(safe_stack_slot);
  USE(slot_rebound_to_stack);
}

void InitTaggedMemberPtr(TaggedMember<Object>** out) { USE(out); }

void TestAlwaysOnHeapSlotsAndTypeTrackedJoins(
    ExternalPointerSlot ext_slot_param,
    const EmbedderDataSlot& embedder_slot_param,
    Handle<NativeContext> native_context, bool cond, Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();

  // `EmbedderDataSlot` is always on-heap: both the slot and addresses derived
  // from it (constructed or passed by const-ref parameter) become stale across
  // GC.
  EmbedderDataSlot slot_from_obj(*js_obj, 0);
  EmbedderDataSlot slot_from_context(*native_context, 0);
  Address addr_from_embedder_slot = slot_from_obj.address();
  Address addr_from_param_slot = embedder_slot_param.address();

  // Default-constructed always-on-heap slots and out-param-initialized
  // `*Member*` pointers have no `raw_member_vars_` provenance before GC, so
  // reassigning them on only one branch must not rescue their liveness from
  // the other branch (`CanRescueOneSidedLive`).
  EmbedderDataSlot default_embedder_slot;
  TaggedMember<Object>* member_ptr = nullptr;
  InitTaggedMemberPtr(&member_ptr);

  CauseGCManaged(42, isolate);

  if (cond) {
    ext_slot_param = ExternalPointerSlot(&obj->ext_field_);
    default_embedder_slot = EmbedderDataSlot(*js_obj, 0);
    member_ptr = &obj->tagged_field_;
  }

  // Should cause warnings.
  USE(slot_from_obj);
  USE(slot_from_context);
  USE(addr_from_embedder_slot);
  USE(embedder_slot_param);
  USE(addr_from_param_slot);
  USE(ext_slot_param);
  USE(default_embedder_slot);
  USE(member_ptr);
}

// ------- Test escaped member arguments and callee summaries -------

void UnsafeCalleeUsesPtrAfterGC(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  *p = 1;
}

void UnsafeCalleeUsesRefAfterGC(int& r, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  r = 1;
}

void UnsafeCalleeUsesSlotAfterGC(ObjectSlot slot, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(slot.address());
}

void UnsafeCalleeUsesEmbedderSlotAfterGC(EmbedderDataSlot slot,
                                         Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(slot.address());
}

void UnsafeWrapperCallee(int* p, Isolate* isolate) {
  UnsafeCalleeUsesPtrAfterGC(p, isolate);
}

void RecursiveCalleeWithGC(int* p, int depth, Isolate* isolate) {
  if (depth > 0) RecursiveCalleeWithGC(p, depth - 1, isolate);
  CauseGCManaged(42, isolate);
}

void SafeCalleeReadsPtrBeforeGC(const int* p, Isolate* isolate) {
  int val = *p;
  USE(val);
  CauseGCManaged(42, isolate);
}

void SafeCalleeReadsRefBeforeGC(const int& r, Isolate* isolate) {
  int val = r;
  USE(val);
  CauseGCManaged(42, isolate);
}

void SafeCalleeReadsSlotBeforeGC(ObjectSlot slot, Isolate* isolate) {
  Address addr = slot.address();
  USE(addr);
  CauseGCManaged(42, isolate);
}

void SafeCalleeReadsEmbedderSlotBeforeGC(const EmbedderDataSlot& slot,
                                         Isolate* isolate) {
  Address addr = slot.address();
  USE(addr);
  CauseGCManaged(42, isolate);
}

void SafeWrapperCallee(const int* p, Isolate* isolate) {
  SafeCalleeReadsPtrBeforeGC(p, isolate);
}

void UnsafeCalleeAliasesPtr(int* p, Isolate* isolate) {
  int* q = p;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeSubfieldRef(SomeObjectWithMembers::Nested& n,
                             Isolate* isolate) {
  int* q = &n.inner_field_;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeRebindsPtr(int* p, Isolate* isolate) {
  int local = 0;
  p = &local;
  CauseGCManaged(42, isolate);
  *p = 1;
}

void UnsafeCalleeUsesProxyRef(UnalignedSlot<int>::Reference ref,
                              Isolate* isolate) {
  CauseGCManaged(42, isolate);
  ref = 1;
}

void SafeCalleeReadsProxyRefBeforeGC(UnalignedSlot<int>::Reference ref,
                                     Isolate* isolate) {
  int val = ref;
  USE(val);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeEvalOrder(int* p, Isolate* isolate) {
  TakeIntAndObj(*p, CauseGCManaged(42, isolate));
}

void SafeCalleeWithUnrelatedStaleTagged(const int* p, Isolate* isolate) {
  int val = *p;
  USE(val);
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Tagged<Object> unrelated_raw = *obj1;
  CauseGCManaged(42, isolate);
  // Should cause warning on `unrelated_raw` here, but must NOT blame callers
  // passing a member pointer as `p`.
  Print(unrelated_raw);
}

void SafeCalleeWithUnrelatedStaleMemberAndSlot(const int* p, Isolate* isolate) {
  int val = *p;
  USE(val);
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> other = handle(raw_so, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();
  int* unrelated_member = &other->primitive_field_;
  ObjectSlot unrelated_obj_slot = other->RawField(0);
  EmbedderDataSlot unrelated_slot(*js_obj, 0);
  CauseGCManaged(42, isolate);
  // Should warn on `unrelated_member`, `unrelated_obj_slot`, and
  // `unrelated_slot` here, but must NOT blame callers passing a member pointer
  // as `p`.
  USE(unrelated_member);
  USE(unrelated_obj_slot);
  USE(unrelated_slot);
}

void UnsafeCalleeRebindsDualProvenancePtrRef(int*& r, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  int local = 0;
  r = &local;
}

void UnsafeCalleeDualProvenanceRebindThenDeref(int*& r, Isolate* isolate) {
  int local = 0;
  r = &local;
  CauseGCManaged(42, isolate);
  *r = 1;
}

void UnsafeCalleeRebindsDualProvenanceSlotRef(ObjectSlot& s, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  s = ObjectSlot();
}

void SafeCalleeReadsDualProvenancePtrRef(int*& r, Isolate* isolate) {
  int* val = r;
  USE(val);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeDualProvenanceCopyPtr(int*& r, Isolate* isolate) {
  int* q = r;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeDualProvenanceDerefRef(int*& r, Isolate* isolate) {
  int& x = *r;
  CauseGCManaged(42, isolate);
  x = 1;
}

void UnsafeCalleeDualProvenancePostInc(int*& r, Isolate* isolate) {
  int* q = r++;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeDualProvenanceForwardPtr(int*& r, Isolate* isolate) {
  UnsafeCalleeUsesPtrAfterGC(r, isolate);
}

void UnsafeCalleeStdForwardDualProvenance(int*& r, Isolate* isolate) {
  int* q = std::forward<int*&>(r);
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeStdMoveDualProvenance(int*& r, Isolate* isolate) {
  int* q = std::move(r);
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeIfCondVarRead(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  if (int x = *p) {
    USE(x);
  }
}

void UnsafeCalleeIfCondVarAlias(int* p, Isolate* isolate) {
  if (int* q = p) {
    CauseGCManaged(42, isolate);
    *q = 1;
  }
}

void UnsafeCalleeWhileCondVar(int* p, Isolate* isolate) {
  while (int x = *p) {
    USE(x);
    CauseGCManaged(42, isolate);
  }
}

void UnsafeCalleeForCondVar(int* p, Isolate* isolate) {
  for (; int x = *p;) {
    USE(x);
    CauseGCManaged(42, isolate);
  }
}

void UnsafeCalleeSwitchCondVar(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  switch (int x = *p) {
    default:
      USE(x);
      break;
  }
}

void UnsafeCalleeSwitchGCInCond(int* p, Isolate* isolate) {
  switch (CauseGCInt(isolate)) {
    case 1:
      break;
    case 2:
      *p = 1;
      break;
  }
}

void UnsafeCalleeSwitchInitAlias(int* p, int k, Isolate* isolate) {
  switch (int* q = p; k) {
    default:
      CauseGCManaged(42, isolate);
      *q = 1;
      break;
  }
}

void UnsafeCalleeSwitchNoDefaultFallPast(int* p, int k, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  switch (k) {
    case 1:
      return;
  }
  *p = 1;
}

void SafeCalleeIfCondVarBeforeGC(const int* p, Isolate* isolate) {
  if (int x = *p) {
    USE(x);
  }
  CauseGCManaged(42, isolate);
}

void UnsafeCalleePlacementNew(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  new (p) int(42);
}

void UnsafeCalleePlacementNewReturn(int* p, Isolate* isolate) {
  int* q = new (p) int(42);
  CauseGCManaged(42, isolate);
  *q = 99;
}

void UnsafeCalleeNewArraySize(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  int* arr = new int[*p];
  delete[] arr;
}

void UnsafeCalleeIndirectCall(void (**fn_ptr)(), Isolate* isolate) {
  CauseGCManaged(42, isolate);
  (*fn_ptr)();
}

void UnsafeCalleePtrToMemberRHS(
    void (SomeObjectWithMembers::Nested::** pmf_ptr)(), Isolate* isolate) {
  SomeObjectWithMembers::Nested target{0};
  CauseGCManaged(42, isolate);
  (target.*(*pmf_ptr))();
}

void UnsafeCalleeStaticMethodStaleBase(SomeObjectWithMembers::Nested* p,
                                       Isolate* isolate) {
  CauseGCManaged(42, isolate);
  p->StaticMethodThatGCs(isolate);
}

void UnsafeCalleeStaticMethodBaseGC(int* p, Isolate* isolate) {
  SomeObjectWithMembers::Nested n{0};
  (CauseGCManaged(42, isolate), n).StaticMethodNoGC();
  *p = 1;
}

template <typename T>
void UnsafeCalleePseudoDestructor(T* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  p->~T();
}

void UnsafeCalleeStructuredBindingRef(SomeObjectWithMembers::Nested& n,
                                      Isolate* isolate) {
  auto& [inner] = n;
  CauseGCManaged(42, isolate);
  inner = 1;
}

void SafeCalleeStructuredBindingCopy(SomeObjectWithMembers::Nested& n,
                                     Isolate* isolate) {
  auto [inner] = n;
  CauseGCManaged(42, isolate);
  USE(inner);
}

void UnsafeCalleeRefAggregate(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  CauseGCManaged(42, isolate);
  box.ref = 1;
}

void UnsafeCalleeRefAggregateAlias(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  int& alias = box.ref;
  CauseGCManaged(42, isolate);
  alias = 1;
}

void UnsafeCalleeRefAggregateAddrOf(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  int* p = &box.ref;
  CauseGCManaged(42, isolate);
  *p = 1;
}

void UnsafeCalleeRefAggregateForward(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  UnsafeCalleeUsesRefAfterGC(box.ref, isolate);
}

void UnsafeCalleeRefAggregateCopy(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  RefBox box2 = box;
  CauseGCManaged(42, isolate);
  box2.ref = 1;
}

void SafeCalleeRefAggregateReadsValueBeforeGC(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  int v = box.ref;
  CauseGCManaged(42, isolate);
  USE(v);
}

void SafeCalleeTakesIntByValue(int v, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(v);
}

void SafeCalleeRefAggregatePassesIntByValue(int& r, Isolate* isolate) {
  struct RefBox {
    int& ref;
  };
  RefBox box{r};
  SafeCalleeTakesIntByValue(box.ref, isolate);
}

void SafeCalleePtrAndIntReadsIntBeforeGC(int* p, Isolate* isolate) {
  struct PtrAndInt {
    int* p;
    int count;
  };
  PtrAndInt box{p, 5};
  int n = box.count;
  CauseGCManaged(42, isolate);
  USE(n);
}

void UnsafeCalleePtrAndIntUsesPtrAfterGC(int* p, Isolate* isolate) {
  struct PtrAndInt {
    int* p;
    int count;
  };
  PtrAndInt box{p, 5};
  int* q = box.p;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeCopiesNestedByValue(SomeObjectWithMembers::Nested& n,
                                   Isolate* isolate) {
  SomeObjectWithMembers::Nested copy = n;
  CauseGCManaged(42, isolate);
  USE(copy.inner_field_);
}

void UnsafeCalleeCallsSafeHelperThenUsesPtr(int* p, Isolate* isolate) {
  SafeCalleeReadsPtrBeforeGC(p, isolate);
  *p = 1;
}

void UnsafeCalleeCallsSafeHelperInLoop(const int* p, int n, Isolate* isolate) {
  for (int i = 0; i < n; ++i) {
    SafeCalleeReadsPtrBeforeGC(p, isolate);
  }
}

void ForwardDeclaredSafeCallee(const int* p, Isolate* isolate);

void CalleeWithLambda(const int* p, Isolate* isolate) {
  auto fn = [&]() { return *p; };
  CauseGCManaged(42, isolate);
  USE(fn);
}

void CalleeWithBackwardGoto(const int* p, bool cond, Isolate* isolate) {
retry:
  int v = *p;
  USE(v);
  if (cond) {
    cond = false;
    CauseGCManaged(42, isolate);
    goto retry;
  }
}

void SafeCalleeWithDesignatedInit(const int* p, Isolate* isolate) {
  struct Pair {
    int a;
    int b;
  };
  // Omitted field `.a` produces an `ImplicitValueInitExpr` inside
  // `InitListExpr`.
  Pair pair{.b = *p};
  USE(pair);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeTwoPtrs(int* a, int* b, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  *a = 1;
  *b = 2;
}

// Body-less forward declaration matching `GC_PATTERN` so it enters
// `gcsuspects` without an available definition in this TU.
void ExternalCollectGarbage(int* p);

void SafeVariadicCallee(const int* p, Isolate* isolate, ...) {
  int val = *p;
  USE(val);
  CauseGCManaged(42, isolate);
}

void TestEscapedMemberArgsAndCalleeSummaries(Isolate* isolate, bool cond) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();

  // Should cause warnings: callee accesses the member pointer, reference, or
  // slot after triggering a GC (or is recursive / variadic / body-less /
  // unmodeled).
  UnsafeCalleeUsesPtrAfterGC(&obj->primitive_field_, isolate);
  UnsafeCalleeUsesRefAfterGC(obj->primitive_field_, isolate);
  UnsafeCalleeUsesSlotAfterGC(obj->RawField(0), isolate);
  UnsafeCalleeUsesEmbedderSlotAfterGC(EmbedderDataSlot(*js_obj, 0), isolate);
  UnsafeWrapperCallee(&obj->primitive_field_, isolate);
  RecursiveCalleeWithGC(&obj->primitive_field_, 1, isolate);
  UnsafeCalleeAliasesPtr(&obj->primitive_field_, isolate);
  UnsafeCalleeSubfieldRef(obj->nested_, isolate);
  UnsafeCalleeUsesProxyRef(UnalignedSlot<int>(&obj->primitive_field_)[0],
                           isolate);
  UnsafeCalleeEvalOrder(&obj->primitive_field_, isolate);
  CalleeWithLambda(&obj->primitive_field_, isolate);
  CalleeWithBackwardGoto(&obj->primitive_field_, cond, isolate);
  ExternalCollectGarbage(&obj->primitive_field_);
  SafeVariadicCallee(&obj->primitive_field_, isolate, 1);
  // Both unsafe escaped arguments should be reported at top-level call sites.
  UnsafeCalleeTwoPtrs(&obj->primitive_field_, &obj->nested_.inner_field_,
                      isolate);

  // Dual provenance (`kRawMemberAddress` + `kOnHeapMemberLValue`) on reference
  // parameters must track both bits across writes, by-value loads, derefs,
  // postfix increments, and `std::forward`/`std::move`.
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeRebindsDualProvenancePtrRef(cond ? stack_ptr : obj->ptr_field_,
                                            isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeDualProvenanceRebindThenDeref(
        cond ? stack_ptr : obj->ptr_field_, isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeDualProvenanceCopyPtr(cond ? stack_ptr : obj->ptr_field_,
                                      isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeDualProvenanceDerefRef(cond ? stack_ptr : obj->ptr_field_,
                                       isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeDualProvenancePostInc(cond ? stack_ptr : obj->ptr_field_,
                                      isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeDualProvenanceForwardPtr(cond ? stack_ptr : obj->ptr_field_,
                                         isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeStdForwardDualProvenance(cond ? stack_ptr : obj->ptr_field_,
                                         isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeStdMoveDualProvenance(cond ? stack_ptr : obj->ptr_field_,
                                      isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    UnsafeCalleeUsesPtrAfterGC(cond ? stack_ptr : obj->ptr_field_, isolate);
  }
  {
    int* stack_ptr = &obj->primitive_field_;
    int* loaded = (cond ? stack_ptr : obj->ptr_field_);
    CauseGCManaged(42, isolate);
    *loaded = 1;
  }
  {
    ObjectSlot local_slot = obj->RawField(0);
    UnsafeCalleeRebindsDualProvenanceSlotRef(
        cond ? local_slot : obj->slot_field_, isolate);
  }

  // Condition-variable declarations, `switch` init/cond/fall-past edges, `new`
  // placement/return/array size, indirect & pointer-to-member calls, static
  // method base expressions, pseudo-destructors, structured bindings, reference
  // aggregates, and post-helper uses inside callees must all be caught.
  UnsafeCalleeIfCondVarRead(&obj->primitive_field_, isolate);
  UnsafeCalleeIfCondVarAlias(&obj->primitive_field_, isolate);
  UnsafeCalleeWhileCondVar(&obj->primitive_field_, isolate);
  UnsafeCalleeForCondVar(&obj->primitive_field_, isolate);
  UnsafeCalleeSwitchCondVar(&obj->primitive_field_, isolate);
  UnsafeCalleeSwitchGCInCond(&obj->primitive_field_, isolate);
  UnsafeCalleeSwitchInitAlias(&obj->primitive_field_, 0, isolate);
  UnsafeCalleeSwitchNoDefaultFallPast(&obj->primitive_field_, 0, isolate);
  UnsafeCalleePlacementNew(&obj->primitive_field_, isolate);
  UnsafeCalleePlacementNewReturn(&obj->primitive_field_, isolate);
  UnsafeCalleeNewArraySize(&obj->primitive_field_, isolate);
  UnsafeCalleeIndirectCall(&obj->fn_field_, isolate);
  UnsafeCalleePtrToMemberRHS(&obj->pmf_field_, isolate);
  UnsafeCalleeStaticMethodStaleBase(&obj->nested_, isolate);
  UnsafeCalleeStaticMethodBaseGC(&obj->primitive_field_, isolate);
  UnsafeCalleePseudoDestructor(&obj->primitive_field_, isolate);
  UnsafeCalleeStructuredBindingRef(obj->nested_, isolate);
  UnsafeCalleeRefAggregate(obj->primitive_field_, isolate);
  UnsafeCalleeRefAggregateAlias(obj->primitive_field_, isolate);
  UnsafeCalleeRefAggregateAddrOf(obj->primitive_field_, isolate);
  UnsafeCalleeRefAggregateForward(obj->primitive_field_, isolate);
  UnsafeCalleeRefAggregateCopy(obj->primitive_field_, isolate);
  UnsafeCalleePtrAndIntUsesPtrAfterGC(&obj->primitive_field_, isolate);
  UnsafeCalleeCallsSafeHelperThenUsesPtr(&obj->primitive_field_, isolate);
  UnsafeCalleeCallsSafeHelperInLoop(&obj->primitive_field_, 2, isolate);

  // Should not cause warnings: callee only reads the member before its GC,
  // rebinds the parameter before GC, takes a by-value copy, or only has an
  // unrelated stale variable.
  SafeCalleeReadsPtrBeforeGC(&obj->primitive_field_, isolate);
  SafeCalleeReadsRefBeforeGC(obj->primitive_field_, isolate);
  SafeCalleeReadsSlotBeforeGC(obj->RawField(0), isolate);
  SafeCalleeReadsEmbedderSlotBeforeGC(EmbedderDataSlot(*js_obj, 0), isolate);
  SafeCalleeReadsProxyRefBeforeGC(UnalignedSlot<int>(&obj->primitive_field_)[0],
                                  isolate);
  SafeWrapperCallee(&obj->primitive_field_, isolate);
  SafeCalleeRebindsPtr(&obj->primitive_field_, isolate);
  SafeCalleeWithUnrelatedStaleTagged(&obj->primitive_field_, isolate);
  SafeCalleeWithUnrelatedStaleMemberAndSlot(&obj->primitive_field_, isolate);
  {
    int* stack_ptr = &obj->primitive_field_;
    SafeCalleeReadsDualProvenancePtrRef(cond ? stack_ptr : obj->ptr_field_,
                                        isolate);
  }
  SafeCalleeIfCondVarBeforeGC(&obj->primitive_field_, isolate);
  SafeCalleeStructuredBindingCopy(obj->nested_, isolate);
  SafeCalleeRefAggregateReadsValueBeforeGC(obj->primitive_field_, isolate);
  SafeCalleeRefAggregatePassesIntByValue(obj->primitive_field_, isolate);
  SafeCalleePtrAndIntReadsIntBeforeGC(&obj->primitive_field_, isolate);
  SafeCalleeCopiesNestedByValue(obj->nested_, isolate);
  ForwardDeclaredSafeCallee(&obj->primitive_field_, isolate);
  SafeCalleeWithDesignatedInit(&obj->primitive_field_, isolate);
  SafeCalleeTakesIntByValue(obj->primitive_field_, isolate);
}

void ForwardDeclaredSafeCallee(const int* p, Isolate* isolate) {
  int val = *p;
  USE(val);
  CauseGCManaged(42, isolate);
}

struct SafeOpTarget {
  Isolate* isolate;
  void operator+=(const int& r) {
    int val = r;
    USE(val);
    CauseGCManaged(42, isolate);
  }
  SafeOpTarget& operator=(const int& r) {
    int val = r;
    USE(val);
    CauseGCManaged(42, isolate);
    return *this;
  }
};

struct UnsafeOpTarget {
  Isolate* isolate;
  void operator+=(int& r) {
    CauseGCManaged(42, isolate);
    r = 1;
  }
  UnsafeOpTarget& operator=(int& r) {
    CauseGCManaged(42, isolate);
    r = 1;
    return *this;
  }
};

void operator-(SafeOpTarget target, const int& r) {
  int val = r;
  USE(val);
  CauseGCManaged(42, target.isolate);
}

void operator-(UnsafeOpTarget target, int& r) {
  CauseGCManaged(42, target.isolate);
  r = 1;
}

class CalleeSummaryVirtualBase {
 public:
  virtual void SafeVirtual(const int* p, Isolate* isolate) {
    int val = *p;
    USE(val);
    CauseGCManaged(42, isolate);
  }
};

class CalleeSummaryFinalDerived final : public CalleeSummaryVirtualBase {
 public:
  void SafeVirtual(const int* p, Isolate* isolate) override {
    int val = *p;
    USE(val);
    CauseGCManaged(42, isolate);
  }
};

void TestCalleeSummaryOperatorsAndVirtuals(
    Isolate* isolate, CalleeSummaryVirtualBase* base_ptr,
    CalleeSummaryFinalDerived* final_ptr) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  SafeOpTarget safe_op{isolate};
  UnsafeOpTarget unsafe_op{isolate};

  // Non-static member call receiver warns (`prop_idx == 0`); static member
  // calls lower to a plain `CallExpr` whose base is visited only for side
  // effects and does not pass `this`.
  obj->nested_.MethodThatGCs(isolate);
  obj->nested_.StaticMethodThatGCs(isolate);

  // Member operator receiver (`prop_idx == 1`): warns on on-heap receiver.
  obj->nested_ += isolate;

  // Member operator arguments (`param_offset == 2`, including overloaded `=`):
  // unsafe warns, safe does not.
  unsafe_op += obj->primitive_field_;
  safe_op += obj->primitive_field_;
  unsafe_op = obj->primitive_field_;
  safe_op = obj->primitive_field_;

  // Free operator argument (`param_offset == 1`): unsafe warns, safe does not.
  unsafe_op - obj->primitive_field_;
  safe_op - obj->primitive_field_;

  // Virtual dispatch: non-final warns conservatively, while explicitly
  // qualified and `final` devirtualized calls are summarized safe.
  base_ptr->SafeVirtual(&obj->primitive_field_, isolate);
  base_ptr->CalleeSummaryVirtualBase::SafeVirtual(&obj->primitive_field_,
                                                  isolate);
  final_ptr->SafeVirtual(&obj->primitive_field_, isolate);
}

void SafeChainLeaf(const int* p, Isolate* isolate) {
  int val = *p;
  USE(val);
  CauseGCManaged(42, isolate);
}
void SafeChain7(const int* p, Isolate* isolate) { SafeChainLeaf(p, isolate); }
void SafeChain6(const int* p, Isolate* isolate) { SafeChain7(p, isolate); }
void SafeChain5(const int* p, Isolate* isolate) { SafeChain6(p, isolate); }
void SafeChain4(const int* p, Isolate* isolate) { SafeChain5(p, isolate); }
void SafeChain3(const int* p, Isolate* isolate) { SafeChain4(p, isolate); }
void SafeChain2(const int* p, Isolate* isolate) { SafeChain3(p, isolate); }
void SafeChain1(const int* p, Isolate* isolate) { SafeChain2(p, isolate); }
void SafeChain0(const int* p, Isolate* isolate) { SafeChain1(p, isolate); }

void MutualRecB(const int* p, int depth, Isolate* isolate);
void MutualRecA(const int* p, int depth, Isolate* isolate) {
  if (depth > 0) MutualRecB(p, depth - 1, isolate);
  CauseGCManaged(42, isolate);
}
void MutualRecB(const int* p, int depth, Isolate* isolate) {
  if (depth > 0) MutualRecA(p, depth - 1, isolate);
  CauseGCManaged(42, isolate);
}

void SafeCalleeGuardedByDisableGCMole(int* p, Isolate* isolate) {
  {
    DisableGCMole no_gc_mole;
    CauseGCManaged(42, isolate);
    *p = 1;
  }
  // Keeps this callee in `gcsuspects` so the call site enters
  // `DoesCalleeUseMemberArgUnsafely`, while `p` is unused after this GC.
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeDisableGCMoleAfterGC(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  DisableGCMole no_gc_mole;
  *p = 1;
}

void SafeCalleeGuardedByConservativePinning(int* p, Isolate* isolate) {
  {
    // Guards the nested GC call rather than a direct `*p = 1`, since `Use()`
    // compares `ConservativePinningScope` against `p`'s declaration location
    // (which precedes the function body).
    ConservativePinningScope pinning(isolate->heap());
    UnsafeCalleeUsesPtrAfterGC(p, isolate);
  }
  // Keeps this callee in `gcsuspects` so the call site enters
  // `DoesCalleeUseMemberArgUnsafely`, while `p` is unused after this GC.
  CauseGCManaged(42, isolate);
}

enum class TestSwitchKind { kA, kB };

void UnsafeCalleeWhileZeroIterReturn(int* p, bool cond, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  while (cond) {
    return;
  }
  *p = 1;
}

void UnsafeCalleeWhileZeroIterRebind(int* p, bool cond, Isolate* isolate) {
  int local = 0;
  CauseGCManaged(42, isolate);
  while (cond) {
    p = &local;
  }
  *p = 1;
}

void UnsafeCalleeForZeroIterReturn(int* p, int n, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  for (int i = 0; i < n; ++i) {
    return;
  }
  *p = 1;
}

void UnsafeCalleeRangeForZeroIterReturn(int* p, int (&arr)[2],
                                        Isolate* isolate) {
  CauseGCManaged(42, isolate);
  for (int x : arr) {
    USE(x);
    return;
  }
  *p = 1;
}

void SafeCalleeInfiniteWhileRebindsBeforeBreak(int* p, bool cond,
                                               Isolate* isolate) {
  int local = 0;
  CauseGCManaged(42, isolate);
  while (true) {
    if (cond) {
      p = &local;
      break;
    }
  }
  *p = 1;
}

void SafeCalleeInfiniteForRebindsBeforeBreak(int* p, bool cond,
                                             Isolate* isolate) {
  int local = 0;
  CauseGCManaged(42, isolate);
  for (;;) {
    if (cond) {
      p = &local;
      break;
    }
  }
  *p = 1;
}

void SafeCalleeSwitchAllEnumCasesReturn(int* p, TestSwitchKind kind,
                                        Isolate* isolate) {
  CauseGCManaged(42, isolate);
  switch (kind) {
    case TestSwitchKind::kA:
      return;
    case TestSwitchKind::kB:
      return;
  }
  *p = 1;
}

void UnsafeCalleeForContinueGC(int* p, int n, bool cond, Isolate* isolate) {
  for (int i = 0; i < n; ++i) {
    if (cond) {
      CauseGCManaged(42, isolate);
      continue;
    }
    *p = i;
  }
}

void UnsafeCalleeWhileContinueGC(int* p, bool cond, Isolate* isolate) {
  while (cond) {
    if (cond) {
      CauseGCManaged(42, isolate);
      continue;
    }
    *p = 1;
  }
}

void UnsafeCalleeDoContinueGC(int* p, bool cond, Isolate* isolate) {
  do {
    if (cond) {
      CauseGCManaged(42, isolate);
      continue;
    }
    *p = 1;
  } while (cond);
}

void UnsafeCalleeDoFalseContinueGC(int* p, bool cond, Isolate* isolate) {
  do {
    if (cond) {
      CauseGCManaged(42, isolate);
      continue;
    }
    return;
  } while (false);
  *p = 1;
}

void UnsafeCalleeSwitchInsideForContinueGC(int* p, int n, int k,
                                           Isolate* isolate) {
  for (int i = 0; i < n; ++i) {
    switch (k) {
      default:
        break;
      case 0:
        CauseGCManaged(42, isolate);
        continue;
    }
    *p = i;
  }
}

void SafeCalleeSwitchInsideForContinueRebindsInInc(int* p, int n, int k,
                                                   Isolate* isolate) {
  int local = 0;
  for (int i = 0; i < n; ++i, p = &local) {
    switch (k) {
      case 0:
        CauseGCManaged(42, isolate);
        continue;
      default:
        *p = i;
        break;
    }
  }
}

void UnsafeCalleeForContinueStaleInInc(int* p, int n, Isolate* isolate) {
  for (int i = 0; i < n; *p = ++i) {
    CauseGCManaged(42, isolate);
    continue;
  }
}

void SafeCalleeForContinueRebindsInInc(int* p, int n, bool cond,
                                       Isolate* isolate) {
  int local = 0;
  for (int i = 0; i < n; ++i, p = &local) {
    if (cond) {
      CauseGCManaged(42, isolate);
      continue;
    }
    *p = i;
  }
}

struct RefBoxAssign {
  int& ref;
  RefBoxAssign& operator=(int v) {
    ref = v;
    return *this;
  }
};

struct GcAssign {
  int* p;
  Isolate* isolate;
  GcAssign& operator=(int v) {
    CauseGCManaged(42, isolate);
    *p = v;
    return *this;
  }
};

struct TrivialPtrHolder {
  int* p;
};

void UnsafeCalleeCustomAssignAfterGC(int& r, Isolate* isolate) {
  RefBoxAssign box{r};
  CauseGCManaged(42, isolate);
  box = 1;
}

void UnsafeCalleeCustomAssignTriggersGC(int* p, Isolate* isolate) {
  GcAssign box{p, isolate};
  box = 1;
}

void SafeCalleeTrivialRebindHolder(int* p, Isolate* isolate) {
  int local = 0;
  TrivialPtrHolder h{p};
  h = TrivialPtrHolder{&local};
  CauseGCManaged(42, isolate);
  *h.p = 1;
}

void UnsafeCalleeStructuredBindingArrayCopy(int* p, Isolate* isolate) {
  int* ptrs[2] = {p, nullptr};
  auto [a, b] = ptrs;
  CauseGCManaged(42, isolate);
  *a = 1;
  USE(b);
}

void UnsafeCalleeStructArrayField(int* p, Isolate* isolate) {
  struct ArrBox {
    int* ptrs[2];
    int counts[2];
  };
  ArrBox box{{p, nullptr}, {1, 2}};
  int* q = box.ptrs[0];
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeStructScalarArrayField(int* p, Isolate* isolate) {
  struct ArrBox {
    int* ptrs[2];
    int counts[2];
  };
  ArrBox box{{p, nullptr}, {1, 2}};
  int c = box.counts[0];
  CauseGCManaged(42, isolate);
  USE(c);
}

void UnsafeCallee2DLocalArray(int* p, Isolate* isolate) {
  int* grid[1][2] = {{p, nullptr}};
  int* q = grid[0][0];
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeArrayOfRefBoxes(int& r, Isolate* isolate) {
  int local = 0;
  struct RefWrap {
    int& ref;
  };
  struct Outer {
    RefWrap boxes[2];
  };
  Outer o{{{r}, {local}}};
  RefWrap b = o.boxes[0];
  CauseGCManaged(42, isolate);
  b.ref = 1;
}

void UnsafeCalleeDualProvenanceStructRefField(SomeObjectWithMembers::PtrBox& r,
                                              Isolate* isolate) {
  int* q = r.ptr;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeDualProvenanceStructRefScalarField(
    SomeObjectWithMembers::PtrBox& r, Isolate* isolate) {
  int n = r.count;
  CauseGCManaged(42, isolate);
  USE(n);
}

struct DefaultInitGC {
  int v = CauseGCInt(nullptr);
};

void UnsafeCalleeArrayFillerGC(int* p, Isolate* isolate) {
  // Omitted trailing element `arr[1]` runs `DefaultInitGC`'s default member
  // initializer via `InitListExpr::getArrayFiller()`, which triggers GC before
  // `*p = 1`.
  DefaultInitGC arr[2] = {{0}};
  USE(arr);
  *p = 1;
  // Keeps this callee in `gcsuspects` (Pass 1's `CalleesPrinter` visits only
  // syntactic `InitListExpr` forms) while `*p = 1` happens before this call.
  CauseGCManaged(42, isolate);
}

void TestControlFlowAndAggregateEdgeCases(Isolate* isolate, bool cond, int n,
                                          int k, TestSwitchKind kind) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  int local_arr[2] = {1, 2};

  // Zero-iteration loop exits and `continue` routing: unsafe callees warn,
  // while infinite loops rebinding before `break`, exhaustive enum switches
  // returning in all cases, and `continue` rebinding in `for` increment do not.
  UnsafeCalleeWhileZeroIterReturn(&obj->primitive_field_, cond, isolate);
  UnsafeCalleeWhileZeroIterRebind(&obj->primitive_field_, cond, isolate);
  UnsafeCalleeForZeroIterReturn(&obj->primitive_field_, n, isolate);
  UnsafeCalleeRangeForZeroIterReturn(&obj->primitive_field_, local_arr,
                                     isolate);
  UnsafeCalleeForContinueGC(&obj->primitive_field_, n, cond, isolate);
  UnsafeCalleeWhileContinueGC(&obj->primitive_field_, cond, isolate);
  UnsafeCalleeDoContinueGC(&obj->primitive_field_, cond, isolate);
  UnsafeCalleeDoFalseContinueGC(&obj->primitive_field_, cond, isolate);
  UnsafeCalleeSwitchInsideForContinueGC(&obj->primitive_field_, n, k, isolate);
  UnsafeCalleeForContinueStaleInInc(&obj->primitive_field_, n, isolate);

  SafeCalleeInfiniteWhileRebindsBeforeBreak(&obj->primitive_field_, cond,
                                            isolate);
  SafeCalleeInfiniteForRebindsBeforeBreak(&obj->primitive_field_, cond,
                                          isolate);
  SafeCalleeSwitchAllEnumCasesReturn(&obj->primitive_field_, kind, isolate);
  SafeCalleeForContinueRebindsInInc(&obj->primitive_field_, n, cond, isolate);
  SafeCalleeSwitchInsideForContinueRebindsInInc(&obj->primitive_field_, n, k,
                                                isolate);

  // Local aggregate assignment, array subscripts, dual-provenance struct
  // references, `InitListExpr::getArrayFiller()`, and callee-side
  // `ConservativePinningScope`.
  UnsafeCalleeCustomAssignAfterGC(obj->primitive_field_, isolate);
  UnsafeCalleeCustomAssignTriggersGC(&obj->primitive_field_, isolate);
  SafeCalleeTrivialRebindHolder(&obj->primitive_field_, isolate);

  UnsafeCalleeStructuredBindingArrayCopy(&obj->primitive_field_, isolate);
  UnsafeCalleeStructArrayField(&obj->primitive_field_, isolate);
  SafeCalleeStructScalarArrayField(&obj->primitive_field_, isolate);
  UnsafeCallee2DLocalArray(&obj->primitive_field_, isolate);
  UnsafeCalleeArrayOfRefBoxes(obj->primitive_field_, isolate);

  {
    SomeObjectWithMembers::PtrBox local_box{&obj->primitive_field_, 1};
    UnsafeCalleeDualProvenanceStructRefField(
        cond ? local_box : obj->ptr_box_field_, isolate);
  }
  {
    SomeObjectWithMembers::PtrBox local_box{&obj->primitive_field_, 1};
    SafeCalleeDualProvenanceStructRefScalarField(
        cond ? local_box : obj->ptr_box_field_, isolate);
  }

  UnsafeCalleeArrayFillerGC(&obj->primitive_field_, isolate);
  {
    int* p = &obj->primitive_field_;
    DefaultInitGC arr[2] = {{0}};
    USE(arr);
    *p = 1;
  }
  SafeCalleeGuardedByConservativePinning(&obj->primitive_field_, isolate);
}

void TestCalleeSummaryDepthAndGuards(Isolate* isolate, bool cond) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  // Depth 9 (`SafeChain0`..`SafeChain7` -> `SafeChainLeaf`) exceeds
  // `kMaxCalleeSummaryDepth` (8) and warns conservatively, but must NOT poison
  // `callee_safety_cache_` for exact-depth-8 `SafeChain1` or shallower
  // `SafeChain7` (`SafeChain1` runs before `SafeChain7` so it traverses the
  // full depth-8 chain down to `SafeChainLeaf` without hitting a warm cache).
  SafeChain0(&obj->primitive_field_, isolate);
  SafeChain1(&obj->primitive_field_, isolate);
  SafeChain7(&obj->primitive_field_, isolate);

  // Mutual recursion cycle warns conservatively.
  MutualRecA(&obj->primitive_field_, 1, isolate);

  // Guard inside callee suppresses warning when declared before the GC, but
  // does not excuse a stale use after an earlier unguarded GC.
  SafeCalleeGuardedByDisableGCMole(&obj->primitive_field_, isolate);
  UnsafeCalleeDisableGCMoleAfterGC(&obj->primitive_field_, isolate);

  // `ConservativePinningScope` at call site suppresses the escaped-arg warning
  // for `call`'s GC, while a pointer already killed by an earlier GC before
  // the pinning scope is still reported as stale.
  {
    ConservativePinningScope pinning(isolate->heap());
    UnsafeCalleeUsesPtrAfterGC(&obj->primitive_field_, isolate);
  }
  {
    int* pre_killed = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    ConservativePinningScope pinning(isolate->heap());
    UnsafeCalleeUsesPtrAfterGC(pre_killed, isolate);
  }

  // `DisableGCMole` declared after an earlier outer-scope GC must still guard
  // calls inside nested blocks.
  CauseGCManaged(42, isolate);
  DisableGCMole no_gc_mole;
  if (cond) {
    UnsafeCalleeUsesPtrAfterGC(&obj->primitive_field_, isolate);
  }
}

// ------- Test glvalue InitListExpr, refs, chained assigns, and AST edge cases
// -------

void UnsafeCalleeBraceInitRefWrite(int& f, Isolate* isolate) {
  int& r{f};
  CauseGCManaged(42, isolate);
  r = 42;
}

void UnsafeCalleeBraceInitRefAddrOf(int& f, Isolate* isolate) {
  int& r{f};
  int* p = &r;
  CauseGCManaged(42, isolate);
  *p = 1;
}

void UnsafeCalleeBraceInitDualRefRebind(int*& f, Isolate* isolate) {
  int*& dual{f};
  CauseGCManaged(42, isolate);
  int local = 0;
  dual = &local;
}

void UnsafeCalleeBraceInitDualRefDeref(int*& f, Isolate* isolate) {
  int*& dual{f};
  int* q = dual;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeBraceInitRefScalarCopy(int& f, Isolate* isolate) {
  int& r{f};
  int x = r;
  CauseGCManaged(42, isolate);
  USE(x);
}

void UnsafeCalleeTakesMemberPtrRef(TaggedMember<Object>*& p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(p);
}

void UnsafeCalleeTakesMemberPtrConstRef(TaggedMember<Object>* const& p,
                                        Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(p);
}

void SafeCalleeRebindsMemberPtrRef(TaggedMember<Object>*& out,
                                   Isolate* isolate) {
  CauseGCManaged(42, isolate);
  out = nullptr;
  USE(out);
}

void TestMemberPtrRefParams(Isolate* isolate, bool cond,
                            TaggedMember<Object>* const& member_ptr_const_ref,
                            TaggedMember<Object>*& member_ptr_ref) {
  // Should warn: forwarding reference-to-member-pointer parameters without
  // `CK_LValueToRValue` must still carry raw member address provenance.
  if (cond) {
    UnsafeCalleeTakesMemberPtrConstRef(member_ptr_const_ref, isolate);
  } else {
    UnsafeCalleeTakesMemberPtrRef(member_ptr_ref, isolate);
  }
}

void TestMemberPtrRefParamSafeOutParam(Isolate* isolate,
                                       TaggedMember<Object>*& member_ptr_ref) {
  // Safe: callee rebinds `out = nullptr` after GC before using it.
  SafeCalleeRebindsMemberPtrRef(member_ptr_ref, isolate);
}

void UnsafeCalleeTakesHolder(TrivialPtrHolder h, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  *h.p = 1;
}

void UnsafeCalleeChainedTrivialAssign(int* p, Isolate* isolate) {
  TrivialPtrHolder b1{p}, b2{nullptr}, b3{nullptr};
  b3 = (b2 = b1);
  CauseGCManaged(42, isolate);
  *b3.p = 1;
}

void UnsafeCalleeChainedTrivialAssignCopyInit(int* p, Isolate* isolate) {
  TrivialPtrHolder b1{p}, b2{nullptr};
  TrivialPtrHolder b3 = (b2 = b1);
  CauseGCManaged(42, isolate);
  *b3.p = 1;
}

void UnsafeCalleeChainedTrivialAssignCallArg(int* p, Isolate* isolate) {
  TrivialPtrHolder b1{p}, b2{nullptr};
  UnsafeCalleeTakesHolder(b2 = b1, isolate);
}

void UnsafeCalleeOnHeapStructAssignRef(SomeObjectWithMembers::PtrBox& field,
                                       Isolate* isolate) {
  int local = 0;
  SomeObjectWithMembers::PtrBox& ref =
      (field = SomeObjectWithMembers::PtrBox{&local, 1});
  CauseGCManaged(42, isolate);
  ref.count = 2;
}

void ReadConstPtrPtr(int* const* pp) { USE(pp); }
void ReadConstSlotPtr(const ObjectSlot* sp) { USE(sp); }
void InitConstPointeePtr(const int** out) {
  static const int kLocal = 0;
  *out = &kLocal;
}

void UnsafeCalleeConstPtrAddrOf(int* const p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  ReadConstPtrPtr(&p);
  *p = 1;
}

void SafeCalleeMutablePtrToConstOutParam(const int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  InitConstPointeePtr(&p);
  USE(*p);
}

void UnsafeCalleeAssignExprRef(int& f, Isolate* isolate) {
  int& r = (f = 1);
  CauseGCManaged(42, isolate);
  r = 2;
}

void SafeCalleeAssignExprScalarCopy(int& f, Isolate* isolate) {
  int x = (f = 1);
  CauseGCManaged(42, isolate);
  USE(x);
}

void SafeCalleeStdAddressofLocalPtr(int* p, Isolate* isolate) {
  int local = 0;
  int** pp = std::addressof(p);
  p = &local;
  CauseGCManaged(42, isolate);
  USE(pp);
}

static SomeObjectWithMembers* g_filler_obj = nullptr;

struct FillerGCThenMember {
  int gc_first = CauseGCInt(nullptr);
  int* member_second = &g_filler_obj->primitive_field_;
};

void SafeCalleeParenListInitScalar(const int* p, Isolate* isolate) {
  struct IntPair {
    int a;
    int b;
  };
  IntPair pair(*p, 1);
  USE(pair);
  CauseGCManaged(42, isolate);
}

struct OffsetOfArr {
  int a[4];
};

void UnsafeCalleeOffsetOfRuntimeIdx(const int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  size_t off = __builtin_offsetof(OffsetOfArr, a[*p]);
  USE(off);
}

void TestGlvalueInitChainedAssignAndAstEdgeCases(Isolate* isolate, bool cond) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  // Glvalue `InitListExpr` (`int& r{f}` and `{obj->primitive_field_}`):
  // unsafe callees warn, safe scalar copy and safe ref read do not.
  UnsafeCalleeBraceInitRefWrite(obj->primitive_field_, isolate);
  UnsafeCalleeBraceInitRefAddrOf(obj->primitive_field_, isolate);
  {
    int* member_ptr = &obj->primitive_field_;
    UnsafeCalleeBraceInitDualRefRebind(cond ? member_ptr : obj->ptr_field_,
                                       isolate);
  }
  {
    int* member_ptr = &obj->primitive_field_;
    UnsafeCalleeBraceInitDualRefDeref(cond ? member_ptr : obj->ptr_field_,
                                      isolate);
  }
  SafeCalleeBraceInitRefScalarCopy(obj->primitive_field_, isolate);
  UnsafeCalleeUsesRefAfterGC({obj->primitive_field_}, isolate);
  SafeCalleeReadsRefBeforeGC({obj->primitive_field_}, isolate);

  // Chained trivial aggregate assignment and on-heap struct assignment.
  UnsafeCalleeChainedTrivialAssign(&obj->primitive_field_, isolate);
  UnsafeCalleeChainedTrivialAssignCopyInit(&obj->primitive_field_, isolate);
  UnsafeCalleeChainedTrivialAssignCallArg(&obj->primitive_field_, isolate);
  UnsafeCalleeOnHeapStructAssignRef(obj->ptr_box_field_, isolate);

  // `&const_var` cannot act as an out-parameter, while a mutable pointer to
  // `const` pointee (`const int* p`) still supports out-param initialization.
  UnsafeCalleeConstPtrAddrOf(&obj->primitive_field_, isolate);
  SafeCalleeMutablePtrToConstOutParam(&obj->primitive_field_, isolate);
  {
    const ObjectSlot const_slot = obj->RawField(0);
    CauseGCManaged(42, isolate);
    ReadConstSlotPtr(&const_slot);
    USE(*const_slot);
  }

  // Built-in assignment and compound assignment preserve `kOnHeapMemberLValue`.
  UnsafeCalleeAssignExprRef(obj->primitive_field_, isolate);
  SafeCalleeAssignExprScalarCopy(obj->primitive_field_, isolate);
  UnsafeCalleeUsesRefAfterGC(obj->primitive_field_ += 1, isolate);
  UnsafeCalleeUsesRefAfterGC(obj->primitive_field_ |= 1, isolate);

  // `std::addressof(local_ptr)` yields a stack address (`int**`), not an
  // interior heap address.
  SafeCalleeStdAddressofLocalPtr(&obj->primitive_field_, isolate);

  // `InitListExpr::getArrayFiller()` with 1 vs 2 omitted elements: 1 element
  // evaluates GC before the member address (safe), while 2 omitted elements
  // expose element 0's member address to element 1's GC (`BAD_EXPR_MSG`).
  {
    FillerGCThenMember single_filler[1] = {};
    USE(single_filler);
    FillerGCThenMember multi_filler[2] = {};
    USE(multi_filler);
  }

  // C++20 `CXXParenListInitExpr`: parenthesized aggregate init tracks raw
  // member provenance and checks indeterminately-sequenced evaluation order
  // (unlike left-to-right `{}` initialization), without false positives on
  // scalar copies in callee summaries.
  {
    TrivialPtrHolder paren_h(&obj->primitive_field_);
    CauseGCManaged(42, isolate);
    *paren_h.p = 1;
    FillerGCThenMember brace_seq{CauseGCInt(isolate), &obj->primitive_field_};
    USE(brace_seq);
    FillerGCThenMember paren_unseq(CauseGCInt(isolate), &obj->primitive_field_);
    USE(paren_unseq);
  }
  SafeCalleeParenListInitScalar(&obj->primitive_field_, isolate);

  // `OffsetOfExpr` with runtime array-designator indices visits the index
  // subexpression for stale uses.
  UnsafeCalleeOffsetOfRuntimeIdx(&obj->primitive_field_, isolate);
  {
    int* p_off = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    size_t off = __builtin_offsetof(OffsetOfArr, a[*p_off]);
    USE(off);
  }
}

void ReadConstMemberPtrPtr(TaggedMember<Object>* const* mp) { USE(mp); }
void ReadConstPtrArr(int* const (*arr)[2]) { USE(arr); }
void InitMutableIntPtr(int** out) {
  static int local = 0;
  *out = &local;
}
void NoOpCallback() {}

void UnsafeCalleeConstRefPtrAddrOf(int* const& p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  ReadConstPtrPtr(&p);
  *p = 1;
}

void UnsafeCalleeConstSlotRefAddrOf(const ObjectSlot& slot, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  ReadConstSlotPtr(&slot);
  USE(*slot);
}

void UnsafeCalleeConstMemberPtrRefAddrOf(TaggedMember<Object>* const& mp,
                                         Isolate* isolate) {
  CauseGCManaged(42, isolate);
  ReadConstMemberPtrPtr(&mp);
  USE(mp);
}

void SafeCalleeMutablePtrRefOutParam(int*& ref, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  InitMutableIntPtr(&ref);
  *ref = 1;
}

void UnsafeCalleeTakesPtrArray(int* arr[2], Isolate* isolate) {
  int* q = arr[0];
  CauseGCManaged(42, isolate);
  *q = 1;
}

void UnsafeCalleeTakesPtrArrayDeref(int** arr, Isolate* isolate) {
  int* q = *arr;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeTakesPtrArray(int* arr[2], Isolate* isolate) {
  int v = *arr[0];
  USE(v);
  CauseGCManaged(42, isolate);
}

void SafeCalleeReadsSizeTPtr(const size_t* p, Isolate* isolate) {
  size_t v = *p;
  CauseGCManaged(42, isolate);
  USE(v);
}

void UnsafeCalleeTakesBoxes(SomeObjectWithMembers::PtrBox boxes[2],
                            Isolate* isolate) {
  int* q = boxes->ptr;
  CauseGCManaged(42, isolate);
  *q = 1;
}

void SafeCalleeTakesBoxesCount(SomeObjectWithMembers::PtrBox boxes[2],
                               Isolate* isolate) {
  int c = boxes->count;
  CauseGCManaged(42, isolate);
  USE(c);
}

void SafeCalleeIfConstexprFalse(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  if constexpr (false) {
    *p = 1;
  }
}

void UnsafeCalleeIfConstexprTrue(int* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  if constexpr (true) {
    *p = 1;
  }
}

struct PtrAndDefaultGC {
  int* ptr = nullptr;
  int gc_val = CauseGCInt(nullptr);
};

struct PtrAndCallback {
  int* data;
  void (*cb)();
};

void TestIndirectProvenanceConstRefAndAstEdgeCases(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  // `&const_ref` and `&const_arr` cannot act as out-parameters, while mutable
  // `int*&` references still support out-param initialization.
  UnsafeCalleeConstRefPtrAddrOf(&obj->primitive_field_, isolate);
  UnsafeCalleeConstSlotRefAddrOf(obj->RawField(0), isolate);
  UnsafeCalleeConstMemberPtrRefAddrOf(&obj->tagged_field_, isolate);
  {
    int* mut_p = &obj->primitive_field_;
    SafeCalleeMutablePtrRefOutParam(mut_p, isolate);
  }
  {
    const ObjectSlot& const_slot_ref = obj->RawField(0);
    CauseGCManaged(42, isolate);
    ReadConstSlotPtr(&const_slot_ref);
    USE(*const_slot_ref);
  }
  {
    int* const const_ptrs[2] = {&obj->primitive_field_, nullptr};
    CauseGCManaged(42, isolate);
    ReadConstPtrArr(&const_ptrs);
    USE(const_ptrs[0]);
  }
  {
    const SomeObjectWithMembers::PtrBox const_box{&obj->primitive_field_, 1};
    const SomeObjectWithMembers::PtrBox* bp = &const_box;
    int* from_const_box = bp->ptr;
    const SomeObjectWithMembers::PtrBox* bp_std = std::addressof(const_box);
    int* from_std_box = bp_std->ptr;
    CauseGCManaged(42, isolate);
    USE(from_const_box);
    USE(from_std_box);
  }

  // Decayed local raw-member-pointer arrays (`kIndirectRawMemberAddress`):
  // `*ptrs`, `*(ptrs + 0)`, `pp[0]`, `boxes->ptr`, `(*boxes).ptr`, and callees
  // receiving decayed arrays track element provenance without falsely flagging
  // scalar `size_t` loads through direct pointers or `int` fields.
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    UnsafeCalleeTakesPtrArray(ptrs, isolate);
  }
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    UnsafeCalleeTakesPtrArrayDeref(ptrs, isolate);
  }
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    SafeCalleeTakesPtrArray(ptrs, isolate);
  }
  SafeCalleeReadsSizeTPtr(
      reinterpret_cast<const size_t*>(&obj->primitive_field_), isolate);
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    int* q_deref = *ptrs;
    int* q_add = *(ptrs + 0);
    int** pp = ptrs;
    int* q_sub = pp[0];
    int** pp_brace{ptrs};
    int* q_brace = pp_brace[0];
    SomeObjectWithMembers::PtrBox boxes[2] = {{&obj->primitive_field_, 1},
                                              {nullptr, 2}};
    int* q_arrow = boxes->ptr;
    int* q_star_dot = (*boxes).ptr;
    int safe_box_count = boxes->count;
    CauseGCManaged(42, isolate);
    USE(q_deref);
    USE(q_add);
    USE(q_sub);
    USE(q_brace);
    USE(q_arrow);
    USE(q_star_dot);
    USE(safe_box_count);
  }
  {
    SomeObjectWithMembers::PtrBox boxes[2] = {{&obj->primitive_field_, 1},
                                              {nullptr, 2}};
    UnsafeCalleeTakesBoxes(boxes, isolate);
  }
  {
    SomeObjectWithMembers::PtrBox boxes[2] = {{&obj->primitive_field_, 1},
                                              {nullptr, 2}};
    SafeCalleeTakesBoxesCount(boxes, isolate);
  }

  // C99 designated array initializer with an interior hole: `[1]`'s GC runs
  // before `[2]` takes `&obj->primitive_field_`, and `getArrayFiller()` must
  // not be visited again after `[2]` (no false `BAD_EXPR_MSG`).
  {
    PtrAndDefaultGC desig_arr[3] = {[0] = {nullptr, 0},
                                    [2] = {&obj->primitive_field_, 0}};
    USE(desig_arr);
  }

  // `CXXParenListInitExpr` with > 64 elements catches unsequenced GC/member
  // hazards past index 64.
  {
    int* big_paren_arr[66](
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        &obj->primitive_field_, (CauseGCManaged(42, isolate), nullptr));
    USE(big_paren_arr);
  }

  // Pointer-to-data-member (`.*` and `->*`) propagates on-heap lvalue and
  // local container provenance while stripping non-address scalar fields.
  {
    int SomeObjectWithMembers::* pmd_prim =
        &SomeObjectWithMembers::primitive_field_;
    int* SomeObjectWithMembers::PtrBox::* pmd_box_ptr =
        &SomeObjectWithMembers::PtrBox::ptr;
    int SomeObjectWithMembers::PtrBox::* pmd_box_cnt =
        &SomeObjectWithMembers::PtrBox::count;
    SomeObjectWithMembers* raw_ptr = (*obj).operator->();
    int* p_pmi = &(raw_ptr->*pmd_prim);
    int* p_pmd = &((*raw_ptr).*pmd_prim);
    SomeObjectWithMembers::PtrBox local_box{&obj->primitive_field_, 1};
    int* q_box_pmd = local_box.*pmd_box_ptr;
    int safe_box_pmd = local_box.*pmd_box_cnt;
    SomeObjectWithMembers::PtrBox pmd_boxes[2] = {{&obj->primitive_field_, 1},
                                                  {nullptr, 2}};
    SomeObjectWithMembers::PtrBox* pmd_boxes_ptr = pmd_boxes;
    int* q_boxes_pmi = pmd_boxes_ptr->*pmd_box_ptr;
    CauseGCManaged(42, isolate);
    USE(p_pmi);
    USE(p_pmd);
    USE(q_box_pmd);
    USE(safe_box_pmd);
    USE(q_boxes_pmi);
  }

  // Function-pointer fields inside a local aggregate cannot hold raw on-heap
  // data-member addresses (no false positive on calling `fn()` after GC).
  {
    PtrAndCallback entry{&obj->primitive_field_, &NoOpCallback};
    void (*fn)() = entry.cb;
    CauseGCManaged(42, isolate);
    fn();
  }

  // Non-template `if constexpr` only analyzes the non-discarded branch.
  SafeCalleeIfConstexprFalse(&obj->primitive_field_, isolate);
  UnsafeCalleeIfConstexprTrue(&obj->primitive_field_, isolate);
  {
    int* p_ce = &obj->primitive_field_;
    if constexpr (false) {
      CauseGCManaged(42, isolate);
    }
    USE(*p_ce);
  }

  // `VisitDoStmt` isolates `GCScope` so a single-statement `DisableGCMole`
  // body does not leak past the `do ... while` loop.
  {
    int* p_do = &obj->primitive_field_;
    do DisableGCMole no_gc;
    while (false);
    CauseGCManaged(42, isolate);
    USE(*p_do);
  }
}

int TakeIntPtrPtrHelper(int** pp) {
  USE(pp);
  return 0;
}
void TakePtrAndInt(int* a, int b) {
  USE(a);
  USE(b);
}
void TakeTwoInts(int a, int b) {
  USE(a);
  USE(b);
}

struct BitCastMethodHelper {
  int* bit_cast(int* p) {
    USE(p);
    return nullptr;
  }
};

struct TaggedArrHolder {
  Tagged<Object> items[1];
  int x;
};

struct QualifiedVirtualBase {
  virtual void VirtualOp(Isolate* isolate) { USE(isolate); }
};

struct QualifiedVirtualDerived : QualifiedVirtualBase {
  void VirtualOp(Isolate* isolate) override { CauseGCManaged(42, isolate); }
};

void TestUnsequencedMergeParenAssignAndVirtualQualifiers(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  // Indeterminately sequenced siblings (function arguments and C++20
  // parenthesized aggregate init): `&p` in a sibling must not erase `p`'s
  // raw member provenance from `p = &obj->primitive_field_`.
  {
    int* p = nullptr;
    TakePtrAndInt(p = &obj->primitive_field_, TakeIntPtrPtrHelper(&p));
    CauseGCManaged(42, isolate);
    *p = 42;
  }
  {
    int* p = nullptr;
    TakeTwoInts((p = &obj->primitive_field_, CauseGCInt(isolate)),
                TakeIntPtrPtrHelper(&p));
    *p = 42;
  }
  {
    struct PtrAndIntPair {
      int* p;
      int n;
    };
    int* p = nullptr;
    PtrAndIntPair pair(p = &obj->primitive_field_, TakeIntPtrPtrHelper(&p));
    USE(pair);
    CauseGCManaged(42, isolate);
    *p = 42;
  }

  // Parenthesized variable LHS (common in macro expansions): `BO_Assign`,
  // `OO_Equal`, and `UO_AddrOf` unwrap parens to track and clear provenance.
  {
    int* p = nullptr;
    (p) = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    *p = 1;
  }
  {
    ObjectSlot slot;
    (slot) = obj->RawField(0);
    CauseGCManaged(42, isolate);
    USE(*slot);
  }
  {
    int local = 0;
    int* q = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    (q) = &local;
    *q = 1;
  }
  {
    int* out = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    InitMutableIntPtr(&(out));
    *out = 1;
  }

  // Member method named `bit_cast` inside `v8::internal` must not be matched
  // as free-function `v8::base::bit_cast`.
  {
    BitCastMethodHelper helper;
    int* safe_p = helper.bit_cast(&obj->primitive_field_);
    CauseGCManaged(42, isolate);
    USE(safe_p);
  }

  // Sequenced initializer list with an array of `Tagged<Object>` followed by
  // a GC-causing element warns `BAD_EXPR_MSG`.
  {
    TaggedArrHolder arr{{*obj}, CauseGCInt(isolate)};
    USE(arr);
  }

  // Explicitly qualified call to a non-GC base virtual method must not be
  // devirtualized to a GC-causing derived override.
  {
    QualifiedVirtualDerived derived;
    int* p = &obj->primitive_field_;
    derived.QualifiedVirtualBase::VirtualOp(isolate);
    *p = 1;
  }

  // `do { ... } while (0);` runs once and must not flag a variable used
  // before the loop's GC as stale on a phantom second iteration.
  {
    int* p = &obj->primitive_field_;
    do {
      USE(*p);
      CauseGCManaged(42, isolate);
    } while (0);
  }

  // Narrowing integer `+=` drops raw member address provenance, matching
  // narrowing `CK_IntegralCast`.
  {
    uint16_t narrow = 0;
    uint16_t copy = (narrow += obj->ext_field_.storage_address());
    CauseGCManaged(42, isolate);
    USE(copy);
  }
}

struct AggregateWithLocationMethods {
  int* ptr;
  int location() const { return 0; }
  int* address() const { return nullptr; }
};

class HeapObjectWithInnerLocation : public HeapObject {
 public:
  struct Inner {
    int location() const { return 0; }
  } inner_;
};

struct IndirectScalarView {
  uintptr_t count;
};

void InitEmbedderSlot(EmbedderDataSlot* out) { USE(out); }

void InitEmbedderSlotWithGC(EmbedderDataSlot* out, Isolate* isolate) {
  USE(out);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeTakesEmbedderSlotByValue(EmbedderDataSlot s,
                                          Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(s);
}

void UnsafeCalleeTakesEmbedderSlotConstRef(const EmbedderDataSlot& s,
                                           Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(s);
}

void UnsafeCalleeTakesEmbedderSlotRef(EmbedderDataSlot& s, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(s);
}

void SafeCalleeReadsEmbedderSlotByValueBeforeGC(EmbedderDataSlot s,
                                                Isolate* isolate) {
  Address a = s.address();
  USE(a);
  CauseGCManaged(42, isolate);
}

void TestReclearedAmbientSlotAndMemberPtrParams(EmbedderDataSlot& s,
                                                TaggedMember<Object>*& p,
                                                Isolate* isolate) {
  // Re-clearing and out-param-repopulating reference parameters must still
  // report stale uses after GC via `Environment::IsTypeRawVar`.
  s = EmbedderDataSlot();
  InitEmbedderSlot(&s);
  p = nullptr;
  InitTaggedMemberPtr(&p);
  CauseGCManaged(42, isolate);
  USE(s);
  USE(p);
}

void TestStdAddressofAddressAccessorsAndAmbientSlotUse(bool cond,
                                                       Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Tagged<HeapObjectWithInnerLocation> raw_loc;
  Handle<HeapObjectWithInnerLocation> loc_obj = handle(raw_loc, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();

  // `std::addressof` mirrors `UO_AddrOf`: dual-provenance mutable references
  // and non-`const` local container elements/fields preserve
  // `kIndirectRawMemberAddress`.
  {
    int* member_ptr = &obj->primitive_field_;
    int*& dual_ref = cond ? member_ptr : obj->ptr_field_;
    int** pp = std::addressof(dual_ref);
    int* q = *pp;
    CauseGCManaged(42, isolate);
    *q = 1;
  }
  {
    SomeObjectWithMembers::PtrBox boxes[2] = {{&obj->primitive_field_, 1},
                                              {nullptr, 2}};
    SomeObjectWithMembers::PtrBox* bp = std::addressof(boxes[0]);
    int* q = bp->ptr;
    CauseGCManaged(42, isolate);
    *q = 1;
  }
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    int** pp = std::addressof(ptrs[0]);
    int* q = *pp;
    CauseGCManaged(42, isolate);
    *q = 1;
  }
  {
    struct OuterBox {
      SomeObjectWithMembers::PtrBox box;
    };
    OuterBox outer{{&obj->primitive_field_, 1}};
    SomeObjectWithMembers::PtrBox* bp = std::addressof(outer.box);
    int* q = bp->ptr;
    CauseGCManaged(42, isolate);
    *q = 1;
  }

  // `kAddressAccessors` only applies to `*Member` and `SlotBase` receivers
  // with address-capable return types, not same-named methods on local
  // aggregates or plain interior `HeapObject` structs.
  {
    AggregateWithLocationMethods agg{&obj->primitive_field_};
    int safe_loc = agg.location();
    int* safe_addr = agg.address();
    int safe_inner_loc = loc_obj->inner_.location();
    CauseGCManaged(42, isolate);
    USE(safe_loc);
    USE(safe_addr);
    USE(safe_inner_loc);
  }

  // `VisitMemberExpr` strips stray `kIndirectRawMemberAddress` when accessing
  // a field via `.` on a `bit_cast` indirect pointer view.
  {
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    uintptr_t ptrs_addr = reinterpret_cast<uintptr_t>(ptrs);
    uintptr_t count = base::bit_cast<IndirectScalarView>(ptrs_addr).count;
    CauseGCManaged(42, isolate);
    USE(count);
  }

  // Floating-point arithmetic does not propagate raw member provenance, while
  // `+=` into an untracked `Address` variable starts tracking it without
  // resurrecting already-stale tracked variables.
  {
    double safe_d = obj->ext_field_.storage_address() + 1.0;
    CauseGCManaged(42, isolate);
    USE(safe_d);
  }
  {
    Address base = 0;
    base += obj->ext_field_.storage_address();
    CauseGCManaged(42, isolate);
    USE(base);
  }
  {
    int* stale_p = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    stale_p += 1;
    *stale_p = 42;
  }

  // Local out-param-initialized always-on-heap slots re-acquire
  // `kRawMemberAddress` in `Use()` while staying compatible with GC-capable
  // out-param initialization.
  {
    EmbedderDataSlot slot;
    InitEmbedderSlotWithGC(&slot, isolate);
    SafeCalleeReadsEmbedderSlotByValueBeforeGC(slot, isolate);
  }
  {
    EmbedderDataSlot slot;
    InitEmbedderSlot(&slot);
    UnsafeCalleeTakesEmbedderSlotByValue(slot, isolate);
  }
  {
    EmbedderDataSlot slot;
    InitEmbedderSlot(&slot);
    UnsafeCalleeTakesEmbedderSlotConstRef(slot, isolate);
  }
  {
    EmbedderDataSlot slot;
    InitEmbedderSlot(&slot);
    UnsafeCalleeTakesEmbedderSlotRef(slot, isolate);
  }
  {
    EmbedderDataSlot slot;
    InitEmbedderSlot(&slot);
    Address derived = slot.address();
    CauseGCManaged(42, isolate);
    USE(derived);
  }
  {
    EmbedderDataSlot slot;
    InitEmbedderSlot(&slot);
    const EmbedderDataSlot& alias = slot;
    CauseGCManaged(42, isolate);
    USE(alias);
  }

  // Local out-param-initialized `*Member*` pointers re-acquire
  // `kRawMemberAddress` in `Use()` even without `CK_LValueToRValue`.
  {
    TaggedMember<Object>* mp = nullptr;
    InitTaggedMemberPtr(&mp);
    UnsafeCalleeTakesMemberPtrRef(mp, isolate);
  }
  {
    TaggedMember<Object>* mp = nullptr;
    InitTaggedMemberPtr(&mp);
    TaggedMember<Object>*& alias = mp;
    CauseGCManaged(42, isolate);
    USE(alias);
  }

  // Zero-GC negative cases for ambient slot / member-pointer references and
  // local slot arrays.
  {
    const EmbedderDataSlot& empty_slot_ref = EmbedderDataSlot();
    USE(empty_slot_ref);
    TaggedMember<Object>* const& null_member_ref = nullptr;
    USE(null_member_ref);
    EmbedderDataSlot slots[2];
    slots[0] = EmbedderDataSlot(*js_obj, 0);
    USE(slots[0]);
  }
}

class DerivedTaggedMember : public TaggedMember<Object> {
 public:
  void TestPtrLocationOnThis(Isolate* isolate) {
    const Tagged_t* loc = this->ptr_location();
    CauseGCManaged(42, isolate);
    USE(*loc);
  }

  static void TestPtrLocationOnObj(DerivedTaggedMember& member,
                                   DerivedTaggedMember* member_ptr,
                                   DerivedTaggedMember (&member_arr)[2],
                                   int mode, Isolate* isolate) {
    if (mode == 0) {
      const Tagged_t* loc = member.ptr_location();
      CauseGCManaged(42, isolate);
      USE(*loc);
      return;
    }
    if (mode == 1) {
      const Tagged_t* loc = member_ptr->ptr_location();
      CauseGCManaged(42, isolate);
      USE(*loc);
      return;
    }
    const Tagged_t* loc = member_arr->ptr_location();
    CauseGCManaged(42, isolate);
    USE(*loc);
  }
};

class HeapObjectWithDerivedTaggedMember : public HeapObject {
 public:
  DerivedTaggedMember member_;
  DerivedTaggedMember member_array_[2];
};

class DerivedStackTagged : public Tagged<Object> {
 public:
  explicit DerivedStackTagged(Tagged<Object> o) : Tagged<Object>(o) {}
  void TestStackPtrLocationIsSafe(Isolate* isolate) {
    const Address* safe_loc = this->ptr_location();
    CauseGCManaged(42, isolate);
    USE(safe_loc);
  }
};

class TestHandleLocationSlot
    : public SlotBase<TestHandleLocationSlot, ExternalPointer_t, kTaggedSize> {
 public:
  explicit TestHandleLocationSlot(Address ptr) : SlotBase(ptr) {}
  ExternalPointerHandle* handle_location() const {
    return reinterpret_cast<ExternalPointerHandle*>(address());
  }
};

using MemberPtrAlias = TaggedMember<Object>*;

void HelperTakesIntPtrAndInt(int* flag, int val) {
  USE(flag);
  USE(val);
}

void HelperTakesFnPtrAndInt(void (*fn)(), int val) {
  USE(fn);
  USE(val);
}

int* g_global_test_ptr = nullptr;

void TestReclearedMemberPtrAliasParam(MemberPtrAlias& p, Isolate* isolate) {
  p = nullptr;
  InitTaggedMemberPtr(&p);
  CauseGCManaged(42, isolate);
  USE(p);
}

void TestInheritedAccessorsUnsequencedDeltasAndNullInits(int switch_tag,
                                                         int count,
                                                         Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Tagged<HeapObjectWithDerivedTaggedMember> raw_arr_obj;
  Handle<HeapObjectWithDerivedTaggedMember> arr_obj =
      handle(raw_arr_obj, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();

  // Inherited `TaggedImpl::ptr_location()` on `TaggedMember<T>` (via `this`,
  // direct field, pointer, and decayed member array) plus
  // `SlotBase::ToVoidPtr()` and `handle_location()` on a `SlotBase` subclass
  // carry `kRawMemberAddress`, whereas `Tagged<Object>::ptr_location()` on a
  // local stack `Tagged` does not.
  {
    arr_obj->member_.TestPtrLocationOnThis(isolate);
    DerivedTaggedMember::TestPtrLocationOnObj(
        arr_obj->member_, &arr_obj->member_, arr_obj->member_array_, switch_tag,
        isolate);
  }
  {
    ObjectSlot slot(&obj->tagged_field_);
    void* vp = slot.ToVoidPtr();
    CauseGCManaged(42, isolate);
    USE(vp);
  }
  {
    TestHandleLocationSlot ext_slot(obj->ext_field_.storage_address());
    ExternalPointerHandle* hp = ext_slot.handle_location();
    CauseGCManaged(42, isolate);
    USE(*hp);
  }
  {
    DerivedStackTagged local_tagged(*obj);
    local_tagged.TestStackPtrLocationIsSafe(isolate);
  }

  // Unsequenced non-GC sibling deltas (`&local_flag`, `local_flag = 1`,
  // `&NoOpCallback`) do not kill a raw member variable rebound after the only
  // GC in a sibling argument or `CXXParenListInitExpr`, while a variable bound
  // in a non-GC sibling IS still killed.
  {
    int local_flag = 0;
    int* p = nullptr;
    HelperTakesIntPtrAndInt(
        &local_flag, (CauseGCInt(isolate), p = &obj->primitive_field_, 0));
    *p = 1;
  }
  {
    int local_flag = 0;
    int* p = nullptr;
    TakeTwoInts((local_flag = 1),
                (CauseGCInt(isolate), p = &obj->primitive_field_, 0));
    *p = 1;
  }
  {
    int* p = nullptr;
    HelperTakesFnPtrAndInt(
        &NoOpCallback, (CauseGCInt(isolate), p = &obj->primitive_field_, 0));
    *p = 1;
  }
  {
    struct TwoInts {
      int a;
      int b;
    };
    int local_flag = 0;
    int* p = nullptr;
    TwoInts ti((local_flag = 1),
               (CauseGCInt(isolate), p = &obj->primitive_field_, 0));
    USE(ti);
    *p = 1;
  }
  {
    int* p = nullptr;
    int* q = nullptr;
    TakeTwoInts((CauseGCInt(isolate), p = &obj->primitive_field_, 0),
                (q = &obj->primitive_field_, 0));
    // `p` is sequenced after the only GC (safe); `q` is unsequenced (stale).
    *p = 1;
    *q = 2;
  }
  {
    struct TwoInts {
      int a;
      int b;
    };
    int* p = nullptr;
    int* q = nullptr;
    TwoInts ti((CauseGCInt(isolate), p = &obj->primitive_field_, 0),
               (q = &obj->primitive_field_, 0));
    USE(ti);
    // `p` is sequenced after the only GC (safe); `q` is unsequenced (stale).
    *p = 1;
    *q = 2;
  }

  // Null, empty, constexpr, and default-constructed tracked raw elements in
  // brace-init lists do not false-positive against a later GC element, while
  // live non-null elements in both brace-init and C++20 parenthesized aggregate
  // init (`CXXParenListInitExpr`) are caught.
  {
    struct MemberPtrAndInt {
      TaggedMember<Object>* p;
      int x;
    };
    struct HeapObjPtrAndInt {
      HeapObject* o;
      int x;
    };
    struct HeapObjArrAndInt {
      HeapObject* arr[2];
      int x;
    };
    struct ExtHandleAndInt {
      ExternalPointerHandle h;
      int x;
    };
    struct EmbedderSlotAndInt {
      EmbedderDataSlot s;
      int x;
    };
    struct TaggedAndInt {
      Tagged<Object> o;
      int x;
    };
    struct IntAndSlot {
      int x;
      EmbedderDataSlot s;
    };
    struct TwoMemberPtrs {
      TaggedMember<Object>* a;
      TaggedMember<Object>* b;
    };

    // Safe null/empty/constexpr/default-constructed brace-init cases.
    MemberPtrAndInt safe1{nullptr, CauseGCInt(isolate)};
    USE(safe1);
    HeapObjPtrAndInt safe2{nullptr, CauseGCInt(isolate)};
    USE(safe2);
    HeapObjPtrAndInt safe3{{}, CauseGCInt(isolate)};
    USE(safe3);
    HeapObjArrAndInt safe4{{nullptr, nullptr}, CauseGCInt(isolate)};
    USE(safe4);
    ExtHandleAndInt safe5{kNullExternalPointerHandle, CauseGCInt(isolate)};
    USE(safe5);
    EmbedderSlotAndInt safe6{EmbedderDataSlot(), CauseGCInt(isolate)};
    USE(safe6);
    TwoMemberPtrs safe7{nullptr, (CauseGCInt(isolate), &obj->tagged_field_)};
    USE(safe7);

    // Safe null/constexpr/default-constructed/omitted parenthesized aggregate
    // init cases.
    ExtHandleAndInt safe_paren1(kNullExternalPointerHandle,
                                CauseGCInt(isolate));
    USE(safe_paren1);
    HeapObjPtrAndInt safe_paren2(nullptr, CauseGCInt(isolate));
    USE(safe_paren2);
    EmbedderSlotAndInt safe_paren3(EmbedderDataSlot(), CauseGCInt(isolate));
    USE(safe_paren3);
    TaggedAndInt safe_paren4(Tagged<Object>(), CauseGCInt(isolate));
    USE(safe_paren4);
    IntAndSlot safe_paren5(CauseGCInt(isolate));
    USE(safe_paren5);

    // Unsafe live non-null elements in brace-init and parenthesized aggregate
    // init (re-read live parameters before any GC via separate scopes below).
    EmbedderSlotAndInt bad_slot{EmbedderDataSlot(*js_obj, 0),
                                CauseGCInt(isolate)};
    USE(bad_slot);
    EmbedderSlotAndInt bad_slot_paren(EmbedderDataSlot(*js_obj, 0),
                                      CauseGCInt(isolate));
    USE(bad_slot_paren);
  }

  // Typedef/using alias for `*Member*` pointers registers in `type_raw_vars_`
  // consistently with `Use()`, while `const&` to `nullptr` does not warn.
  {
    MemberPtrAlias out = nullptr;
    InitTaggedMemberPtr(&out);
    CauseGCManaged(42, isolate);
    USE(out);
  }
  {
    MemberPtrAlias const& null_ref = nullptr;
    CauseGCManaged(42, isolate);
    USE(null_ref);
  }

  // `std::get` propagates raw member provenance at container granularity for
  // `std::array<int*, N>` structured bindings and explicit `std::get<0>` calls
  // without false positives on scalar arrays.
  {
    auto [p0, p1] = std::array<int*, 2>{&obj->primitive_field_, nullptr};
    CauseGCManaged(42, isolate);
    *p0 = 1;
    USE(p1);
  }
  {
    std::array<int*, 2> arr = {&obj->primitive_field_, nullptr};
    int* p = std::get<0>(arr);
    CauseGCManaged(42, isolate);
    *p = 1;
  }
  {
    std::array<int, 2> ints = {obj->primitive_field_, 1};
    auto [a, b] = ints;
    CauseGCManaged(42, isolate);
    USE(a);
    USE(b);
  }

  // Global variable assignment is ignored by `IsLocalTrackedVar`.
  {
    g_global_test_ptr = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    USE(g_global_test_ptr);
  }

  // Duff's device: `case:` nested inside a loop merges with the enclosing
  // `switch` entry environment via `Block::FindEnclosingSwitch()`.
  {
    int* p = &obj->primitive_field_;
    switch (switch_tag) {
      case 0:
        do {
          CauseGCManaged(42, isolate);
          [[fallthrough]];
          case 1:
            *p = 1;
        } while (--count > 0);
        break;
      default:
        break;
    }
  }
}

void TestLiveRawNonClassInitLists(HeapObject* live_heap_obj,
                                  ExternalPointerHandle live_ext_handle,
                                  int mode, Isolate* isolate) {
  struct HeapObjPtrAndInt {
    HeapObject* o;
    int x;
  };
  struct HeapObjArrAndInt {
    HeapObject* arr[2];
    int x;
  };
  struct ExtHandleAndInt {
    ExternalPointerHandle h;
    int x;
  };

  // Separate non-sequential branches so `live_heap_obj` and `live_ext_handle`
  // are still live at each initializer list (isolating `BAD_EXPR_MSG` from
  // `DEAD_VAR_MSG`).
  if (mode == 0) {
    ExtHandleAndInt bad_brace_handle{live_ext_handle, CauseGCInt(isolate)};
    USE(bad_brace_handle);
    return;
  }
  if (mode == 1) {
    HeapObjPtrAndInt bad_brace_ptr{live_heap_obj, CauseGCInt(isolate)};
    USE(bad_brace_ptr);
    return;
  }
  if (mode == 2) {
    HeapObjArrAndInt bad_brace_arr{{live_heap_obj, nullptr},
                                   CauseGCInt(isolate)};
    USE(bad_brace_arr);
    return;
  }
  if (mode == 3) {
    ExtHandleAndInt bad_paren_handle(live_ext_handle, CauseGCInt(isolate));
    USE(bad_paren_handle);
    return;
  }
  HeapObjPtrAndInt bad_paren_ptr(live_heap_obj, CauseGCInt(isolate));
  USE(bad_paren_ptr);
}

class HeapObjectWithOnHeapStdArray : public HeapObject {
 public:
  std::array<int, 2> std_arr_;
};

using HeapObjPtrAlias = HeapObject*;

void UnsafeCalleeUsesMemberPtrAfterGC(const TaggedMember<Object>* mp,
                                      Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(mp);
}

void UnsafeCalleeUsesMutableMemberPtrAfterGC(TaggedMember<Object>* mp,
                                             Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(mp);
}

void InitTaggedMemberPtrThatGCs(TaggedMember<Object>** out, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(out);
}

void NonTemplateIfConstexprFalseNoGC(Isolate* isolate) {
  if constexpr (sizeof(int) == 0) {
    CauseGCManaged(42, isolate);
  }
  USE(isolate);
}

void NonTemplateIfConstexprTrueGCs(Isolate* isolate) {
  if constexpr (sizeof(int) > 0) {
    CauseGCManaged(42, isolate);
  }
}

void TestHeapObjPtrTypedefAlias(HeapObjPtrAlias param_alias, Isolate* isolate) {
  HeapObjPtrAlias alias_obj = param_alias;
  CauseGCManaged(42, isolate);
  USE(param_alias);
  USE(alias_obj);
}

void TestNullMemberPtrCastsOnHeapStdGetAndIfConstexpr(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Tagged<HeapObjectWithOnHeapStdArray> raw_std_arr;
  Handle<HeapObjectWithOnHeapStdArray> std_arr_obj =
      handle(raw_std_arr, isolate);

  // Explicit and const-qualified null casts to `*Member*` pointers wrap
  // `CK_NullToPointer` in `CK_NoOp` and must not acquire `kRawMemberAddress`
  // (while real member pointers still warn).
  UnsafeCalleeUsesMemberPtrAfterGC(&obj->tagged_field_, isolate);
  UnsafeCalleeUsesMemberPtrAfterGC(static_cast<TaggedMember<Object>*>(nullptr),
                                   isolate);
  UnsafeCalleeUsesMemberPtrAfterGC(
      static_cast<const TaggedMember<Object>*>(nullptr), isolate);
  {
    struct MemberPtrAndInt {
      TaggedMember<Object>* p;
      int x;
    };
    MemberPtrAndInt safe_null_cast{static_cast<TaggedMember<Object>*>(nullptr),
                                   CauseGCInt(isolate)};
    USE(safe_null_cast);
  }
  {
    MemberPtrAlias const& safe_cast_ref = static_cast<MemberPtrAlias>(nullptr);
    CauseGCManaged(42, isolate);
    USE(safe_cast_ref);
  }
  {
    TaggedMember<Object>* out_cast =
        static_cast<TaggedMember<Object>*>(nullptr);
    InitTaggedMemberPtrThatGCs(&out_cast, isolate);
    // Safe immediately after the out-param initializer call, but stale across a
    // subsequent GC via `type_raw_vars_`.
    USE(out_cast);
    CauseGCManaged(42, isolate);
    USE(out_cast);
  }

  // `std::get` and tuple-protocol structured bindings on an on-heap
  // `std::array<int, 2>` field preserve `kOnHeapMemberLValue` for references
  // and pointers without warning on by-value copies.
  {
    int& r = std::get<0>(std_arr_obj->std_arr_);
    CauseGCManaged(42, isolate);
    r = 1;
  }
  {
    int* p = &std::get<0>(std_arr_obj->std_arr_);
    CauseGCManaged(42, isolate);
    *p = 1;
  }
  UnsafeCalleeUsesRefAfterGC(std::get<0>(std_arr_obj->std_arr_), isolate);
  SafeCalleeReadsRefBeforeGC(std::get<0>(std_arr_obj->std_arr_), isolate);
  {
    auto& [a, b] = std_arr_obj->std_arr_;
    CauseGCManaged(42, isolate);
    a = 1;
    USE(b);
  }
  {
    auto [va, vb] = std_arr_obj->std_arr_;
    CauseGCManaged(42, isolate);
    USE(va);
    USE(vb);
  }

  // Placement `new` constructing an on-heap `*Member` into an untracked `void*`
  // buffer marks `kRawMemberAddress` on the returned prvalue pointer (even
  // without an outer `ImplicitCastExpr`).
  {
    alignas(TaggedMember<Object>) uint8_t raw_buf[sizeof(TaggedMember<Object>)];
    void* raw_void = raw_buf;
    UnsafeCalleeUsesMutableMemberPtrAfterGC(
        new (raw_void) TaggedMember<Object>(), isolate);
    Address placed_addr =
        reinterpret_cast<Address>(new (raw_void) TaggedMember<Object>());
    CauseGCManaged(42, isolate);
    USE(placed_addr);
  }

  // Pass 1 (`CalleesPrinter::TraverseIfStmt`) skips discarded non-dependent
  // `if constexpr` branches so `NonTemplateIfConstexprFalseNoGC` is not marked
  // a GC suspect, while `NonTemplateIfConstexprTrueGCs` still is.
  {
    int* p = &obj->primitive_field_;
    NonTemplateIfConstexprFalseNoGC(isolate);
    *p = 1;
  }
  {
    int* p = &obj->primitive_field_;
    NonTemplateIfConstexprTrueGCs(isolate);
    *p = 1;
  }
}

void InitEmbedderSlotLocal(EmbedderDataSlot* out) { USE(out); }

int InitEmbedderSlotRetInt(EmbedderDataSlot* out) {
  USE(out);
  return 0;
}

int TakeIntOnly(int v) { return v; }

struct MockAllocResult {
  Tagged<JSObject> value;
  bool To(Tagged<JSObject>* out) const {
    *out = value;
    return true;
  }
};

MockAllocResult AllocateMockResultThatGCs(Isolate* isolate) {
  Handle<JSObject> h = isolate->factory()->NewJSObjectWithNullProto();
  CauseGCManaged(42, isolate);
  return MockAllocResult{*h};
}

void HelperUnbracedDoGuardThenGC(Isolate* isolate) {
  do DisableGCMole no_gc;
  while (false);
  CauseGCManaged(42, isolate);
}

void HelperUnbracedIfGuardElseGC(bool cond, Isolate* isolate) {
  if (cond)
    DisableGCMole no_gc;
  else
    CauseGCManaged(42, isolate);
}

void TestUnsequencedTypeRawOutParamsScopeAndHandles(bool cond,
                                                    Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();
  DirectHandle<JSObject> direct_js_obj = js_obj;

  // Type-tracked variables (`EmbedderDataSlot`, `TaggedMember<Object>*`,
  // `Tagged<JSObject>`) defined or out-param-initialized inside unsequenced
  // siblings or pre-call subexpressions are killed across sibling/callee GCs,
  // whereas direct `&out` parameters on receiver-GC chains
  // (`Allocate().To(&o)`) stay live.
  {
    EmbedderDataSlot slot;
    TakeTwoInts(InitEmbedderSlotRetInt(&slot), CauseGCInt(isolate));
    USE(slot);
  }
  {
    struct TwoInts {
      int a;
      int b;
    };
    TaggedMember<Object>* mp = nullptr;
    TwoInts ti((InitTaggedMemberPtr(&mp), 0), CauseGCInt(isolate));
    USE(ti);
    USE(mp);
  }
  {
    TaggedMember<Object>* mp = nullptr;
    TakeTwoInts((CauseGCInt(isolate), InitTaggedMemberPtr(&mp), 0),
                CauseGCInt(isolate));
    USE(mp);
  }
  {
    Tagged<Object> raw_assigned;
    CauseGCRaw(raw_assigned = *js_obj, isolate);
    Print(raw_assigned);
  }
  {
    EmbedderDataSlot slot;
    CauseGCManaged((InitEmbedderSlotLocal(&slot), 42), isolate);
    USE(slot);
  }
  {
    Tagged<JSObject> out_obj;
    if (AllocateMockResultThatGCs(isolate).To(&out_obj)) {
      Print(out_obj);
    }
  }

  // Conditional one-sided rescue in a GC-causing argument is invalidated when a
  // non-GC sibling also assigns raw-member provenance inside a non-GC call.
  {
    int* p = nullptr;
    TakeTwoInts(
        (CauseGCInt(isolate), cond ? (p = &obj->primitive_field_, 0) : 0),
        TakeIntOnly((p = &obj->primitive_field_, 0)));
    *p = 1;
  }

  // Pass 1 (`CalleesPrinter`) isolates `GCScope` for `DoStmt` and unbraced
  // `then`/`else` branches so `DisableGCMole` cannot suppress later GC edges.
  {
    int* p = &obj->primitive_field_;
    HelperUnbracedDoGuardThenGC(isolate);
    *p = 1;
  }
  {
    int* p = &obj->primitive_field_;
    HelperUnbracedIfGuardElseGC(cond, isolate);
    *p = 1;
  }

  // Local mutable references to always-on-heap slots and `*Member*` pointers
  // register in `type_raw_vars_` like reference parameters.
  {
    EmbedderDataSlot slot;
    EmbedderDataSlot& s = slot;
    s = EmbedderDataSlot();
    InitEmbedderSlotLocal(&s);
    CauseGCManaged(42, isolate);
    USE(s);
  }
  {
    MemberPtrAlias mp = nullptr;
    MemberPtrAlias& r = mp;
    r = nullptr;
    InitTaggedMemberPtr(&r);
    CauseGCManaged(42, isolate);
    USE(r);
  }

  // `CanHoldRawMemberAddress` excludes GC-rooted internal and public API V8
  // handles (`HandleBase`, `DirectHandleBase`, `v8::Local`) and pure-scalar
  // structs while still tracking slots.
  {
    struct ScalarOptions {
      int mode;
      bool flag;
    };
    struct SlotAndHandles {
      ObjectSlot slot;
      Handle<JSObject> handle;
      DirectHandle<JSObject> direct_handle;
      v8::Local<v8::Value> api_local;
      ScalarOptions opts;
    };
    SlotAndHandles entry{obj->RawField(0),
                         js_obj,
                         direct_js_obj,
                         v8::Local<v8::Value>(),
                         {1, true}};
    Handle<JSObject> h = entry.handle;
    DirectHandle<JSObject> dh = entry.direct_handle;
    v8::Local<v8::Value> lh = entry.api_local;
    ScalarOptions opt = entry.opts;
    ObjectSlot s = entry.slot;
    CauseGCManaged(42, isolate);
    Print(*h);
    Print(*dh);
    USE(lh);
    USE(opt);
    USE(s);
  }
}

template <typename T>
void SafeCalleeTemplateReadsBeforeGC(const T* p, Isolate* isolate) {
  T v = *p;
  USE(v);
  CauseGCManaged(42, isolate);
}

template <typename T>
void UnsafeCalleeTemplateReadsAfterGC(const T* p, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  T v = *p;
  USE(v);
}

bool CauseGCBool(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return false;
}

void HelperUnbracedDoGuardGCInCond(Isolate* isolate) {
  do DisableGCMole no_gc;
  while (CauseGCBool(isolate));
}

void UnsafeCalleeTakesConstPtrPtr(int* const* pp, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  **pp = 1;
}

void SafeCalleeReadsConstPtrPtrBeforeGC(int* const* pp, Isolate* isolate) {
  int v = **pp;
  USE(v);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeTakesConstSlotPtr(const ObjectSlot* sp, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(*sp);
}

void UnsafeCalleeTakesConstVoidSlotPtr(const void* vp, Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(*static_cast<const ObjectSlot*>(vp));
}

void SafeCalleeReadsConstSlotPtrBeforeGC(const ObjectSlot* sp,
                                         Isolate* isolate) {
  USE(*sp);
  CauseGCManaged(42, isolate);
}

void UnsafeCalleeTakesConstEmbedderSlotPtr(const EmbedderDataSlot* sp,
                                           Isolate* isolate) {
  CauseGCManaged(42, isolate);
  USE(*sp);
}

void SafeCalleeReadsConstEmbedderSlotPtrBeforeGC(const EmbedderDataSlot* sp,
                                                 Isolate* isolate) {
  USE(*sp);
  CauseGCManaged(42, isolate);
}

void CauseGCTakingConstJSObjPtr(const Tagged<JSObject>* p, Isolate* isolate) {
  USE(p);
  CauseGCManaged(42, isolate);
}

void CauseGCTakingUintptr(uintptr_t addr, Isolate* isolate) {
  USE(addr);
  CauseGCManaged(42, isolate);
}

void TestMutableVarConstPtrCastsLoopScopesAndTemplates(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  Handle<JSObject> js_obj = isolate->factory()->NewJSObjectWithNullProto();

  // Instantiated function template bodies are resolved and summarized without
  // false positives when reading before GC, while still catching reads after
  // GC.
  SafeCalleeTemplateReadsBeforeGC<int>(&obj->primitive_field_, isolate);
  UnsafeCalleeTemplateReadsAfterGC<int>(&obj->primitive_field_, isolate);

  // `T* -> const T*` qualification conversions (`const ObjectSlot*`,
  // `int* const*`, `const void*`, `const EmbedderDataSlot*`) evaluate
  // `&mutable_var` and `std::addressof(mutable_var)` as reads + indirect
  // container pointers rather than mutable out-parameters.
  {
    ObjectSlot slot = obj->RawField(0);
    CauseGCManaged(42, isolate);
    ReadConstSlotPtr(&slot);
    ReadConstSlotPtr(std::addressof(slot));
    USE(*slot);
  }
  {
    int* p1 = &obj->primitive_field_;
    UnsafeCalleeTakesConstPtrPtr(&p1, isolate);
    int* p2 = &obj->primitive_field_;
    UnsafeCalleeTakesConstPtrPtr(std::addressof(p2), isolate);
    int* p3 = &obj->primitive_field_;
    SafeCalleeReadsConstPtrPtrBeforeGC(&p3, isolate);
    int* p4 = &obj->primitive_field_;
    SafeCalleeReadsConstPtrPtrBeforeGC(std::addressof(p4), isolate);
  }
  {
    ObjectSlot s1 = obj->RawField(0);
    UnsafeCalleeTakesConstSlotPtr(&s1, isolate);
    ObjectSlot s2 = obj->RawField(0);
    UnsafeCalleeTakesConstSlotPtr(std::addressof(s2), isolate);
    ObjectSlot s3 = obj->RawField(0);
    UnsafeCalleeTakesConstVoidSlotPtr(static_cast<const void*>(&s3), isolate);
    ObjectSlot s4 = obj->RawField(0);
    SafeCalleeReadsConstSlotPtrBeforeGC(&s4, isolate);
    ObjectSlot s5 = obj->RawField(0);
    SafeCalleeReadsConstSlotPtrBeforeGC(std::addressof(s5), isolate);
  }
  {
    EmbedderDataSlot es1;
    InitEmbedderSlotLocal(&es1);
    UnsafeCalleeTakesConstEmbedderSlotPtr(&es1, isolate);
    EmbedderDataSlot es2;
    InitEmbedderSlotLocal(&es2);
    UnsafeCalleeTakesConstEmbedderSlotPtr(std::addressof(es2), isolate);
    EmbedderDataSlot es3;
    InitEmbedderSlotLocal(&es3);
    SafeCalleeReadsConstEmbedderSlotPtrBeforeGC(&es3, isolate);
    EmbedderDataSlot es4;
    InitEmbedderSlotLocal(&es4);
    SafeCalleeReadsConstEmbedderSlotPtrBeforeGC(std::addressof(es4), isolate);
  }
  {
    SomeObjectWithMembers::PtrBox mut_box{&obj->primitive_field_, 1};
    const SomeObjectWithMembers::PtrBox* bp = &mut_box;
    const SomeObjectWithMembers::PtrBox* bp_std = std::addressof(mut_box);
    int* q1 = bp->ptr;
    int* q2 = bp_std->ptr;
    CauseGCManaged(42, isolate);
    *q1 = 1;
    *q2 = 2;
  }
  {
    EmbedderDataSlot eslot;
    InitEmbedderSlotLocal(&eslot);
    const EmbedderDataSlot* esp = std::addressof(eslot);
    UnsafeCalleeTakesEmbedderSlotConstRef(*esp, isolate);
  }

  // `direct_out_params` in `VisitCallExpr` excludes `const`-pointee pointer
  // casts and pointer-to-integer casts on `&raw_obj`, so GC-capable calls kill
  // `raw_obj`.
  {
    Tagged<JSObject> raw_obj = *js_obj;
    CauseGCTakingConstJSObjPtr(&raw_obj, isolate);
    Print(raw_obj);
  }
  {
    Tagged<JSObject> raw_obj = *js_obj;
    CauseGCTakingUintptr(reinterpret_cast<uintptr_t>(&raw_obj), isolate);
    Print(raw_obj);
  }

  // Both Pass 1 (`CalleesPrinter::TraverseDoStmt`) and Pass 2
  // (`FunctionAnalyzer::VisitScopedSubStmt`) isolate an unbraced
  // `DisableGCMole` body in `do body while (cond);` so it cannot suppress GC in
  // `cond`.
  {
    int* p = &obj->primitive_field_;
    HelperUnbracedDoGuardGCInCond(isolate);
    *p = 1;
  }
  {
    int* p = &obj->primitive_field_;
    CauseGCManaged(42, isolate);
    do DisableGCMole no_gc;
    while ((*p = 1, false));
  }

  // Null or default-constructed `const&` local bindings
  // (`IsAmbientMemberOrSlotVar`) do not acquire ambient `kRawMemberAddress`
  // when passed by reference or by value to GC-capable callees.
  {
    const EmbedderDataSlot& empty_slot_ref = EmbedderDataSlot();
    UnsafeCalleeTakesEmbedderSlotConstRef(empty_slot_ref, isolate);
    UnsafeCalleeTakesEmbedderSlotByValue(empty_slot_ref, isolate);
    TaggedMember<Object>* const& null_member_ref = nullptr;
    UnsafeCalleeConstMemberPtrRefAddrOf(null_member_ref, isolate);
    UnsafeCalleeUsesMemberPtrAfterGC(null_member_ref, isolate);
  }
}

void ForwardDeclaredUnsafeFunc(Isolate* isolate);

class OutOfLineUnsafeClass {
 public:
  void OutOfLineLoopWarning(bool cond, Isolate* isolate);
};

void ForwardDeclaredUnsafeFunc(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  int* p = &obj->primitive_field_;
  CauseGCManaged(42, isolate);
  *p = 1;
}

void OutOfLineUnsafeClass::OutOfLineLoopWarning(bool cond, Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);
  int* p = &obj->primitive_field_;
  while (cond) {
    *p = 1;
    CauseGCManaged(42, isolate);
  }
}

struct NarrowShortsView {
  uint16_t halves[sizeof(Address) / sizeof(uint16_t)];
};

void TestBitCastDestinationTypesAndByValueRefDecomp(Isolate* isolate) {
  Tagged<SomeObjectWithMembers> raw_so;
  Handle<SomeObjectWithMembers> obj = handle(raw_so, isolate);

  // `base::bit_cast` gates raw-member provenance on the destination type:
  // floating-point and narrow-scalar structs do not inherit `kRawMemberAddress`
  // and by-value structs do not inherit `kIndirectRawMemberAddress`, whereas
  // pointer-width integers (`intptr_t`) still preserve provenance.
  {
    Address addr = obj->ext_field_.storage_address();
    double safe_bits_d = base::bit_cast<double>(addr);
    NarrowShortsView safe_shorts = base::bit_cast<NarrowShortsView>(addr);
    intptr_t signed_addr = base::bit_cast<intptr_t>(addr);
    int* ptrs[2] = {&obj->primitive_field_, nullptr};
    uintptr_t ptrs_addr = reinterpret_cast<uintptr_t>(ptrs);
    IndirectScalarView safe_indirect_view =
        base::bit_cast<IndirectScalarView>(ptrs_addr);
    CauseGCManaged(42, isolate);
    USE(safe_bits_d);
    USE(safe_shorts);
    USE(safe_indirect_view);
    USE(signed_addr);
  }

  // By-value structured bindings (`auto [...] = expr;`) track individual
  // reference-typed bindings (`int&`, `EmbedderDataSlot&`) without falsely
  // marking sibling scalar copies.
  {
    struct RefAndScalar {
      int& ref;
      int val;
    };
    RefAndScalar rs{obj->primitive_field_, 7};
    auto [bound_ref, bound_val] = rs;
    CauseGCManaged(42, isolate);
    bound_ref = 1;
    USE(bound_val);
  }
  {
    struct EmbedderSlotRefHolder {
      EmbedderDataSlot& slot_ref;
    };
    EmbedderDataSlot backing_slot;
    EmbedderSlotRefHolder holder{backing_slot};
    auto [bound_slot_ref] = holder;
    bound_slot_ref = EmbedderDataSlot();
    InitEmbedderSlotLocal(&bound_slot_ref);
    CauseGCManaged(42, isolate);
    USE(bound_slot_ref);
  }
}

}  // namespace internal
}  // namespace v8
