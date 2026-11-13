// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wrapper-tiering-budget=1

// Wasm-to-JS wrappers pass `undefined` as receiver to strict and native
// functions and the callee realm's global proxy to sloppy functions. Compiled
// wrappers are specialized on that choice (ImportCallKind::kJSFunction*);
// verify every variant across generic execution and wrapper tier-up.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kSig_r_i = makeSig([kWasmI32], [kWasmExternRef]);
const otherRealm = Realm.createAllowCrossRealmAccess();
const otherGlobal = Realm.global(otherRealm);

function strict() {
  'use strict';
  return this;
}
function sloppy() {
  return this;
}
function sloppyTwoParams(a, b) {
  assertSame(undefined, a);
  assertSame(undefined, b);
  return this;
}
const otherSloppy = Realm.eval(otherRealm, '(function() { return this; })');

// Imports each function in {imports} as [] -> [externref] and exports a
// caller under the same name. {wasm} selects the instantiating realm.
function instantiate(imports, wasm = WebAssembly) {
  const builder = new WasmModuleBuilder();
  const names = Object.keys(imports);
  const indices = names.map(name => builder.addImport('m', name, kSig_r_v));
  names.forEach(
      (name, i) => builder.addFunction(name, kSig_r_v)
                       .addBody([kExprCallFunction, indices[i]])
                       .exportFunc());
  return new wasm.Instance(builder.toModule(), {m: imports});
}

// The first call runs the generic wrapper and triggers tier-up; later calls
// run the compiled Turboshaft wrapper.
function assertReceivers(instance, expected) {
  for (let i = 0; i < 4; ++i) {
    for (const [name, receiver] of Object.entries(expected)) {
      assertSame(receiver, instance.exports[name]());
    }
  }
}

(function TestSameSignatureAndArityAcrossCallKinds() {
  // Only the receiver kind separates these three cache keys. Tier them up one
  // at a time via direct call, call_indirect, and call_ref, checking that all
  // three coexist without cache collision.
  const builder = new WasmModuleBuilder();
  const sigIdx = builder.addType(kSig_r_v);
  const iStrict = builder.addImport('m', 'strict', sigIdx);
  const iSloppy = builder.addImport('m', 'sloppy', sigIdx);
  const iOtherSloppy = builder.addImport('m', 'otherSloppy', sigIdx);
  builder.appendToTable([iStrict, iSloppy, iOtherSloppy]);

  builder.addFunction('strict', kSig_r_v)
      .addBody([kExprCallFunction, iStrict])
      .exportFunc();
  builder.addFunction('sloppy', kSig_r_v)
      .addBody([kExprCallFunction, iSloppy])
      .exportFunc();
  builder.addFunction('otherSloppy', kSig_r_v)
      .addBody([kExprCallFunction, iOtherSloppy])
      .exportFunc();
  builder.addFunction('callTable', kSig_r_i)
      .addBody([kExprLocalGet, 0, kExprCallIndirect, sigIdx, kTableZero])
      .exportFunc();
  builder.addFunction('refStrict', kSig_r_v)
      .addBody([kExprRefFunc, iStrict, kExprCallRef, sigIdx])
      .exportFunc();
  builder.addFunction('refSloppy', kSig_r_v)
      .addBody([kExprRefFunc, iSloppy, kExprCallRef, sigIdx])
      .exportFunc();
  builder.addFunction('refOtherSloppy', kSig_r_v)
      .addBody([kExprRefFunc, iOtherSloppy, kExprCallRef, sigIdx])
      .exportFunc();

  const {exports} = builder.instantiate({m: {strict, sloppy, otherSloppy}});

  for (let i = 0; i < 4; ++i) assertSame(undefined, exports.strict());
  for (let i = 0; i < 4; ++i) assertSame(globalThis, exports.callTable(1));
  for (let i = 0; i < 4; ++i) assertSame(otherGlobal, exports.refOtherSloppy());

  assertSame(undefined, exports.strict());
  assertSame(globalThis, exports.sloppy());
  assertSame(otherGlobal, exports.otherSloppy());

  assertSame(undefined, exports.callTable(0));
  assertSame(globalThis, exports.callTable(1));
  assertSame(otherGlobal, exports.callTable(2));

  assertSame(undefined, exports.refStrict());
  assertSame(globalThis, exports.refSloppy());
  assertSame(otherGlobal, exports.refOtherSloppy());
})();

(function TestInstanceInOtherRealm() {
  // Instantiated in {otherRealm}, {sloppy} is cross-realm and {otherSloppy} is
  // same-realm.
  assertReceivers(
      instantiate({strict, sloppy, otherSloppy}, otherGlobal.WebAssembly),
      {strict: undefined, sloppy: globalThis, otherSloppy: otherGlobal});
})();

(function TestArityMismatch() {
  assertReceivers(
      instantiate({sloppyTwoParams}), {sloppyTwoParams: globalThis});
})();

(function TestNativeSloppyFunctionGetsUndefined() {
  // Array.prototype.slice is sloppy and native: slice.call(globalThis) returns
  // [], whereas slice.call(undefined) throws a TypeError.
  const instance = instantiate({slice: Array.prototype.slice});
  for (let i = 0; i < 4; ++i) {
    assertThrows(() => instance.exports.slice(), TypeError);
  }
})();

(function TestReexportKeepsExportersReceiver() {
  // A re-exported import keeps the exporter's WasmImportData and wrapper, both
  // before and after the exporter's wrapper tiers up.
  const exporterBuilder = new WasmModuleBuilder();
  exporterBuilder.addExport(
      'fn', exporterBuilder.addImport('m', 'fn', kSig_r_v));
  const exporterMod = exporterBuilder.toModule();

  const exporterMain = new WebAssembly.Instance(exporterMod, {m: {fn: sloppy}});
  assertReceivers(
      instantiate({fn: exporterMain.exports.fn}, otherGlobal.WebAssembly),
      {fn: globalThis});

  const thirdGlobal = Realm.global(Realm.createAllowCrossRealmAccess());
  const exporterForOther =
      new WebAssembly.Instance(exporterMod, {m: {fn: otherSloppy}});
  assertReceivers(
      instantiate({fn: exporterForOther.exports.fn}, thirdGlobal.WebAssembly),
      {fn: otherGlobal});
  // Re-importing again after tier-up reuses the compiled wrapper.
  assertReceivers(
      instantiate({fn: exporterForOther.exports.fn}, thirdGlobal.WebAssembly),
      {fn: otherGlobal});
})();

(function TestCompileTimeImportWrapperTierUp() {
  const kSig_i_r = makeSig([kWasmExternRef], [kWasmI32]);
  const builder = new WasmModuleBuilder();
  const sigIdx = builder.addType(kSig_i_r);
  const strLen = builder.addImport('wasm:js-string', 'length', sigIdx);
  builder.appendToTable([strLen]);
  builder.addFunction('callLen', kSig_i_r)
      .addBody([
        kExprLocalGet, 0, kExprI32Const, 0, kExprCallIndirect, sigIdx,
        kTableZero
      ])
      .exportFunc();

  const mod =
      new WebAssembly.Module(builder.toBuffer(), {builtins: ['js-string']});
  const instance = new WebAssembly.Instance(mod);
  for (let i = 0; i < 4; ++i) {
    assertEquals(5, instance.exports.callLen('hello'));
  }
})();
