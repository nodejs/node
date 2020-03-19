// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd

load('test/mjsunit/wasm/wasm-module-builder.js');

// This test case tries to exercise SIMD stack to stack movements by creating
// a function that has many parameters.
(function() {
const builder = new WasmModuleBuilder();
// At this point we have limited support for SIMD operations, but we can load
// and store s128 values. So this memory will be used for reading and writing
// s128 values and we will assert expected values from JS.
builder.addImportedMemory('m', 'imported_mem', 1, 2);
builder.addType(makeSig(new Array(18).fill(kWasmS128), []));

builder.addFunction(undefined, makeSig([], []))
    .addLocals({s128_count: 9})
    .addBodyWithEnd([
      // These will all be args to the callee.
      // Load first arg from memory, this was written with values from JS.
      kExprI32Const, 0,                     // i32.const
      kSimdPrefix, kExprS128LoadMem, 0, 0,  // s128.load
      kExprLocalGet, 0,                     // local.get
      kExprLocalGet, 1,                     // local.get
      kExprLocalGet, 2,                     // local.get
      kExprLocalGet, 3,                     // local.get
      kExprLocalGet, 4,                     // local.get
      kExprLocalGet, 5,                     // local.get
      kExprLocalGet, 6,                     // local.get
      kExprLocalGet, 7,                     // local.get
      kExprLocalGet, 8,                     // local.get
      kExprLocalGet, 0,                     // local.get
      kExprLocalGet, 1,                     // local.get
      kExprLocalGet, 2,                     // local.get
      kExprLocalGet, 3,                     // local.get
      kExprLocalGet, 4,                     // local.get
      kExprLocalGet, 5,                     // local.get
      kExprLocalGet, 6,                     // local.get
      // Load last s128 from memory, this was written with values from JS.
      kExprI32Const, 16,                    // i32.const
      kSimdPrefix, kExprS128LoadMem, 0, 0,  // s128.load
      kExprCallFunction, 0x01,              // call
      kExprEnd,                             // end
    ]);

builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
  kExprI32Const, 32,                     // i32.const
  kExprLocalGet, 0,                      // local.get
  kSimdPrefix, kExprS128StoreMem, 0, 0,  // s128.store
  kExprI32Const, 48,                     // i32.const
  kExprLocalGet, 17,                     // local.get
  kSimdPrefix, kExprS128StoreMem, 0, 0,  // s128.store
  kExprEnd,                              // end
]);

builder.addExport('main', 0);
var memory = new WebAssembly.Memory({initial: 1, maximum: 2});
const instance = builder.instantiate({m: {imported_mem: memory}});

// We write sentinel values to two s128 values at the start of the memory.
// Function 1 will read these values from memory, and pass them as the first
// and last arg to function 2. Function 2 then write these values to memory
// after these two s128 values.
const arr = new Uint32Array(memory.buffer);
for (let i = 0; i < 8; i++) {
  arr[0] = i * 2;
}

instance.exports.main();

for (let i = 0; i < 8; i++) {
  assertEquals(arr[i], arr[i + 8]);
}
})();
