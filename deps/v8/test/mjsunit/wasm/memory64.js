// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-memory64

load('test/mjsunit/wasm/wasm-module-builder.js');

// We use standard JavaScript doubles to represent bytes and offsets. They offer
// enough precision (53 bits) for every allowed memory size.

function BasicMemory64Tests(num_pages) {
  const num_bytes = num_pages * kPageSize;
  print(`Testing ${num_bytes} bytes (${num_pages} pages)`);

  let builder = new WasmModuleBuilder();
  builder.addMemory64(num_pages, num_pages, true);

  builder.addFunction('load', makeSig([kWasmF64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,       // local.get 0
        kExprI64UConvertF64,    // i64.uconvert_sat.f64
        kExprI32LoadMem, 0, 0,  // i32.load_mem align=1 offset=0
      ])
      .exportFunc();
  builder.addFunction('store', makeSig([kWasmF64, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,        // local.get 0
        kExprI64UConvertF64,     // i64.uconvert_sat.f64
        kExprLocalGet, 1,        // local.get 1
        kExprI32StoreMem, 0, 0,  // i32.store_mem align=1 offset=0
      ])
      .exportFunc();

  let module = builder.instantiate();
  let memory = module.exports.memory;
  let load = module.exports.load;
  let store = module.exports.store;

  let array = new Int8Array(memory.buffer);
  assertEquals(num_bytes, array.length);

  assertEquals(0, load(num_bytes - 4));
  assertThrows(() => load(num_bytes - 3));

  store(num_bytes - 4, 0x12345678);
  assertEquals(0x12345678, load(num_bytes - 4));

  let kStoreOffset = 27;
  store(kStoreOffset, 11);
  assertEquals(11, load(kStoreOffset));

  // Now check 100 random positions.
  for (let i = 0; i < 100; ++i) {
    let position = Math.floor(Math.random() * num_bytes);
    let expected = 0;
    if (position == kStoreOffset) {
      expected = 11;
    } else if (num_bytes - position <= 4) {
      expected = [0x12, 0x34, 0x56, 0x78][num_bytes - position - 1];
    }
    assertEquals(expected, array[position]);
  }
}

(function TestSmallMemory() {
  print(arguments.callee.name);
  BasicMemory64Tests(4);
})();

(function Test3GBMemory() {
  print(arguments.callee.name);
  let num_pages = 3 * 1024 * 1024 * 1024 / kPageSize;
  // This test can fail if 3GB of memory cannot be allocated.
  try {
    BasicMemory64Tests(num_pages);
  } catch (e) {
    assertInstanceof(e, RangeError);
    assertMatches(/Out of memory/, e.message);
  }
})();

// TODO(clemensb): Allow for memories >4GB and enable this test.
//(function Test5GBMemory() {
//  print(arguments.callee.name);
//  let num_pages = 5 * 1024 * 1024 * 1024 / kPageSize;
//  BasicMemory64Tests(num_pages);
//})();

(function TestGrow64() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 10, false);

  builder.addFunction('grow', makeSig([kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,    // local.get 0
        kExprMemoryGrow, 0,  // memory.grow 0
      ])
      .exportFunc();

  let instance = builder.instantiate();

  assertEquals(1n, instance.exports.grow(2n));
  assertEquals(3n, instance.exports.grow(1n));
  assertEquals(-1n, instance.exports.grow(-1n));
  assertEquals(-1n, instance.exports.grow(1n << 31n));
  assertEquals(-1n, instance.exports.grow(1n << 32n));
  assertEquals(-1n, instance.exports.grow(1n << 33n));
  assertEquals(-1n, instance.exports.grow(1n << 63n));
  assertEquals(-1n, instance.exports.grow(7n));  // Above the of 10.
  assertEquals(4n, instance.exports.grow(6n));   // Just at the maximum of 10.
})();
