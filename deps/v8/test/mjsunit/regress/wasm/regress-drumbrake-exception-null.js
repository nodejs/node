// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// A null reference thrown as an exception payload must be caught as the same
// kind of null (WasmNull for internal types, JS null for extern types), so
// that null checks keep working after the catch.

(function TestNullPayloadRoundTrip() {
  const builder = new WasmModuleBuilder();
  const struct = builder.addStruct([makeField(kWasmI32, true)]);
  const cases = [
    ['anyref', kWasmAnyRef, [kAnyRefCode], false],
    ['eqref', kWasmEqRef, [kEqRefCode], true],
    ['structref', wasmRefNullType(struct), wasmSignedLeb(struct), true],
    ['nullref', kWasmNullRef, [kNullRefCode], true],
    ['funcref', kWasmFuncRef, [kFuncRefCode], false],
    ['externref', kWasmExternRef, [kExternRefCode], false],
  ];
  for (const [name, type, heap_type, eq] of cases) {
    const tag = builder.addTag(makeSig([type], []));
    const block_type = builder.addType(makeSig([], [type]));
    const throw_and_catch_null = [
      kExprTry, ...wasmSignedLeb(block_type),
        kExprRefNull, ...heap_type,
        kExprThrow, ...wasmUnsignedLeb(tag),
      kExprCatch, ...wasmUnsignedLeb(tag),
      kExprEnd,
    ];
    builder.addFunction(name + '_is_null', kSig_i_v)
        .addBody([...throw_and_catch_null, kExprRefIsNull])
        .exportFunc();
    builder.addFunction(name + '_br_on_null', kSig_i_v)
        .addBody([
          kExprBlock, kWasmVoid,
            ...throw_and_catch_null,
            kExprBrOnNull, 0,
            kExprDrop,
            kExprI32Const, 0,
            kExprReturn,
          kExprEnd,
          kExprI32Const, 1,
        ])
        .exportFunc();
    builder.addFunction(name + '_test_null', kSig_i_v)
        .addBody([
          ...throw_and_catch_null,
          kGCPrefix, kExprRefTestNull, ...heap_type,
        ])
        .exportFunc();
    if (eq) {
      builder.addFunction(name + '_eq_null', kSig_i_v)
          .addBody([
            ...throw_and_catch_null,
            kExprRefNull, ...heap_type,
            kExprRefEq,
          ])
          .exportFunc();
    }
    builder.addFunction(name + '_get', makeSig([], [type]))
        .addBody(throw_and_catch_null)
        .exportFunc();
  }

  const instance = builder.instantiate();
  for (const [name, , , eq] of cases) {
    assertEquals(1, instance.exports[name + '_is_null'](), name);
    assertEquals(1, instance.exports[name + '_br_on_null'](), name);
    assertEquals(1, instance.exports[name + '_test_null'](), name);
    if (eq) assertEquals(1, instance.exports[name + '_eq_null'](), name);
    assertNull(instance.exports[name + '_get'](), name);
  }
})();

(function TestNullPayloadCastToSubtype() {
  // After a caught null payload incorrectly passes br_on_null, a cast to a
  // non-final subtype would inspect the map of the JS null object as if it was
  // a Wasm struct.
  const builder = new WasmModuleBuilder();
  const super_type = builder.addStruct(
      [makeField(kWasmI32, true)], kNoSuperType, false);
  const sub_type = builder.addStruct(
      [makeField(kWasmI32, true), makeField(kWasmI64, true)], super_type,
      false);
  const tag = builder.addTag(makeSig([wasmRefNullType(super_type)], []));
  const block_type =
      builder.addType(makeSig([], [wasmRefNullType(super_type)]));
  builder.addFunction('main', kSig_i_v)
      .addBody([
        kExprBlock, kWasmVoid,
          kExprTry, ...wasmSignedLeb(block_type),
            kExprRefNull, ...wasmSignedLeb(super_type),
            kExprThrow, tag,
          kExprCatch, tag,
          kExprEnd,
          kExprBrOnNull, 0,
          kGCPrefix, kExprRefCast, ...wasmSignedLeb(sub_type),
          kGCPrefix, kExprStructGet, ...wasmUnsignedLeb(sub_type), 0,
          kExprReturn,
        kExprEnd,
        kExprI32Const, 42,
      ])
      .exportFunc();
  const instance = builder.instantiate();
  assertEquals(42, instance.exports.main());
})();

(function TestNullPayloadFromJS() {
  // Null payloads of exceptions created from JS must be handled the same way.
  const builder = new WasmModuleBuilder();
  const tag_index = builder.addImportedTag('m', 'tag', makeSig([kWasmAnyRef], []));
  const throw_index = builder.addImport('m', 'throw', kSig_v_v);
  const block_type = builder.addType(makeSig([], [kWasmAnyRef]));
  builder.addFunction('main', kSig_i_v)
      .addBody([
        kExprTry, ...wasmSignedLeb(block_type),
          kExprCallFunction, throw_index,
          kExprUnreachable,
        kExprCatch, tag_index,
        kExprEnd,
        kExprRefIsNull,
      ])
      .exportFunc();
  const tag = new WebAssembly.Tag({parameters: ['anyref']});
  const instance = builder.instantiate({
    m: {tag, throw() { throw new WebAssembly.Exception(tag, [null]); }}
  });
  assertEquals(1, instance.exports.main());
})();
