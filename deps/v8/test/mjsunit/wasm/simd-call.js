// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Tests function calls containing s128 parameters. It also checks that
// lowering of simd calls are correct, so we create 2 functions with s128
// arguments: function 2 has a single s128 parameter, function 3 has a i32 then
// s128, to ensure that the arguments in different indices are correctly lowered.
(function TestSimd128Params() {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'imported_mem', 1, 2);

  builder
    .addFunction("main", makeSig([], []))
    .addBodyWithEnd([
      kExprI32Const, 0,
      kSimdPrefix, kExprS128LoadMem, 0, 0,
      kExprCallFunction, 0x01,
      kExprEnd,
    ]);

  // Writes s128 argument to memory starting byte 16.
  builder
    .addFunction("function2", makeSig([kWasmS128], []))
    .addBodyWithEnd([
      kExprI32Const, 16,
      kExprLocalGet, 0,
      kSimdPrefix, kExprS128StoreMem, 0, 0,
      kExprI32Const, 9, // This constant doesn't matter.
      kExprLocalGet, 0,
      kExprCallFunction, 0x02,
      kExprEnd,
    ]);

  // Writes s128 argument to memory starting byte 32.
  builder
    .addFunction("function3", makeSig([kWasmI32, kWasmS128], []))
    .addBodyWithEnd([
      kExprI32Const, 32,
      kExprLocalGet, 1,
      kSimdPrefix, kExprS128StoreMem, 0, 0,
      kExprEnd,
    ]);

  builder.addExport('main', 0);
  var memory = new WebAssembly.Memory({initial:1, maximum:2});
  const instance = builder.instantiate({m: {imported_mem: memory}});

  const arr = new Uint8Array(memory.buffer);
  // Fill the initial memory with some values, this is read by main and passed
  // as arguments to function2, and then to function3.
  for (let i = 0; i < 16; i++) {
    arr[i] = i * 2;
  }

  instance.exports.main();

  for (let i = 0; i < 16; i++) {
    assertEquals(arr[i], arr[i+16]);
    assertEquals(arr[i], arr[i+32]);
  }
})();
