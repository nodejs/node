// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/vm-cage.h"
#include "test/cctest/cctest.h"

#ifdef V8_VIRTUAL_MEMORY_CAGE

namespace v8 {
namespace internal {

UNINITIALIZED_TEST(VirtualMemoryCageCreation) {
  base::PageAllocator page_allocator;

  V8VirtualMemoryCage cage;

  CHECK(!cage.is_initialized());
  CHECK(!cage.is_disabled());
  CHECK_EQ(cage.size(), 0);

  CHECK(cage.Initialize(&page_allocator));

  CHECK(cage.is_initialized());
  CHECK_GT(cage.base(), 0);
  CHECK_GT(cage.size(), 0);

  cage.TearDown();

  CHECK(!cage.is_initialized());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_VIRTUAL_MEMORY_CAGE
