// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --mock-arraybuffer-allocator --allow-natives-syntax

if (%Is64Bit()) {
  let mem =
      new WebAssembly.Memory({initial: 1n, maximum: 100_000n, address: 'i64'});

  let buf = mem.toResizableBuffer();
  assertEquals(0x1_0000, buf.byteLength);
  buf.resize(0x1_0001_0000);
  assertEquals(0x1_0001_0000, buf.byteLength);
  assertEquals(0x1_0001_0000, mem.buffer.byteLength);
}
