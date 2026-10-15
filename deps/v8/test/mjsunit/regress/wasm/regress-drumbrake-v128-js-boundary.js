// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestV128HostBoundaryThrows() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('v128Param', makeSig([kWasmS128], [kWasmI32]))
      .addBody([kExprI32Const, 0])
      .exportFunc();
  builder.addFunction('v128Return', makeSig([], [kWasmS128]))
      .addBody(wasmS128Const(0, 0))
      .exportFunc();
  builder.addFunction(
      'MultiValueV128Return', makeSig([], [kWasmI32, kWasmS128]))
      .addBody([...wasmI32Const(42), ...wasmS128Const(0, 0)])
      .exportFunc();

  const instance = builder.instantiate();
  assertThrows(() => instance.exports.v128Param(), TypeError);
  assertThrows(() => instance.exports.v128Return(), TypeError);
  assertThrows(() => instance.exports.MultiValueV128Return(), TypeError);
})();

(function TestNestedJSToWasmContextLoad() {
  const builder = new WasmModuleBuilder();
  const reduceRight = builder.addImport(
      'env', 'reduceRight', makeSig([kWasmFuncRef], []));
  const callback = builder.addFunction(
      'callback', makeSig([kWasmS128], []))
      .addBody([kExprUnreachable])
      .index;

  builder.addFunction('callReduceRight', kSig_v_v)
      .addBody([
        kExprRefFunc, callback,
        kExprCallFunction, reduceRight,
      ])
      .exportFunc();
  builder.addDeclarativeElementSegment([callback]);

  const array = [0, , 1];
  Object.setPrototypeOf(array, new Proxy(Array.prototype, {}));
  const instance = builder.instantiate({
    env: {reduceRight: Array.prototype.reduceRight.bind(array)},
  });

  assertThrows(() => instance.exports.callReduceRight(), TypeError);
})();
