// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-simd-opt

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function Crash467479137() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  let main = builder.addFunction("main", kSig_i_iii)
  .exportFunc()
  .addBody([
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kExprI32Const, 0,
    kSimdPrefix, kExprI8x16Splat,
    kSimdPrefix, kExprI8x16Shuffle, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    ...SimdInstr(kExprI64x2SConvertI32x4Low),
    ...SimdInstr(kExprI64x2SConvertI32x4Low),
    ...SimdInstr(kExprI32x4BitMask),
  ]);

  const instance = builder.instantiate({});
  instance.exports.main(1, 2, 3);
})();
