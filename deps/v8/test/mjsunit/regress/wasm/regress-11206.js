// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-mv --expose-gc --stress-compaction
// Flags: --stress-scavenge=16

load('test/mjsunit/wasm/wasm-module-builder.js');

(function TestReturnOddNumberOfReturns() {
  let builder = new WasmModuleBuilder();
  let void_sig = builder.addType(kSig_v_v);
  let mv_sig = builder.addType(
      makeSig([], [kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32]));

  let gc_index = builder.addImport('q', 'gc', void_sig);
  builder.addFunction('main', mv_sig)
      .addBodyWithEnd([
        kExprCallFunction, gc_index,
        kExprI32Const, 1,
        kExprI32Const, 2,
        kExprI32Const, 3,
        kExprI32Const, 4,
        kExprI32Const, 5,
        kExprEnd
      ])
      .exportFunc();

  let instance = builder.instantiate({q: {gc: gc}});

  instance.exports.main();
})();

(function TestReturnEvenNumberOfReturns() {
  let builder = new WasmModuleBuilder();
  let void_sig = builder.addType(kSig_v_v);
  let mv_sig =
      builder.addType(makeSig([], [kWasmI32, kWasmI32, kWasmI32, kWasmI32]));

  let gc_index = builder.addImport('q', 'gc', void_sig);
  builder.addFunction('main', mv_sig)
      .addBodyWithEnd([
        kExprCallFunction, gc_index,
        kExprI32Const, 1,
        kExprI32Const, 2,
        kExprI32Const, 3,
        kExprI32Const, 4,
        kExprEnd
      ])
      .exportFunc();

  let instance = builder.instantiate({q: {gc: gc}});

  instance.exports.main();
})();
