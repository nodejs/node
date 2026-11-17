// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// A JS import returning null for a nullable reference type must be converted
// to the right Wasm null representation, without passing WasmNull to the
// JS-to-Wasm conversion.

(function TestFuzzerRepro() {
  // Minimized from the fuzzer case (scenario "import_ref_null").
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport('m', 'callback', makeSig([], [kWasmFuncRef]));
  builder.addFunction('run', kSig_i_v)
      .addBody([kExprCallFunction, imp, kExprDrop, ...wasmI32Const(7)])
      .exportFunc();
  const instance =
      builder.instantiate({m: {callback() { gc(); return null; }}});
  assertEquals(7, instance.exports.run());
  assertEquals(7, instance.exports.run());
})();

(function TestImportReturnsNull() {
  const builder = new WasmModuleBuilder();
  const struct = builder.addStruct([makeField(kWasmI32, true)]);
  const cases = [
    ['funcref', kWasmFuncRef],
    ['anyref', kWasmAnyRef],
    ['eqref', kWasmEqRef],
    ['structref', wasmRefNullType(struct)],
    ['nullref', kWasmNullRef],
    ['externref', kWasmExternRef],
  ];
  const imports = {};
  const import_indices = cases.map(([name, type]) => {
    imports[name] = () => { gc(); return null; };
    return builder.addImport('m', name, makeSig([], [type]));
  });
  cases.forEach(([name, type], i) => {
    const sig = makeSig([], [type]);
    const imp = import_indices[i];
    builder.addFunction(name + '_is_null', kSig_i_v)
        .addBody([kExprCallFunction, imp, kExprRefIsNull])
        .exportFunc();
    builder.addFunction(name + '_br_on_null', kSig_i_v)
        .addBody([
          kExprBlock, kWasmVoid,
            kExprCallFunction, imp,
            kExprBrOnNull, 0,
            kExprDrop,
            kExprI32Const, 0,
            kExprReturn,
          kExprEnd,
          kExprI32Const, 1,
        ])
        .exportFunc();
    builder.addFunction(name + '_get', sig)
        .addBody([kExprCallFunction, imp])
        .exportFunc();
  });
  const instance = builder.instantiate({m: imports});
  for (const [name] of cases) {
    assertEquals(1, instance.exports[name + '_is_null'](), name);
    assertEquals(1, instance.exports[name + '_br_on_null'](), name);
    assertNull(instance.exports[name + '_get'](), name);
  }
})();

(function TestImportReturnsMultipleNulls() {
  const builder = new WasmModuleBuilder();
  const sig = makeSig([], [kWasmFuncRef, kWasmI32, kWasmExternRef,
                           kWasmFuncRef]);
  const imp = builder.addImport('m', 'f', sig);
  builder.addFunction('main', sig)
      .addBody([kExprCallFunction, imp])
      .exportFunc();
  const instance = builder.instantiate(
      {m: {f() { gc(); return [null, 7, null, null]; }}});
  assertEquals([null, 7, null, null], instance.exports.main());
})();

(function TestIndirectCallToImportReturnsNull() {
  // Indirect calls coerce the result against the callee's canonical
  // signature.
  const builder = new WasmModuleBuilder();
  const sig = builder.addType(makeSig([], [kWasmFuncRef]));
  const imp = builder.addImport('m', 'f', sig);
  const table = builder.addTable(kWasmFuncRef, 1, 1);
  builder.addActiveElementSegment(table.index, wasmI32Const(0), [imp]);
  builder.addFunction('call_indirect', kSig_i_v)
      .addBody([
        kExprI32Const, 0,
        kExprCallIndirect, sig, table.index,
        kExprRefIsNull,
      ])
      .exportFunc();
  builder.addDeclarativeElementSegment([imp]);
  builder.addFunction('call_ref', kSig_i_v)
      .addBody([
        kExprRefFunc, imp,
        kExprCallRef, sig,
        kExprRefIsNull,
      ])
      .exportFunc();
  const instance = builder.instantiate({m: {f() { gc(); return null; }}});
  assertEquals(1, instance.exports.call_indirect());
  assertEquals(1, instance.exports.call_ref());
})();

(function TestImportReturnsNullForNonNullableType() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([], [wasmRefType(kWasmFuncRef)]));
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprCallFunction, imp, kExprDrop])
      .exportFunc();
  const instance = builder.instantiate({m: {f() { return null; }}});
  assertThrows(() => instance.exports.main(), TypeError);
})();
