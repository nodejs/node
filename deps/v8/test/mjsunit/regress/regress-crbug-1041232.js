// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --multi-mapped-mock-allocator

// Chosen for stress runs on 32-bit systems. Physical memory is not an issue
// thanks to the mock allocator, but virtual address space is still limited.
let kSize = 128 * 1024 * 1024;
// Must be >= MultiMappedMockAllocator::kChunkSize in d8.cc.
let kChunkSize = 2 * 1024 * 1024;
let a = new Uint8Array(kSize);

for (let i = 0; i < kChunkSize; i++) {
  a[i] = 42;
}

// Check that OOB accesses return undefined and all array elements are 42.
// Importantly, nothing crashes.
assertEquals(undefined, a[-kChunkSize - 1]);
assertEquals(undefined, a[-kChunkSize]);
assertEquals(undefined, a[-1]);
assertEquals(42, a[0]);
assertEquals(42, a[1]);
// If this fails, then you probably tried to run this test without the
// multi-mapped mock allocator.
assertEquals(42, a[kChunkSize]);
assertEquals(42, a[kChunkSize + 1]);
assertEquals(42, a[kChunkSize + 1]);
assertEquals(42, a[kSize - kChunkSize]);
assertEquals(42, a[kSize - 1]);
assertEquals(undefined, a[kSize]);
assertEquals(undefined, a[kSize + 1]);
assertEquals(undefined, a[kSize + kChunkSize]);
assertEquals(undefined, a[kSize + kSize]);

// Check that excessive requests throw (crbug.com/1042173, crbug.com/1042151).
assertThrows(() => new ArrayBuffer(Number.MAX_SAFE_INTEGER));
