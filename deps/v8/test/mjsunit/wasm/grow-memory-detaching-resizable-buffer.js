// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-rab-integration

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let module = (() => {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 100);
  builder.addFunction("grow_memory", kSig_i_i)
              .addBody([kExprLocalGet, 0, kExprMemoryGrow, kMemoryZero])
    .exportFunc();
  builder.exportMemoryAs("memory");
  return builder.toModule();
})();

(function TestDetachingViaMemoryAPI() {
  print("TestDetachingViaMemoryAPI...");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 100});
  let growMem = (pages) => memory.grow(pages);

  let b0 = memory.buffer;
  assertFalse(b0.resizable);

  let b1 = memory.toResizableBuffer();
  assertFalse(b0 === b1);
  assertTrue(b0.detached);
  assertEquals(0, b0.byteLength);
  assertTrue(b1.resizable);
  assertEquals(kPageSize, b1.byteLength);
  assertEquals(100 * kPageSize, b1.maxByteLength);

  growMem(0);
  let b2 = memory.buffer;
  assertTrue(b1 === b2);
  assertEquals(kPageSize, b2.byteLength);

  growMem(1);
  let b3 = memory.buffer;
  assertTrue(b1 === b3);
  assertTrue(b2 === b3);
  assertEquals(2 * kPageSize, b3.byteLength);
})();


(function TestDetachingViaArrayBufferAPI() {
  print("TestDetachingViaArrayBufferAPI...");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 100});
  let growMem = (pages) => memory.buffer.resize(
    memory.buffer.byteLength + pages * kPageSize);

  let b0 = memory.buffer;
  assertFalse(b0.resizable);

  let b1 = memory.toResizableBuffer();
  assertFalse(b0 === b1);
  assertTrue(b0.detached);
  assertEquals(0, b0.byteLength);
  assertTrue(b1.resizable);
  assertEquals(kPageSize, b1.byteLength);
  assertEquals(100 * kPageSize, b1.maxByteLength);

  growMem(0);
  let b2 = memory.buffer;
  assertTrue(b1 === b2);
  assertEquals(kPageSize, b2.byteLength);

  growMem(1);
  let b3 = memory.buffer;
  assertTrue(b1 === b3);
  assertTrue(b2 === b3);
  assertEquals(2 * kPageSize, b3.byteLength);
})();

(function TestDetachingViaBytecode() {
  print("TestDetachingViaBytecode...");
  let instance = new WebAssembly.Instance(module);
  let growMem = instance.exports.grow_memory;
  let memory = instance.exports.memory;

  let b0 = memory.buffer;
  assertFalse(b0.resizable);

  let b1 = memory.toResizableBuffer();
  assertFalse(b0 === b1);
  assertTrue(b0.detached);
  assertEquals(0, b0.byteLength);
  assertTrue(b1.resizable);
  assertEquals(kPageSize, b1.byteLength);
  assertEquals(100 * kPageSize, b1.maxByteLength);

  growMem(0);
  let b2 = memory.buffer;
  assertTrue(b1 === b2);
  assertEquals(kPageSize, b2.byteLength);

  growMem(1);
  let b3 = memory.buffer;
  assertTrue(b1 === b3);
  assertTrue(b2 === b3);
  assertEquals(2 * kPageSize, b3.byteLength);
})();
