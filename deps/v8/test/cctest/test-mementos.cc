// Copyright 2014 the V8 project authors. All rights reserved.
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

#include "cctest.h"

using namespace v8::internal;


static void SetUpNewSpaceWithPoisonedMementoAtTop() {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  NewSpace* new_space = heap->new_space();

  // Make sure we can allocate some objects without causing a GC later.
  heap->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  // Allocate a string, the GC may suspect a memento behind the string.
  Handle<SeqOneByteString> string = isolate->factory()->NewRawOneByteString(12);
  CHECK(*string);

  // Create an allocation memento behind the string with a garbage allocation
  // site pointer.
  AllocationMemento* memento =
      reinterpret_cast<AllocationMemento*>(new_space->top() + kHeapObjectTag);
  memento->set_map_no_write_barrier(heap->allocation_memento_map());
  memento->set_allocation_site(
      reinterpret_cast<AllocationSite*>(kHeapObjectTag), SKIP_WRITE_BARRIER);
}


TEST(Regress340063) {
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());


  SetUpNewSpaceWithPoisonedMementoAtTop();

  // Call GC to see if we can handle a poisonous memento right after the
  // current new space top pointer.
  CcTest::i_isolate()->heap()->CollectAllGarbage(
      Heap::kAbortIncrementalMarkingMask);
}


TEST(BadMementoAfterTopForceScavenge) {
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());

  SetUpNewSpaceWithPoisonedMementoAtTop();

  // Force GC to test the poisoned memento handling
  CcTest::i_isolate()->heap()->CollectGarbage(i::NEW_SPACE);
}


TEST(PretenuringCallNew) {
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  if (!i::FLAG_pretenuring_call_new) return;

  v8::HandleScope scope(CcTest::isolate());
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();

  // We need to create several instances to get past the slack-tracking
  // phase, where mementos aren't emitted.
  int call_count = 10;
  CHECK_GE(call_count, SharedFunctionInfo::kGenerousAllocationCount);
  i::ScopedVector<char> test_buf(1024);
  const char* program =
      "function f() {"
      "  this.a = 3;"
      "  this.b = {};"
      "  return this;"
      "};"
      "var a;"
      "for(var i = 0; i < %d; i++) {"
      "  a = new f();"
      "}"
      "a;";
  i::OS::SNPrintF(test_buf, program, call_count);
  v8::Local<v8::Value> res = CompileRun(test_buf.start());
  Handle<JSObject> o =
      v8::Utils::OpenHandle(*v8::Handle<v8::Object>::Cast(res));

  // The object of class f should have a memento secreted behind it.
  Address memento_address = o->address() + o->map()->instance_size();
  AllocationMemento* memento =
      reinterpret_cast<AllocationMemento*>(memento_address + kHeapObjectTag);
  CHECK_EQ(memento->map(), heap->allocation_memento_map());

  // Furthermore, how many mementos did we create? The count should match
  // call_count - SharedFunctionInfo::kGenerousAllocationCount.
  AllocationSite* site = memento->GetAllocationSite();
  CHECK_EQ(call_count - SharedFunctionInfo::kGenerousAllocationCount,
           site->pretenure_create_count()->value());
}
