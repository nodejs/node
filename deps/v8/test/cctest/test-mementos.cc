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

#include "src/heap/factory.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

static void SetUpNewSpaceWithPoisonedMementoAtTop() {
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = isolate->heap();
  NewSpace* new_space = heap->new_space();

  // Make sure we can allocate some objects without causing a GC later.
  CcTest::CollectAllGarbage();

  // Allocate a string, the GC may suspect a memento behind the string.
  Handle<SeqOneByteString> string =
      isolate->factory()->NewRawOneByteString(12).ToHandleChecked();
  CHECK(*string);

  // Create an allocation memento behind the string with a garbage allocation
  // site pointer.
  AllocationMemento* memento =
      reinterpret_cast<AllocationMemento*>(new_space->top() + kHeapObjectTag);
  memento->set_map_after_allocation(heap->allocation_memento_map(),
                                    SKIP_WRITE_BARRIER);
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
  CcTest::CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
}


TEST(Regress470390) {
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());

  SetUpNewSpaceWithPoisonedMementoAtTop();

  // Set the new space limit to be equal to the top.
  Address top = CcTest::i_isolate()->heap()->new_space()->top();
  *(CcTest::i_isolate()->heap()->new_space()->allocation_limit_address()) = top;

  // Call GC to see if we can handle a poisonous memento right after the
  // current new space top pointer.
  CcTest::CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
}


TEST(BadMementoAfterTopForceScavenge) {
  CcTest::InitializeVM();
  if (!i::FLAG_allocation_site_pretenuring) return;
  v8::HandleScope scope(CcTest::isolate());

  SetUpNewSpaceWithPoisonedMementoAtTop();

  // Force GC to test the poisoned memento handling
  CcTest::CollectGarbage(i::NEW_SPACE);
}

}  // namespace internal
}  // namespace v8
