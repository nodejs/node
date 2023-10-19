// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-mem-pages=49152
// Save some memory on Linux; other platforms ignore this flag.
// Flags: --multi-mapped-mock-allocator

// This test makes sure things don't break once we support >2GB wasm memories.
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testHugeMemory() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  const num_pages = 49152;  // 3GB

  builder.addMemory(num_pages, num_pages);
  builder.addFunction("geti", kSig_i_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI32Mul,
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

  var module = builder.instantiate();
  const geti = module.exports.geti;

  print("In bounds");
  assertEquals(0, geti(2500, 1 << 20));
  print("Out of bounds");
  assertTraps(kTrapMemOutOfBounds, () => geti(3500, 1 << 20));
})();

(function testHugeMemoryConstInBounds() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  const num_pages = 49152;  // 3GB

  builder.addMemory(num_pages, num_pages);
  builder.addFunction("geti", kSig_i_v)
    .addBody([
      kExprI32Const, 0x80, 0x80, 0x80, 0x80, 0x7A, // 0xA0000000, 2.5GB
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

  var module = builder.instantiate();
  const geti = module.exports.geti;

  print("In bounds");
  assertEquals(0, geti());
})();

(function testHugeMemoryConstOutOfBounds() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();

  const num_pages = 49152;  // 3GB

  builder.addMemory(num_pages, num_pages);
  builder.addFunction("geti", kSig_i_v)
    .addBody([
      kExprI32Const, 0x80, 0x80, 0x80, 0x80, 0x7E, // 0xE0000000, 3.5GB
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

  var module = builder.instantiate();
  const geti = module.exports.geti;

  print("Out of bounds");
  assertTraps(kTrapMemOutOfBounds, geti);
})();

(function testGrowHugeMemory() {
  print(arguments.callee.name);

  let mem = new WebAssembly.Memory({initial: 1});
  mem.grow(49151);
})();
