// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// f64 exception payloads must be preserved exactly by the Wasm interpreter,
// not narrowed to float precision when unpacked.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestF64ExceptionPayload() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  const tag = builder.addTag(makeSig([kWasmF64], []));
  const values = [1 + 2 ** -40, 0.1, 1e300, -1e-300, 5e-324];
  values.forEach((value, i) => {
    builder.addFunction(`catch_f64_${i}`, makeSig([], [kWasmF64]))
        .addBody([
          kExprTry, kWasmF64,
            ...wasmF64Const(value),
            kExprThrow, tag,
          kExprCatch, tag,
          kExprEnd,
        ])
        .exportFunc();
  });
  const instance = builder.instantiate();
  values.forEach((value, i) => {
    assertEquals(value, instance.exports[`catch_f64_${i}`]());
  });
})();
