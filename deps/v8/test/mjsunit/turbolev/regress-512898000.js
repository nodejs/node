// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing --no-lazy-feedback-allocation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function tryCatch(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

const builder = new WasmModuleBuilder();
builder.addFunction('w', makeSig([kWasmI64, kWasmF32], []))
    .addBody([])
    .exportFunc();
const instance = tryCatch(() => builder.instantiate());
const exportedFn = tryCatch(() => instance.exports.w);
function outer() {
  function getFn() {
    return exportedFn;
  }
  const target = tryCatch(() => getFn());
  try {
    target.call(target, outer, target);
  } catch (e) {}
}
tryCatch(() => outer());
outer();
%OptimizeFunctionOnNextCall(outer);
outer();
