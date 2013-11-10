// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "global-handles.h"

#include "cctest.h"

using namespace v8::internal;
using v8::UniqueId;


static List<Object*> skippable_objects;
static List<Object*> can_skip_called_objects;


static bool CanSkipCallback(Heap* heap, Object** pointer) {
  can_skip_called_objects.Add(*pointer);
  return skippable_objects.Contains(*pointer);
}


static void ResetCanSkipData() {
  skippable_objects.Clear();
  can_skip_called_objects.Clear();
}


class TestRetainedObjectInfo : public v8::RetainedObjectInfo {
 public:
  TestRetainedObjectInfo() : has_been_disposed_(false) {}

  bool has_been_disposed() { return has_been_disposed_; }

  virtual void Dispose() {
    ASSERT(!has_been_disposed_);
    has_been_disposed_ = true;
  }

  virtual bool IsEquivalent(v8::RetainedObjectInfo* other) {
    return other == this;
  }

  virtual intptr_t GetHash() { return 0; }

  virtual const char* GetLabel() { return "whatever"; }

 private:
  bool has_been_disposed_;
};


class TestObjectVisitor : public ObjectVisitor {
 public:
  virtual void VisitPointers(Object** start, Object** end) {
    for (Object** o = start; o != end; ++o)
      visited.Add(*o);
  }

  List<Object*> visited;
};


TEST(IterateObjectGroupsOldApi) {
  CcTest::InitializeVM();
  GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  Heap* heap = CcTest::heap();
  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> g1s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g1s2 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());

  Handle<Object> g2s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g2s2 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());

  TestRetainedObjectInfo info1;
  TestRetainedObjectInfo info2;
  {
    Object** g1_objects[] = { g1s1.location(), g1s2.location() };
    Object** g2_objects[] = { g2s1.location(), g2s2.location() };

    global_handles->AddObjectGroup(g1_objects, 2, &info1);
    global_handles->AddObjectGroup(g2_objects, 2, &info2);
  }

  // Iterate the object groups. First skip all.
  {
    ResetCanSkipData();
    skippable_objects.Add(*g1s1.location());
    skippable_objects.Add(*g1s2.location());
    skippable_objects.Add(*g2s1.location());
    skippable_objects.Add(*g2s2.location());
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 4);
    ASSERT(can_skip_called_objects.Contains(*g1s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g1s2.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s2.location()));

    // Nothing was visited.
    ASSERT(visitor.visited.length() == 0);
    ASSERT(!info1.has_been_disposed());
    ASSERT(!info2.has_been_disposed());
  }

  // Iterate again, now only skip the second object group.
  {
    ResetCanSkipData();
    // The first grough should still be visited, since only one object is
    // skipped.
    skippable_objects.Add(*g1s1.location());
    skippable_objects.Add(*g2s1.location());
    skippable_objects.Add(*g2s2.location());
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 3 ||
           can_skip_called_objects.length() == 4);
    ASSERT(can_skip_called_objects.Contains(*g1s2.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s2.location()));

    // The first group was visited.
    ASSERT(visitor.visited.length() == 2);
    ASSERT(visitor.visited.Contains(*g1s1.location()));
    ASSERT(visitor.visited.Contains(*g1s2.location()));
    ASSERT(info1.has_been_disposed());
    ASSERT(!info2.has_been_disposed());
  }

  // Iterate again, don't skip anything.
  {
    ResetCanSkipData();
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 1);
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()) ||
           can_skip_called_objects.Contains(*g2s2.location()));

    // The second group was visited.
    ASSERT(visitor.visited.length() == 2);
    ASSERT(visitor.visited.Contains(*g2s1.location()));
    ASSERT(visitor.visited.Contains(*g2s2.location()));
    ASSERT(info2.has_been_disposed());
  }
}


TEST(IterateObjectGroups) {
  CcTest::InitializeVM();
  GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  Heap* heap = CcTest::heap();

  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> g1s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g1s2 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());

  Handle<Object> g2s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g2s2 =
    global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());

  TestRetainedObjectInfo info1;
  TestRetainedObjectInfo info2;
  global_handles->SetObjectGroupId(g2s1.location(), UniqueId(2));
  global_handles->SetObjectGroupId(g2s2.location(), UniqueId(2));
  global_handles->SetRetainedObjectInfo(UniqueId(2), &info2);
  global_handles->SetObjectGroupId(g1s1.location(), UniqueId(1));
  global_handles->SetObjectGroupId(g1s2.location(), UniqueId(1));
  global_handles->SetRetainedObjectInfo(UniqueId(1), &info1);

  // Iterate the object groups. First skip all.
  {
    ResetCanSkipData();
    skippable_objects.Add(*g1s1.location());
    skippable_objects.Add(*g1s2.location());
    skippable_objects.Add(*g2s1.location());
    skippable_objects.Add(*g2s2.location());
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 4);
    ASSERT(can_skip_called_objects.Contains(*g1s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g1s2.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s2.location()));

    // Nothing was visited.
    ASSERT(visitor.visited.length() == 0);
    ASSERT(!info1.has_been_disposed());
    ASSERT(!info2.has_been_disposed());
  }

  // Iterate again, now only skip the second object group.
  {
    ResetCanSkipData();
    // The first grough should still be visited, since only one object is
    // skipped.
    skippable_objects.Add(*g1s1.location());
    skippable_objects.Add(*g2s1.location());
    skippable_objects.Add(*g2s2.location());
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 3 ||
           can_skip_called_objects.length() == 4);
    ASSERT(can_skip_called_objects.Contains(*g1s2.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()));
    ASSERT(can_skip_called_objects.Contains(*g2s2.location()));

    // The first group was visited.
    ASSERT(visitor.visited.length() == 2);
    ASSERT(visitor.visited.Contains(*g1s1.location()));
    ASSERT(visitor.visited.Contains(*g1s2.location()));
    ASSERT(info1.has_been_disposed());
    ASSERT(!info2.has_been_disposed());
  }

  // Iterate again, don't skip anything.
  {
    ResetCanSkipData();
    TestObjectVisitor visitor;
    global_handles->IterateObjectGroups(&visitor, &CanSkipCallback);

    // CanSkipCallback was called for all objects.
    ASSERT(can_skip_called_objects.length() == 1);
    ASSERT(can_skip_called_objects.Contains(*g2s1.location()) ||
           can_skip_called_objects.Contains(*g2s2.location()));

    // The second group was visited.
    ASSERT(visitor.visited.length() == 2);
    ASSERT(visitor.visited.Contains(*g2s1.location()));
    ASSERT(visitor.visited.Contains(*g2s2.location()));
    ASSERT(info2.has_been_disposed());
  }
}


