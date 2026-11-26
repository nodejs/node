// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestConstLocalSet() {
  for (const [type, constExpr, values] of [
         [kWasmI32, wasmI32Const, [0, -1, 0x12345678]],
         [kWasmI64, wasmI64Const, [0n, -1n, 0x1234567890abcdefn]],
         [kWasmF32, wasmF32Const,
          [0, -0, Math.fround(13.37), Infinity, -Infinity, NaN]],
         [kWasmF64, wasmF64Const,
          [0, -0, 13.37, Number.MIN_VALUE, Number.MAX_VALUE,
           Infinity, -Infinity, NaN]]]) {
    const builder = new WasmModuleBuilder();
    for (let i = 0; i < values.length; ++i) {
      const constant = constExpr(values[i]);
      builder.addFunction(`set${i}`, makeSig([], [type]))
          .addLocals(type, 1)
          .addBody([
            ...constant,
            kExprLocalSet, 0,
            ...constant,
            kExprLocalSet, 0,
            kExprLocalGet, 0,
          ])
          .exportFunc();
      builder.addFunction(`preserve${i}`, makeSig([type], [type, type]))
          .addBody([
            kExprLocalGet, 0,
            ...constant,
            kExprLocalSet, 0,
            kExprLocalGet, 0,
          ])
          .exportFunc();
    }
    const instance = builder.instantiate();
    const initial = type === kWasmI64 ? 42n : 42;
    for (let i = 0; i < values.length; ++i) {
      assertEquals(values[i], instance.exports[`set${i}`]());
      assertEquals([initial, values[i]],
                   instance.exports[`preserve${i}`](initial));
    }
  }
})();
