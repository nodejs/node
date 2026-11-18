// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
const $desc = builder.nextTypeIndex() + 1;
const $struct = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  descriptor: $desc,
});
builder.addStruct({
  fields: [makeField(kWasmExternRef, false)],
  describes: $struct,
});
builder.endRecGroup();

const $proto = builder.addImportedGlobal('m', 'proto', kWasmExternRef);
const $g_desc1 = builder.addGlobal(wasmRefType($desc).exact(), false, false, [
  kExprGlobalGet, $proto,
  kGCPrefix, kExprStructNew, $desc,
]);
const $g_desc2 = builder.addGlobal(wasmRefType($desc).exact(), false, false, [
  kExprGlobalGet, $proto,
  kGCPrefix, kExprStructNew, $desc,
]);

builder.addFunction('make1', makeSig([kWasmI32], [kWasmAnyRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_desc1.index,
      kGCPrefix, kExprStructNewDesc, $struct,
    ]);

builder.addFunction('make2', makeSig([kWasmI32], [kWasmAnyRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $g_desc2.index,
      kGCPrefix, kExprStructNewDesc, $struct,
    ]);

const {make1, make2} = builder.instantiate({m: {proto: {}}}).exports;
const obj1 = make1(1);
const obj2 = make1(2);
const otherDescObj = make2(3);

// 1. Transitioning / pre-monomorphic store checking FieldType::Class on a
// WasmCustomMap (AccessorAssembler::CheckFieldType).
(function TestTransitioningStoreFieldType() {
  function makeWithField(v) {
    const o = {};
    o.field = v;
    return o;
  }
  assertSame(obj1, makeWithField(obj1).field);
  assertSame(obj2, makeWithField(obj2).field);
  assertSame(otherDescObj, makeWithField(otherDescObj).field);
})();

// 2. Monomorphic in-place mutable HeapObject field store checking
// FieldType::Class on a WasmCustomMap
// (AccessorAssembler::CheckHeapObjectTypeMatchesDescriptor).
(function TestInPlaceMutableFieldStore() {
  const holder = {};
  Object.defineProperty(holder, 'field', {
    value: obj1,
    writable: true,
    enumerable: true,
    configurable: true,
  });
  Object.defineProperty(holder, 'field', {value: obj2});

  function storeField(o, v) {
    o.field = v;
  }
  for (let i = 0; i < 20; i++) {
    const expected = (i & 1) === 0 ? obj1 : obj2;
    storeField(holder, expected);
    assertSame(expected, holder.field);
  }
  // Generalize FieldType from Class(WasmCustomMap) to Any.
  storeField(holder, otherDescObj);
  assertSame(otherDescObj, holder.field);
})();
