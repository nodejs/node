// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const set = new Set([10, 20, 30, 40]);
const next = set.values().next;

// Eager deopt inside an inlined builtin callback in Array.prototype.forEach
// must resume at the current index rather than re-running earlier iterations.
(function TestForEachInlinedBuiltinEagerDeopt() {
  function runForEach(arr) {
    arr.forEach(Function.prototype.call, next);
  }

  %PrepareFunctionForOptimization(runForEach);
  for (let i = 0; i < 5; i++) {
    const it = set.values();
    runForEach([it, it]);
  }
  %OptimizeFunctionOnNextCall(runForEach);

  const it = set.values();
  assertThrows(() => runForEach([it, it, 'not_an_iterator']), TypeError);
  assertEquals(30, it.next().value);
})();

// Eager deopt inside an inlined builtin callback in Array.prototype.map
// must resume at the current index rather than re-running earlier iterations.
(function TestMapInlinedBuiltinEagerDeopt() {
  function runMap(arr) {
    return arr.map(Function.prototype.call, next);
  }

  %PrepareFunctionForOptimization(runMap);
  for (let i = 0; i < 5; i++) {
    const it = set.values();
    runMap([it, it]);
  }
  %OptimizeFunctionOnNextCall(runMap);

  const it = set.values();
  assertThrows(() => runMap([it, it, 'not_an_iterator']), TypeError);
  assertEquals(30, it.next().value);
})();

// Eager deopt in ProcessWasmArgument when calling a Wasm callback from
// Array.prototype.map and Array.prototype.forEach under Turbolev.
(function TestWasmCallbackArgumentEagerDeopt() {
  if (typeof WebAssembly === 'undefined') return;
  d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
  const builder = new WasmModuleBuilder();
  const counter = builder.addGlobal(kWasmI32, true, false);
  builder.addExportOfKind('counter', kExternalGlobal, counter.index);
  builder
      .addFunction(
          'incAndAdd',
          makeSig([kWasmI32, kWasmI32, kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprGlobalGet, counter.index,
        kExprI32Const, 1,
        kExprI32Add,
        kExprGlobalSet, counter.index,
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Add,
      ])
      .exportFunc();
  const instance = builder.instantiate();
  const wasmCb = instance.exports.incAndAdd;

  function makePackedArray(a, b, c) {
    const arr = [1, 2, {}];
    arr[0] = a;
    arr[1] = b;
    arr[2] = c;
    return arr;
  }

  function runWasmMap(arr) {
    return arr.map(wasmCb);
  }

  function runWasmForEach(arr) {
    arr.forEach(wasmCb);
  }

  %PrepareFunctionForOptimization(runWasmMap);
  %PrepareFunctionForOptimization(runWasmForEach);
  const warm = makePackedArray(10, 20, 30);
  for (let i = 0; i < 5; i++) {
    runWasmMap(warm);
    runWasmForEach(warm);
  }
  %OptimizeFunctionOnNextCall(runWasmMap);
  %OptimizeFunctionOnNextCall(runWasmForEach);

  instance.exports.counter.value = 0;
  const res = runWasmMap(makePackedArray(10, 20, {valueOf() { return 30; }}));
  assertEquals([11, 21, 31], res);
  assertEquals(3, instance.exports.counter.value);

  instance.exports.counter.value = 0;
  runWasmForEach(makePackedArray(10, 20, {valueOf() { return 30; }}));
  assertEquals(3, instance.exports.counter.value);

  // Passing Array.prototype.map as the callback of an outer Array.prototype.map
  // must not nest EagerDeoptFrameScopes.
  function runNestedWasmMap(outer, inner) {
    inner[0] = 10;
    return outer.map(Array.prototype.map, inner);
  }

  %PrepareFunctionForOptimization(runNestedWasmMap);
  for (let i = 0; i < 5; i++) {
    runNestedWasmMap([wasmCb, wasmCb], makePackedArray(10, 20, 30));
  }
  %OptimizeFunctionOnNextCall(runNestedWasmMap);

  instance.exports.counter.value = 0;
  const nestedRes = runNestedWasmMap(
      [wasmCb, wasmCb],
      makePackedArray(10, 20, {valueOf() { return 30; }}));
  assertEquals([[11, 21, 31], [11, 21, 31]], nestedRes);
  assertEquals(6, instance.exports.counter.value);
})();
