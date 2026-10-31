// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(kSig_i_v);
const $i8Array = builder.addArray(kWasmI8);
const $funcArray = builder.addArray(kWasmFuncRef);

const f42 = builder.addFunction('f42', sig).addBody([...wasmI32Const(42)]);
const f43 = builder.addFunction('f43', sig).addBody([...wasmI32Const(43)]);
const $data = builder.addPassiveDataSegment([42, 43, 44, 45]);
const $elem = builder.addPassiveElementSegment([f42.index, f43.index]);

function callFuncAt(index) {
  return [
    ...wasmI32Const(index),
    kGCPrefix, kExprArrayGet, $funcArray,
    kGCPrefix, kExprRefCast, sig,
    kExprCallRef, sig,
  ];
}

builder.addFunction('newData', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayNewData, $i8Array, $data,
      ...wasmI32Const(1),
      kGCPrefix, kExprArrayGetU, $i8Array,
    ])
    .exportFunc();

builder.addFunction('initData', kSig_i_i)
    .addLocals(wasmRefNullType($i8Array), 1)
    .addBody([
      ...wasmI32Const(4),
      kGCPrefix, kExprArrayNewDefault, $i8Array,
      kExprLocalSet, 1,
      kExprLocalGet, 1,
      ...wasmI32Const(1),
      kExprLocalGet, 0,
      ...wasmI32Const(3),
      kGCPrefix, kExprArrayInitData, $i8Array, $data,
      kExprLocalGet, 1,
      ...wasmI32Const(3),
      kGCPrefix, kExprArrayGetU, $i8Array,
    ])
    .exportFunc();

builder.addFunction('newElem', kSig_i_ii)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayNewElem, $funcArray, $elem,
      ...callFuncAt(0),
    ])
    .exportFunc();

builder.addFunction('initElem', kSig_i_i)
    .addLocals(wasmRefNullType($funcArray), 1)
    .addBody([
      ...wasmI32Const(2),
      kGCPrefix, kExprArrayNewDefault, $funcArray,
      kExprLocalSet, 1,
      kExprLocalGet, 1,
      ...wasmI32Const(1),
      kExprLocalGet, 0,
      ...wasmI32Const(1),
      kGCPrefix, kExprArrayInitElem, $funcArray, $elem,
      kExprLocalGet, 1,
      ...callFuncAt(1),
    ])
    .exportFunc();

const exports = builder.instantiate().exports;

assertEquals(43, exports.newData(0, 4));
assertEquals(45, exports.newData(2, 2));
assertTraps(kTrapDataSegmentOutOfBounds, () => exports.newData(3, 2));

assertEquals(45, exports.initData(1));
assertTraps(kTrapDataSegmentOutOfBounds, () => exports.initData(2));

assertEquals(42, exports.newElem(0, 2));
assertEquals(43, exports.newElem(1, 1));
assertTraps(kTrapElementSegmentOutOfBounds, () => exports.newElem(1, 2));

assertEquals(43, exports.initElem(1));
assertTraps(kTrapElementSegmentOutOfBounds, () => exports.initElem(2));
