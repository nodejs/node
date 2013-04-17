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

static int NumberOfWeakCalls = 0;
static void WeakPointerCallback(v8::Isolate* isolate,
                                v8::Persistent<v8::Value> handle,
                                void* id) {
  ASSERT(id == reinterpret_cast<void*>(1234));
  NumberOfWeakCalls++;
  handle.Dispose(isolate);
}

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
  GlobalHandles* global_handles = Isolate::Current()->global_handles();

  v8::HandleScope handle_scope(CcTest::isolate());

  Handle<Object> g1s1 =
      global_handles->Create(HEAP->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g1s2 =
      global_handles->Create(HEAP->AllocateFixedArray(1)->ToObjectChecked());
  global_handles->MakeWeak(g1s1.location(),
                           reinterpret_cast<void*>(1234),
                           NULL,
                           &WeakPointerCallback);
  global_handles->MakeWeak(g1s2.location(),
                           reinterpret_cast<void*>(1234),
                           NULL,
                           &WeakPointerCallback);

  Handle<Object> g2s1 =
      global_handles->Create(HEAP->AllocateFixedArray(1)->ToObjectChecked());
  Handle<Object> g2s2 =
      global_handles->Create(HEAP->AllocateFixedArray(1)->ToObjectChecked());
  global_handles->MakeWeak(g2s1.location(),
                           reinterpret_cast<void*>(1234),
                           NULL,
                           &WeakPointerCallback);
  global_handles->MakeWeak(g2s2.location(),
                           reinterpret_cast<void*>(1234),
                           NULL,
                           &WeakPointerCallback);

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
    fprintf(stderr, "can skip len %d\n", can_skip_called_objects.length());
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
