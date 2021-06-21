// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/spaces.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-tester.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

HEAP_TEST(WriteBarrier_Marking) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  MarkCompactCollector* collector = isolate->heap()->mark_compact_collector();
  HandleScope outer(isolate);
  Handle<FixedArray> objects = factory->NewFixedArray(3);
  {
    // Make sure that these objects are not immediately reachable from
    // the roots to prevent them being marked grey at the start of marking.
    HandleScope inner(isolate);
    Handle<FixedArray> host = factory->NewFixedArray(1);
    Handle<HeapNumber> value1 = factory->NewHeapNumber(1.1);
    Handle<HeapNumber> value2 = factory->NewHeapNumber(1.2);
    objects->set(0, *host);
    objects->set(1, *value1);
    objects->set(2, *value2);
  }
  heap::SimulateIncrementalMarking(CcTest::heap(), false);
  FixedArray host = FixedArray::cast(objects->get(0));
  HeapObject value1 = HeapObject::cast(objects->get(1));
  HeapObject value2 = HeapObject::cast(objects->get(2));
  CHECK(collector->marking_state()->IsWhite(host));
  CHECK(collector->marking_state()->IsWhite(value1));
  WriteBarrier::Marking(host, host.RawFieldOfElementAt(0), value1);
  CHECK_EQ(V8_CONCURRENT_MARKING_BOOL,
           collector->marking_state()->IsGrey(value1));
  collector->marking_state()->WhiteToGrey(host);
  collector->marking_state()->GreyToBlack(host);
  CHECK(collector->marking_state()->IsWhite(value2));
  WriteBarrier::Marking(host, host.RawFieldOfElementAt(0), value2);
  CHECK(collector->marking_state()->IsGrey(value2));
  heap::SimulateIncrementalMarking(CcTest::heap(), true);
  CHECK(collector->marking_state()->IsBlack(host));
  CHECK(collector->marking_state()->IsBlack(value1));
  CHECK(collector->marking_state()->IsBlack(value2));
}

HEAP_TEST(WriteBarrier_MarkingExtension) {
  ManualGCScope manual_gc_scope;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  MarkCompactCollector* collector = isolate->heap()->mark_compact_collector();
  HandleScope outer(isolate);
  Handle<FixedArray> objects = factory->NewFixedArray(1);
  ArrayBufferExtension* extension;
  {
    HandleScope inner(isolate);
    Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(CcTest::isolate(), 100);
    Handle<JSArrayBuffer> host = v8::Utils::OpenHandle(*ab);
    extension = host->extension();
    objects->set(0, *host);
  }
  heap::SimulateIncrementalMarking(CcTest::heap(), false);
  JSArrayBuffer host = JSArrayBuffer::cast(objects->get(0));
  CHECK(collector->marking_state()->IsWhite(host));
  CHECK(!extension->IsMarked());
  WriteBarrier::Marking(host, extension);
  CHECK_EQ(V8_CONCURRENT_MARKING_BOOL, extension->IsMarked());
  heap::SimulateIncrementalMarking(CcTest::heap(), true);
  CHECK(collector->marking_state()->IsBlack(host));
  CHECK(extension->IsMarked());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
