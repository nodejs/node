// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-debug-code

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const pairType = builder.addStruct([
  makeField(kWasmExternRef, true),
  makeField(kWasmExternRef, true),
]);
const externArrType = builder.addArray(kWasmExternRef);
const pairArrType = builder.addArray(wasmRefNullType(pairType));
const f64ArrType = builder.addArray(kWasmF64);

builder.addFunction('id', makeSig([kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

builder.addFunction('arrLen', makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, kArrayRefCode,
      kGCPrefix, kExprArrayLen,
    ])
    .exportFunc();

builder.addFunction('setExternArr', makeSig(
    [kWasmExternRef, kWasmExternRef, kWasmExternRef, kWasmI32, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, externArrType,
      kExprLocalGet, 3, kExprLocalGet, 1,
      kGCPrefix, kExprArraySet, externArrType,
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, externArrType,
      kExprLocalGet, 4, kExprLocalGet, 2,
      kGCPrefix, kExprArraySet, externArrType,
    ])
    .exportFunc();

builder.addFunction('writeField0', makeSig(
    [kWasmExternRef, kWasmExternRef, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, pairArrType,
      kExprLocalGet, 2,
      kGCPrefix, kExprArrayGet, pairArrType,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructSet, pairType, 0,
    ])
    .exportFunc();

builder.addFunction('writeField1', makeSig(
    [kWasmExternRef, kWasmExternRef, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, pairArrType,
      kExprLocalGet, 2,
      kGCPrefix, kExprArrayGet, pairArrType,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructSet, pairType, 1,
    ])
    .exportFunc();

builder.addFunction('roundtripF64', makeSig(
    [kWasmExternRef, kWasmI32, kWasmF64], [kWasmF64]))
    .addBody([
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, f64ArrType,
      kExprLocalGet, 1, kExprLocalGet, 2,
      kGCPrefix, kExprArraySet, f64ArrType,
      kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, f64ArrType,
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayGet, f64ArrType,
    ])
    .exportFunc();

builder.addFunction('truncF64', makeSig([kWasmF64], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprI64SConvertF64,
      kExprI32ConvertI64,
      kExprI32Const, 0,
      kExprI32And,
    ])
    .exportFunc();

builder.addFunction('mkExternArr', makeSig([], [wasmRefType(externArrType)]))
    .addBody([kExprI32Const, 2, kGCPrefix, kExprArrayNewDefault, externArrType])
    .exportFunc();

builder.addFunction('mkPairArr', makeSig([], [wasmRefType(pairArrType)]))
    .addBody([
      kGCPrefix, kExprStructNewDefault, pairType,
      kGCPrefix, kExprStructNewDefault, pairType,
      kGCPrefix, kExprArrayNewFixed, pairArrType, 2,
    ])
    .exportFunc();

builder.addFunction('mkF64Arr', makeSig([], [wasmRefType(f64ArrType)]))
    .addBody([kExprI32Const, 2, kGCPrefix, kExprArrayNewDefault, f64ArrType])
    .exportFunc();

const {
  id,
  arrLen,
  setExternArr,
  writeField0,
  writeField1,
  roundtripF64,
  truncF64,
  mkExternArr,
  mkPairArr,
  mkF64Arr,
} = builder.instantiate().exports;

const extArr = mkExternArr();
const pairArr = mkPairArr();
const f64Arr = mkF64Arr();
const constObj = {x: 1};

const u32View = new Uint32Array(8);
const f64View = new Float64Array(u32View.buffer);

function keepRegsLive(val) {
  let m0 = u32View[0];
  let m1 = u32View[1];
  let m2 = u32View[4];
  f64View[2] = val;
  u32View[3] = m0 ^ m1 ^ m2;
  return val;
}

const K = 2 ** 34;
const step = x => x + x - K;
const step4 = x => step(step(step(step(x))));
const step16 = x => step4(step4(step4(step4(x))));

function unreachableLoad(a, d, doubleArr, smiArr, z, off) {
  let neg = (roundtripF64(f64Arr, 0, -2147483648) | 0) >> 1;
  if (arrLen(a) === z) {
    setExternArr(a, doubleArr, smiArr, 0, 1);
  } else {
    setExternArr(a, doubleArr, smiArr, 0, 1);
  }
  writeField0(d, constObj, 0);
  writeField1(d, neg, 1);
  let q = Math.max(-1024, (0 - smiArr.length) | 0);
  let t = step16(step4(step4(step4(step(step(step(K + q)))))));
  let idx = t - K - 1 + (off | 0);
  return keepRegsLive(doubleArr[id(0) ? -0 : idx]);
}

function main(a, doubleArr, smiArr, z, takeDead) {
  let r;
  if (takeDead) {
    r = unreachableLoad(a, a, doubleArr, smiArr, z, 0);
  } else {
    let v = doubleArr[0];
    u32View[0] = truncF64(v + 128);
    r = v;
  }
  return r + 0.2;
}

%PrepareFunctionForOptimization(step);
%PrepareFunctionForOptimization(step4);
%PrepareFunctionForOptimization(step16);
%PrepareFunctionForOptimization(keepRegsLive);
%PrepareFunctionForOptimization(unreachableLoad);
%PrepareFunctionForOptimization(main);

for (let i = 0; i < 20; i++) {
  unreachableLoad(extArr, pairArr, [1.5], [], (i % 2) * 2, 1);
  main(extArr, [3.4], [], i % 2, false);
  assertThrows(
      () => main(extArr, [3.4], [], i % 2, true), WebAssembly.RuntimeError);
}

%OptimizeFunctionOnNextCall(main);
assertEquals(3.6, main(extArr, [3.4], [], 0, false));
