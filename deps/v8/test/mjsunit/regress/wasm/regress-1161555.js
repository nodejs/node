// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

// Test case copied from clusterfuzz, this exercises a bug in WasmCompileLazy
// where we are not correctly pushing the full 128-bits of a SIMD register.
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const __v_0 = new WasmModuleBuilder();
__v_0.addImportedMemory('m', 'imported_mem');
__v_0.addFunction('main', makeSig([], [])).addBodyWithEnd([
  kExprI32Const, 0, kSimdPrefix, kExprS128LoadMem, 0, 0, kExprCallFunction,
  0x01, kExprEnd
]);
__v_0.addFunction('function2', makeSig([kWasmS128], [])).addBodyWithEnd([
  kExprI32Const, 17, kExprLocalGet, 0, kSimdPrefix, kExprS128StoreMem, 0, 0,
  kExprI32Const, 9, kExprLocalGet, 0, kExprCallFunction, 0x02, kExprEnd
]);
__v_0.addFunction('function3', makeSig([kWasmI32, kWasmS128], []))
    .addBodyWithEnd([
      kExprI32Const, 32, kExprLocalGet, 1, kSimdPrefix, kExprS128StoreMem, 0, 0,
      kExprEnd
    ]);
__v_0.addExport('main');
var __v_1 = new WebAssembly.Memory({
  initial: 1,
});
const __v_2 = __v_0.instantiate({m: {imported_mem: __v_1}});
const __v_3 = new Uint8Array(__v_1.buffer);
for (let __v_4 = 0; __v_4 < 16; __v_4++) {
  __v_3[__v_4] = __v_4 * 2;
}
__v_2.exports.main();
for (let __v_5 = 0; __v_5 < 16; __v_5++) {
  assertEquals(__v_3[__v_5], __v_3[__v_5 + 32]);
}
