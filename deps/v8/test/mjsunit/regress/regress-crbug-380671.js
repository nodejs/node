// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator --expose-gc

var buffer = new ArrayBuffer(0xc0000000);
assertEquals(0xc0000000, buffer.byteLength);
// We call the GC here to free up the large array buffer. Otherwise, the
// mock allocator would allow us to allocate more than the physical memory
// available on 32bit platforms, leaving the internal counters in an invalid
// state.
gc();
