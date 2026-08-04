// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
let $sig0 = builder.addType(makeSig([kWasmF32, kWasmI32, kWasmF32, kWasmF64], [kWasmF32]));
let w0 = builder.addFunction('w0', $sig0)
             .addBody([kExprLocalGet, 2])
             .exportFunc();
const instance = builder.instantiate({});

function f() {
  try {
    instance.exports.w0();
  } catch (e) {
    throw e;
  }
  const v20 = 'SYMBOL'['toUpperCase']();
  print(v20);
  v21 = v20.link();
}
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
