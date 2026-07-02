// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/local-heap.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/managed.h"
#include "src/objects/maybe-object.h"
#include "src/objects/object-macros.h"
#include "src/objects/visitors.h"

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
template <typename T, typename CompressionScheme>
class TaggedMember;

JSDispatchHandleMember* ReturnRawJSDispatchHandleMember(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return nullptr;
}

JSDispatchHandle* ReturnRawJSDispatchHandle(Isolate* isolate) {
  CauseGCManaged(42, isolate);
  return nullptr;
}

void DummyTakePointers(JSDispatchHandleMember* a, JSDispatchHandleMember* b) {}
void DummyTakePointers2(JSDispatchHandle* a, JSDispatchHandle* b) {}

void TestJSDispatchHandleMemberEvalOrder(Isolate* isolate) {
  // Should cause warning.
  DummyTakePointers(ReturnRawJSDispatchHandleMember(isolate),
                    ReturnRawJSDispatchHandleMember(isolate));
}

void TestJSDispatchHandleEvalOrder(Isolate* isolate) {
  // Should cause warning.
  DummyTakePointers2(ReturnRawJSDispatchHandle(isolate),
                     ReturnRawJSDispatchHandle(isolate));
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

void TestJSDispatchHandleDeadVar(JSDispatchHandle* raw_handle,
                                 Isolate* isolate) {
  CauseGCManaged(42, isolate);
  // Should cause warning.
  USE(raw_handle);
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

}  // namespace internal
}  // namespace v8
