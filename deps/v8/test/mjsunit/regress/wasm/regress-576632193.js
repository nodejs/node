// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $s = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  descriptor: $desc,
  final: true,
});
/* $desc */ builder.addStruct({fields: [], describes: $s, final: true});
let $other = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  final: true,
});
builder.endRecGroup();

let $g_desc = builder.addGlobal(
    wasmRefType($desc).exact(), false, false,
    [kGCPrefix, kExprStructNewDefault, $desc]);

builder.addFunction('make', makeSig([kWasmI32], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_desc.index,
      kGCPrefix, kExprStructNewDesc, $s,
      kGCPrefix, kExprExternConvertAny,
    ]);

builder.addFunction('makeOther', makeSig([kWasmI32], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructNew, $other,
      kGCPrefix, kExprExternConvertAny,
    ]);

builder.addFunction('get', makeSig([kWasmExternRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCast, $s,
      kGCPrefix, kExprStructGet, $s, 0,
    ]);

builder.addFunction('castNull', makeSig([kWasmExternRef], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprAnyConvertExtern,
      kGCPrefix, kExprRefCastNull, $s,
      kGCPrefix, kExprExternConvertAny,
    ]);

let instance = builder.instantiate();
let {make, makeOther, get, castNull} = instance.exports;
let obj = make(42);
let other = makeOther(99);

function testGet(o) {
  return get(o);
}

function testCastNull(o) {
  return castNull(o);
}

%PrepareFunctionForOptimization(testGet);
%PrepareFunctionForOptimization(testCastNull);
for (let i = 0; i < 10; i++) {
  assertEquals(42, testGet(obj));
  assertSame(obj, testCastNull(obj));
  assertNull(testCastNull(null));
}

%OptimizeFunctionOnNextCall(testGet);
assertEquals(42, testGet(obj));
assertTraps(kTrapIllegalCast, () => testGet(other));
assertTraps(kTrapIllegalCast, () => testGet({}));
assertTraps(kTrapIllegalCast, () => testGet(123));
assertTraps(kTrapIllegalCast, () => testGet(null));

%OptimizeFunctionOnNextCall(testCastNull);
assertSame(obj, testCastNull(obj));
assertNull(testCastNull(null));
assertTraps(kTrapIllegalCast, () => testCastNull(other));
assertTraps(kTrapIllegalCast, () => testCastNull({}));
assertTraps(kTrapIllegalCast, () => testCastNull(123));
