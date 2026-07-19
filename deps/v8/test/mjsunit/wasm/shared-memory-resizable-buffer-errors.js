// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-rab-integration

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestGrowNonPageMultiple() {
  print("TestGrowNonPageMultiple...");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 2, shared: true});
  let buf = memory.toResizableBuffer();
  assertEquals("SharedArrayBuffer", buf.constructor.name);
  assertThrows(() => buf.grow(kPageSize + 1), RangeError);
})();