TEST(ImplicitReferences) {
  CcTest::InitializeVM();
  GlobalHandles* global_handles = CcTest::i_isolate()->global_handles();
  Heap* heap = CcTest::heap();

  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> g1s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g1c1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g1c2 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());


  Handle<Object> g2s1 =
      global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g2s2 =
    global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g2c1 =
    global_handles->Create(heap->AllocateFixedArray(1)->ToObjectChecked());

  global_handles->SetObjectGroupId(g1s1.location(), UniqueId(1));
  global_handles->SetObjectGroupId(g2s1.location(), UniqueId(2));
  global_handles->SetObjectGroupId(g2s2.location(), UniqueId(2));
  global_handles->SetReferenceFromGroup(UniqueId(1), g1c1.location());
  global_handles->SetReferenceFromGroup(UniqueId(1), g1c2.location());
  global_handles->SetReferenceFromGroup(UniqueId(2), g2c1.location());

  List<ImplicitRefGroup*>* implicit_refs =
      global_handles->implicit_ref_groups();
  USE(implicit_refs);
  ASSERT(implicit_refs->length() == 2);
  ASSERT(implicit_refs->at(0)->parent ==
         reinterpret_cast<HeapObject**>(g1s1.location()));
  ASSERT(implicit_refs->at(0)->length == 2);
  ASSERT(implicit_refs->at(0)->children[0] == g1c1.location());
  ASSERT(implicit_refs->at(0)->children[1] == g1c2.location());
  ASSERT(implicit_refs->at(1)->parent ==
         reinterpret_cast<HeapObject**>(g2s1.location()));
  ASSERT(implicit_refs->at(1)->length == 1);
  ASSERT(implicit_refs->at(1)->children[0] == g2c1.location());
}


TEST(EternalHandles) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  EternalHandles* eternal_handles = isolate->eternal_handles();

  // Create a number of handles that will not be on a block boundary
  const int kArrayLength = 2048-1;
  int indices[kArrayLength];
  v8::Eternal<v8::Value> eternals[kArrayLength];

  CHECK_EQ(0, eternal_handles->NumberOfHandles());
  for (int i = 0; i < kArrayLength; i++) {
    indices[i] = -1;
    HandleScope scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New();
    object->Set(i, v8::Integer::New(i, v8_isolate));
    // Create with internal api
    eternal_handles->Create(
        isolate, *v8::Utils::OpenHandle(*object), &indices[i]);
    // Create with external api
    CHECK(eternals[i].IsEmpty());
    eternals[i].Set(v8_isolate, object);
    CHECK(!eternals[i].IsEmpty());
  }

  isolate->heap()->CollectAllAvailableGarbage();

  for (int i = 0; i < kArrayLength; i++) {
    for (int j = 0; j < 2; j++) {
      HandleScope scope(isolate);
      v8::Local<v8::Value> local;
      if (j == 0) {
        // Test internal api
        local = v8::Utils::ToLocal(eternal_handles->Get(indices[i]));
      } else {
        // Test external api
        local = eternals[i].Get(v8_isolate);
      }
      v8::Local<v8::Object> object = v8::Handle<v8::Object>::Cast(local);
      v8::Local<v8::Value> value = object->Get(i);
      CHECK(value->IsInt32());
      CHECK_EQ(i, value->Int32Value());
    }
  }

  CHECK_EQ(2*kArrayLength, eternal_handles->NumberOfHandles());

  // Create an eternal via the constructor
  {
    HandleScope scope(isolate);
    v8::Local<v8::Object> object = v8::Object::New();
    v8::Eternal<v8::Object> eternal(v8_isolate, object);
    CHECK(!eternal.IsEmpty());
    CHECK(object == eternal.Get(v8_isolate));
  }

  CHECK_EQ(2*kArrayLength + 1, eternal_handles->NumberOfHandles());
}
