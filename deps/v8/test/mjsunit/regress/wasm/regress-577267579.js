// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-merged-descriptors
// Flags: --expose-gc --verify-heap
// Avoid slow per-field type assertions on >8000-field struct.new_default.
// Flags: --no-wasm-assert-types

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Exceed kMaxRegularHeapObjectSize (131072 bytes on 256KB-page builds):
// 8200 * 16 > 131072.
const kNumS128Fields = 8200;
const kLastField = kNumS128Fields;  // 1 externref field + 8200 s128 fields.

const builder = new WasmModuleBuilder();
builder.startRecGroup();
const $desc = builder.nextTypeIndex() + 1;
const $s = builder.addStruct({
  fields: [makeField(kWasmI32, true)],
  descriptor: $desc,
});
const desc_fields = [makeField(kWasmExternRef, true)];
for (let i = 0; i < kNumS128Fields; i++) {
  desc_fields.push(makeField(kWasmS128, true));
}
/* $desc */ builder.addStruct({fields: desc_fields, describes: $s});
builder.endRecGroup();

const $g_desc = builder.addGlobal(
    wasmRefType($desc).exact(), false, false,
    [kGCPrefix, kExprStructNewDefault, $desc]);
const $g_s = builder.addGlobal(wasmRefType($s), false, false, [
  kExprGlobalGet, $g_desc.index,
  kGCPrefix, kExprStructNewDefaultDesc, $s,
]);
const $dyn_desc = builder.addGlobal(
    wasmRefNullType($desc).exact(), true, false,
    [kExprRefNull, kNullRefCode]);

builder.addFunction('get_global_s', makeSig([], [wasmRefType($s)]))
    .exportFunc()
    .addBody([kExprGlobalGet, $g_s.index]);

builder
    .addFunction('make_desc', makeSig([], [wasmRefType($desc).exact()]))
    .exportFunc()
    .addBody([
      kGCPrefix, kExprStructNewDefault, $desc,
      kExprGlobalSet, $dyn_desc.index,
      kExprGlobalGet, $dyn_desc.index,
      kExprRefAsNonNull,
    ]);

builder
    .addFunction('init_desc', makeSig([kWasmExternRef, kWasmI32], []))
    .exportFunc()
    .addBody([
      kExprGlobalGet, $dyn_desc.index,
      kExprLocalGet, 0,
      kGCPrefix, kExprStructSet, $desc, 0,
      kExprGlobalGet, $dyn_desc.index,
      kExprLocalGet, 1,
      kSimdPrefix, kExprI32x4Splat,
      kGCPrefix, kExprStructSet, $desc, ...wasmUnsignedLeb(kLastField),
    ]);

builder
    .addFunction('make_s', makeSig([kWasmI32], [wasmRefType($s)]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $dyn_desc.index,
      kExprRefAsNonNull,
      kGCPrefix, kExprStructNewDesc, $s,
    ]);

builder
    .addFunction('check', makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalGet, $dyn_desc.index,
      kExprRefAsNonNull,
      kGCPrefix, kExprRefCastDescEq, $s,
      kGCPrefix, kExprStructGet, $s, 0,
      kExprGlobalGet, $dyn_desc.index,
      kGCPrefix, kExprStructGet, $desc, ...wasmUnsignedLeb(kLastField),
      kSimdPrefix, kExprI32x4ExtractLane, 0,
      kExprI32Add,
    ]);

builder
    .addFunction('get_desc_ref', makeSig([], [kWasmExternRef]))
    .exportFunc()
    .addBody([
      kExprGlobalGet, $dyn_desc.index,
      kGCPrefix, kExprStructGet, $desc, 0,
    ]);

const instance = builder.instantiate();
const {get_global_s, make_desc, init_desc, make_s, check, get_desc_ref} =
    instance.exports;

const global_s = get_global_s();
const marker = {tag: 'alive'};
const desc = make_desc();
init_desc(marker, 100);
const s = make_s(42);

gc({type: 'minor'});
gc();

assertEquals('object', typeof desc);
assertSame(marker, get_desc_ref());
assertEquals(142, check(s));
assertNotNull(global_s);
