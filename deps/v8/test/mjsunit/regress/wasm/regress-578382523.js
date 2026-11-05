// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wrapper-tiering-budget=2

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kSigSub = makeSig([], [wasmRefType(kWasmExternRef)]);
const kSigSuper = makeSig([], [kWasmExternRef]);

function addSigPair(builder) {
  const sigSuper = builder.addType(kSigSuper, kNoSuperType, false);
  const sigSub = builder.addType(kSigSub, sigSuper, false);
  return {sigSuper, sigSub};
}

(function TestSupertypeImportOfReexportedJSFunction() {
  let ret = null;

  const builderA = new WasmModuleBuilder();
  const {sigSub: sigSubA} = addSigPair(builderA);
  const impA = builderA.addImport('m', 'f', sigSubA);
  builderA.addExport('f', impA);
  const instA = builderA.instantiate({m: {f: () => ret}});

  const builderB = new WasmModuleBuilder();
  const {sigSuper: sigSuperB} = addSigPair(builderB);
  const impB = builderB.addImport('m', 'f', sigSuperB);
  builderB.addDeclarativeElementSegment([impB]);
  builderB.addExport('reexport', impB);
  builderB.addFunction('call_direct', sigSuperB)
      .addBody([kExprCallFunction, impB])
      .exportFunc();
  builderB.addFunction('return_call_direct', sigSuperB)
      .addBody([kExprReturnCall, impB])
      .exportFunc();
  builderB.addFunction('call_via_ref', sigSuperB)
      .addBody([kExprRefFunc, impB, kExprCallRef, sigSuperB])
      .exportFunc();
  const instB = builderB.instantiate({m: {f: instA.exports.f}});

  const builderC = new WasmModuleBuilder();
  const {sigSuper: sigSuperC} = addSigPair(builderC);
  const impC = builderC.addImport('m', 'f', sigSuperC);
  builderC.addFunction('call_direct', sigSuperC)
      .addBody([kExprCallFunction, impC])
      .exportFunc();
  const instC = builderC.instantiate({m: {f: instB.exports.reexport}});

  // Call instB before instA so instB executes the generic WasmToJSWrapper on
  // both throwing and non-throwing paths and triggers wrapper tier-up itself.
  for (let i = 0; i < 4; ++i) {
    ret = null;
    assertThrows(() => instB.exports.call_direct(), TypeError);
    const obj = {i};
    ret = obj;
    assertSame(obj, instB.exports.call_direct());

    ret = null;
    assertThrows(() => instB.exports.return_call_direct(), TypeError);
    assertThrows(() => instB.exports.call_via_ref(), TypeError);
    assertThrows(() => instC.exports.call_direct(), TypeError);
    assertThrows(() => instA.exports.f(), TypeError);

    ret = obj;
    assertSame(obj, instB.exports.return_call_direct());
    assertSame(obj, instB.exports.call_via_ref());
    assertSame(obj, instC.exports.call_direct());
  }
})();

(function TestTableDispatchWithSupertypeImports() {
  const wasmObj = {from: 'wasm'};
  let jsRet = null;

  const builderA = new WasmModuleBuilder();
  const {sigSub: sigSubA} = addSigPair(builderA);
  const impJsA = builderA.addImport('m', 'js_fn', sigSubA);
  const globalObj = builderA.addImportedGlobal(
      'm', 'obj', wasmRefType(kWasmExternRef), false);
  builderA.addExport('js_reexport', impJsA);
  builderA.addFunction('wasm_def', sigSubA)
      .addBody([kExprGlobalGet, globalObj])
      .exportFunc();
  // Use a distinct JS formal parameter count so this import does not hit the
  // already-tiered-up wrapper cache entry from the previous subtest.
  const instA =
      builderA.instantiate({m: {js_fn: (_a, _b) => jsRet, obj: wasmObj}});

  const builderB = new WasmModuleBuilder();
  const {sigSuper: sigSuperB, sigSub: sigSubB} = addSigPair(builderB);

  const impJs = builderB.addImport('m', 'js_reexport', sigSuperB);
  const impWasm = builderB.addImport('m', 'wasm_def', sigSuperB);

  // 1. Active element segment initialization.
  const activeTable = builderB.addTable(kWasmFuncRef, 2, 2);
  builderB.addActiveElementSegment(
      activeTable.index, wasmI32Const(0), [impJs, impWasm]);

  // 2. Table initial value initialization (ConstantExpression::Kind::kRefFunc).
  const initJsTable = builderB.addTable(
      wasmRefNullType(sigSuperB), 1, 1, [kExprRefFunc, impJs]);
  const initWasmTable = builderB.addTable(
      wasmRefNullType(sigSuperB), 1, 1, [kExprRefFunc, impWasm]);

  // 3. Passive element segment + table.init.
  const passiveTable = builderB.addTable(kWasmFuncRef, 2, 2);
  const passiveSeg = builderB.addPassiveElementSegment([impJs, impWasm]);

  builderB.addFunction('init_passive', kSig_v_v)
      .addBody([
        kExprI32Const, 0,
        kExprI32Const, 0,
        kExprI32Const, 2,
        kNumericPrefix, kExprTableInit, passiveSeg, passiveTable.index,
      ])
      .exportFunc();

  function addTableAccessors(name, tableIndex) {
    builderB.addFunction(`test_sub_${name}`, kSig_i_i)
        .addBody([
          kExprLocalGet, 0,
          kExprTableGet, tableIndex,
          kGCPrefix, kExprRefTest, sigSubB,
        ])
        .exportFunc();
    builderB
        .addFunction(
            `call_sub_${name}`, makeSig([kWasmI32], [kWasmExternRef]))
        .addBody([
          kExprLocalGet, 0,
          kExprCallIndirect, sigSubB, tableIndex,
        ])
        .exportFunc();
  }

  addTableAccessors('active', activeTable.index);
  addTableAccessors('init_js', initJsTable.index);
  addTableAccessors('init_wasm', initWasmTable.index);
  addTableAccessors('passive', passiveTable.index);

  const instB = builderB.instantiate({
    m: {
      js_reexport: instA.exports.js_reexport,
      wasm_def: instA.exports.wasm_def,
    },
  });
  instB.exports.init_passive();

  for (const tableKind of ['active', 'passive']) {
    assertEquals(1, instB.exports[`test_sub_${tableKind}`](0));
    assertEquals(1, instB.exports[`test_sub_${tableKind}`](1));

    jsRet = null;
    assertThrows(() => instB.exports[`call_sub_${tableKind}`](0), TypeError);
    const obj = {tableKind};
    jsRet = obj;
    assertSame(obj, instB.exports[`call_sub_${tableKind}`](0));
    assertSame(wasmObj, instB.exports[`call_sub_${tableKind}`](1));
  }

  assertEquals(1, instB.exports.test_sub_init_js(0));
  jsRet = null;
  assertThrows(() => instB.exports.call_sub_init_js(0), TypeError);
  const initObj = {init: true};
  jsRet = initObj;
  assertSame(initObj, instB.exports.call_sub_init_js(0));

  assertEquals(1, instB.exports.test_sub_init_wasm(0));
  assertSame(wasmObj, instB.exports.call_sub_init_wasm(0));
})();
