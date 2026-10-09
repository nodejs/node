// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-generic-wrapper --stack-size=100

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function exhaustAndCall(callFn) {
  let didCall = false;
  let caughtError = null;
  function recurse() {
    try {
      recurse();
    } catch (e) {
      if (!didCall) {
        didCall = true;
        try {
          callFn();
        } catch (callErr) {
          caughtError = callErr;
        }
      }
      throw e;
    }
  }
  assertThrows(recurse, RangeError, /Maximum call stack size exceeded/);
  assertTrue(didCall);
  assertInstanceof(caughtError, RangeError);
  assertEquals('Maximum call stack size exceeded', caughtError.message);
}

const kBoundaryParamCounts = [0, 4, 5, 6, 8, 9];
const kI32Const0x500 = new Array(500).fill([kExprI32Const, 0]).flat();
const kI32Const0x1000 = kI32Const0x500.concat(kI32Const0x500);

const builder = new WasmModuleBuilder();
const sigParams1000 = builder.addType(makeSig(Array(1000).fill(kWasmI32), []));
const sigReturns1000 = builder.addType(makeSig([], Array(1000).fill(kWasmI32)));
const sigParamsReturns500 = builder.addType(
    makeSig(Array(500).fill(kWasmI32), Array(500).fill(kWasmI32)));

builder.addFunction('params1000', sigParams1000).exportFunc().addBody([]);
builder.addFunction('returns1000', sigReturns1000)
    .exportFunc()
    .addBody(kI32Const0x1000);
builder.addFunction('paramsReturns500', sigParamsReturns500)
    .exportFunc()
    .addBody(kI32Const0x500);

for (let paramCount of kBoundaryParamCounts) {
  let sig = builder.addType(makeSig(Array(paramCount).fill(kWasmI32), []));
  builder.addFunction(`params${paramCount}`, sig).exportFunc().addBody([]);
}

const instance = builder.instantiate();
const args1000 = new Array(1000).fill(42);
const args500 = args1000.slice(0, 500);

(function testGenericWrapperPreemptiveStackCheckParams() {
  exhaustAndCall(() => instance.exports.params1000(...args1000));
})();

(function testGenericWrapperPreemptiveStackCheckReturns() {
  exhaustAndCall(() => instance.exports.returns1000());
})();

(function testGenericWrapperPreemptiveStackCheckParamsAndReturns() {
  exhaustAndCall(() => instance.exports.paramsReturns500(...args500));
})();

(function testGenericWrapperPreemptiveStackCheckRegisterBoundary() {
  for (let paramCount of kBoundaryParamCounts) {
    let fn = instance.exports[`params${paramCount}`];
    let args = args1000.slice(0, paramCount);
    fn(...args);
    exhaustAndCall(() => fn(...args));
  }
})();

(function testGenericWrapperPreemptiveStackCheckJSPIParams() {
  if (typeof WebAssembly.promising !== 'function') return;
  let promisingParams1000 = WebAssembly.promising(instance.exports.params1000);
  exhaustAndCall(() => promisingParams1000(...args1000));
})();

(function testGenericWrapperPreemptiveStackCheckJSPIReturns() {
  if (typeof WebAssembly.promising !== 'function') return;
  let promisingReturns1000 =
      WebAssembly.promising(instance.exports.returns1000);
  exhaustAndCall(() => promisingReturns1000());
})();
