// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-rab-integration

(function TestToResizableMaxPages() {
  let mem = new WebAssembly.Memory({initial: 1, maximum: 2**16});
  assertEquals(2**16, mem.buffer.maxByteLength);
  let ab = mem.toResizableBuffer();
  assertEquals(2**32, ab.maxByteLength);
  let smem = new WebAssembly.Memory({initial: 1, maximum: 2**16, shared: true});
  assertEquals(2**16, smem.buffer.maxByteLength);
  let sab = smem.toResizableBuffer();
  assertEquals(2**32, sab.maxByteLength);
  let first_unsafe_max_pages = (BigInt(Number.MAX_SAFE_INTEGER) + 1n) / (2n ** 16n);
  assertThrows(() => new WebAssembly.Memory(
      {initial: 1n, maximum: first_unsafe_max_pages, address: 'i64'}),
      RangeError);
})();
