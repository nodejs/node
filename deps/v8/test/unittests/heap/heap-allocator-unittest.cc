// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-isolate.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/heap/local-factory-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/slots-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using HeapAllocatorTest = TestWithIsolate;

TEST_F(HeapAllocatorTest, TryResizeLargeObject) {
  if (!COMPRESS_POINTERS_BOOL) return;
  Isolate* i_isolate = this->i_isolate();
  HeapAllocator* allocator = i_isolate->main_thread_local_heap()->allocator();
  v8::Isolate::Scope isolate_scope(this->v8_isolate());
  HandleScope scope(i_isolate);
  constexpr size_t kInitialElements = 64 * KB;
  constexpr size_t kResizedElements = 2 * kInitialElements;
  Handle<FixedArray> array = i_isolate->factory()->NewFixedArray(
      kInitialElements, AllocationType::kYoung);
  ReadOnlyRoots roots{i_isolate};

  for (size_t i = kInitialElements; i < kResizedElements; i++) {
    CHECK(allocator->TryResizeLargeObject(
        *array, FixedArray::SizeFor(static_cast<int>(i)),
        FixedArray::SizeFor(static_cast<int>(i + 1))));
    array->set_capacity(static_cast<int>(i + 1));
    MemsetTagged((*array)->RawFieldOfElementAt(static_cast<int>(i)),
                 roots.undefined_value(), 1);

    if (i % 20'000 == 0) {
      InvokeMinorGC();
    }
  }

  CHECK_EQ(roots.undefined_value(), array->get(0));
  CHECK_EQ(roots.undefined_value(), array->get(kInitialElements - 1));
  CHECK_EQ(roots.undefined_value(), array->get(kInitialElements));
  CHECK_EQ(roots.undefined_value(), array->get(kResizedElements - 1));

  InvokeMajorGC();
}

}  // namespace internal
}  // namespace v8
