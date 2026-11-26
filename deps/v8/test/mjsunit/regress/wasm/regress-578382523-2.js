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

(function TestCrossRealmReexportedJSFunction() {
  const foreignRealm = Realm.createAllowCrossRealmAccess();
  const foreignGlobal = Realm.global(foreignRealm);

  const builderA = new WasmModuleBuilder();
  const {sigSub: sigSubA} = addSigPair(builderA);
  const impExtA = builderA.addImport('m', 'ext', sigSubA);
  const impI64A = builderA.addImport('m', 'i64', kSig_l_v);
  builderA.addExport('ext', impExtA);
  builderA.addExport('i64', impI64A);
  const modA = new foreignGlobal.WebAssembly.Module(builderA.toBuffer());
  const instA = new foreignGlobal.WebAssembly.Instance(
      modA, {m: {ext: (_a) => null, i64: () => 1}});

  const builderB = new WasmModuleBuilder();
  const {sigSuper: sigSuperB} = addSigPair(builderB);
  const sigI64B = builderB.addType(kSig_l_v);
  const impExtB = builderB.addImport('m', 'ext', sigSuperB);
  const impI64B = builderB.addImport('m', 'i64', sigI64B);
  builderB.addDeclarativeElementSegment([impExtB, impI64B]);
  builderB.addFunction('call_ext', sigSuperB)
      .addBody([kExprCallFunction, impExtB])
      .exportFunc();
  builderB.addFunction('call_ref_ext', sigSuperB)
      .addBody([kExprRefFunc, impExtB, kExprCallRef, sigSuperB])
      .exportFunc();
  builderB.addFunction('call_i64', sigI64B)
      .addBody([kExprCallFunction, impI64B])
      .exportFunc();
  builderB.addFunction('call_ref_i64', sigI64B)
      .addBody([kExprRefFunc, impI64B, kExprCallRef, sigI64B])
      .exportFunc();
  const instB = builderB.instantiate(
      {m: {ext: instA.exports.ext, i64: instA.exports.i64}});

  for (let i = 0; i < 4; ++i) {
    assertThrows(() => instB.exports.call_ext(), foreignGlobal.TypeError);
    assertThrows(() => instB.exports.call_ref_ext(), foreignGlobal.TypeError);
    assertThrows(() => instA.exports.ext(), foreignGlobal.TypeError);
    assertThrows(() => instB.exports.call_i64(), foreignGlobal.TypeError);
    assertThrows(() => instB.exports.call_ref_i64(), foreignGlobal.TypeError);
    assertThrows(() => instA.exports.i64(), foreignGlobal.TypeError);
  }
})();

(function TestSuspendingReexportedJSFunctionSupertype() {
  if (typeof WebAssembly.promising !== 'function') return;

  const builderA = new WasmModuleBuilder();
  const {sigSub: sigSubA} = addSigPair(builderA);
  const impA = builderA.addImport('m', 'f', sigSubA);
  builderA.addExport('f', impA);

  const builderB = new WasmModuleBuilder();
  const {sigSuper: sigSuperB} = addSigPair(builderB);
  const impB = builderB.addImport('m', 'f', sigSuperB);
  builderB.addFunction('run', sigSuperB)
      .addBody([kExprCallFunction, impB])
      .exportFunc();

  const instANull = builderA.instantiate(
      {m: {f: new WebAssembly.Suspending(() => Promise.resolve(null))}});
  const instBNull = builderB.instantiate({m: {f: instANull.exports.f}});
  assertThrowsAsync(
      WebAssembly.promising(instBNull.exports.run)(), TypeError);

  const okObj = {ok: true};
  const instAOk = builderA.instantiate(
      {m: {f: new WebAssembly.Suspending(() => Promise.resolve(okObj))}});
  const instBOk = builderB.instantiate({m: {f: instAOk.exports.f}});
  assertPromiseResult(
      WebAssembly.promising(instBOk.exports.run)(),
      v => assertSame(okObj, v));
})();
