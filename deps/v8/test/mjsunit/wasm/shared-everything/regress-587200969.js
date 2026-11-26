// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Module A creates a shared WasmArray and exports it to JS.
const sharedBuilder = new WasmModuleBuilder();
const sharedArrType = sharedBuilder.addArray(kWasmI32, {shared: true});
sharedBuilder.addFunction(
    'makeSharedArr', makeSig([], [wasmRefType(sharedArrType)]))
    .addBody([
      kExprI32Const, 10,
      kGCPrefix, kExprArrayNewDefault, sharedArrType,
    ])
    .exportFunc();
const sharedArr = sharedBuilder.instantiate().exports.makeSharedArr();

// Module B does not use any shared types itself, so its detected_features()
// does not include `shared`. However, under --wasm-shared, JS can still pass
// a shared WasmArray via externref, and abstract casts to unshared arrayref
// must still reject shared arrays even when TurboFan optimizes the JS caller.
const unsharedBuilder = new WasmModuleBuilder();
const unsharedArrType = unsharedBuilder.addArray(kWasmI32);
unsharedBuilder.addFunction(
    'makeUnsharedArr', makeSig([], [wasmRefType(unsharedArrType)]))
    .addBody([
      kExprI32Const, 5,
      kGCPrefix, kExprArrayNewDefault, unsharedArrType,
    ])
    .exportFunc();

unsharedBuilder.addFunction('castArrayLen', kSig_i_r)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, kArrayRefCode,
      kGCPrefix, kExprArrayLen,
    ])
    .exportFunc();

unsharedBuilder.addFunction('castNullArray', makeSig([kWasmExternRef], []))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCastNull, kArrayRefCode,
      kExprDrop,
    ])
    .exportFunc();

const unsharedExports = unsharedBuilder.instantiate().exports;
const unsharedArr = unsharedExports.makeUnsharedArr();

function callCastArrayLen(arr) {
  return unsharedExports.castArrayLen(arr);
}
%PrepareFunctionForOptimization(callCastArrayLen);
assertEquals(5, callCastArrayLen(unsharedArr));
%OptimizeFunctionOnNextCall(callCastArrayLen);
assertEquals(5, callCastArrayLen(unsharedArr));
assertTraps(kTrapIllegalCast, () => callCastArrayLen(sharedArr));

function callCastNullArray(arr) {
  return unsharedExports.castNullArray(arr);
}
%PrepareFunctionForOptimization(callCastNullArray);
callCastNullArray(unsharedArr);
callCastNullArray(null);
%OptimizeFunctionOnNextCall(callCastNullArray);
callCastNullArray(unsharedArr);
callCastNullArray(null);
assertTraps(kTrapIllegalCast, () => callCastNullArray(sharedArr));
