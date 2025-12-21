// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev --no-maglev --turbolev-inline-js-wasm-wrappers
// Flags: --allow-natives-syntax --trace-deopt --trace-deopt-verbose

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

const iterationCount = 100;
let arrayIndex = 0;

function createWasmModuleForLazyDeopt(returnType, createValue, callback) {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  let index = builder.addArray(kWasmI32, true);
  assertEquals(arrayIndex, index);
  let callbackIndex = builder.addImport('env', 'callback', kSig_v_i);

  builder.addFunction("triggerDeopt", makeSig([kWasmI32], [returnType]))
    .addLocals(kWasmI32, 1)
    .addBody([
      ...wasmI32Const(1032),
      ...wasmI32Const(1032),
      kExprI32LoadMem, 0, 0,
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalTee, 1,
      kExprI32StoreMem, 0, 0,
      kExprBlock, kWasmVoid,
      kExprLocalGet, 1,
      ...wasmI32Const(iterationCount),
      kExprI32Ne,
      kExprBrIf, 0,
      kExprLocalGet, 0,
      kExprCallFunction, callbackIndex,
      kExprEnd,
      ...createValue,
    ]).exportFunc();

  return builder.instantiate({ env: { callback } });
}

{ // Test with a f32 result, which needs materialization.
  var globalForI32 = 0;
  let { triggerDeopt } = createWasmModuleForLazyDeopt(kWasmF32,
    [...wasmF32Const(3.14)], () => globalForI32 = 1).exports;

  function test(arg0) {
    var result = 0;
    for (let i = 0; i < iterationCount + 5; i++) {
      if (i == 2) % OptimizeOsr();
      result = triggerDeopt(arg0 + globalForI32);
    }
    return result;
  }

  %PrepareFunctionForOptimization(test);
  assertEqualsDelta(3.14, test(0));
  %OptimizeFunctionOnNextCall(test);

  // CHECK: [marking dependent code {{.*}} <Code TURBOFAN_JS> ({{.*}} <SharedFunctionInfo test>)
  // CHECK: [bailout (kind: deopt-lazy, reason: (unknown))
  // CHECK: reading JS to Wasm builtin continuation frame test
  // CHECK: translating BuiltinContinuation to JSToWasmLazyDeoptContinuation
  // CHECK: Materialization {{.*}} <HeapNumber 3.14>
  assertEqualsDelta(3.14, test(0));
}
