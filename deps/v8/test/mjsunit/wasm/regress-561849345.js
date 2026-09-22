// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wrapper-tiering-budget=1

// When a Wasm instance created in one realm (importer realm) calls an imported
// sloppy-mode JS function defined in another realm (callee realm), the `this`
// receiver must be bound to the callee's global proxy
// (`callee_context->native_context()->global_proxy()`), not the importing
// Wasm instance's `WasmImportData::native_context` global proxy, both before
// and after Turboshaft Wasm-to-JS wrapper tier-up.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const imp = builder.addImport('m', 'fn', kSig_r_v);
builder.addFunction('call_import', kSig_r_v)
    .addBody([kExprCallFunction, imp])
    .exportFunc();
const module = builder.toModule();

function instantiateInForeignRealm(jsFunc) {
  const foreignRealm = Realm.createAllowCrossRealmAccess();
  const foreignGlobal = Realm.global(foreignRealm);
  const foreignInstance =
      new foreignGlobal.WebAssembly.Instance(module, {m: {fn: jsFunc}});
  return {instance: foreignInstance, foreignGlobal};
}

// Test 1: Top-level sloppy JS function from main realm imported into a Wasm
// instance instantiated via a cross-realm WebAssembly.Instance constructor.
// Verify `this === globalThis` and `this !== foreignGlobal` across multiple calls
// (spanning generic wrapper and tiered-up Turboshaft wrapper).
(function testTopLevelSloppyFunction() {
  function sloppyTarget() {
    return this;
  }

  const {instance, foreignGlobal} = instantiateInForeignRealm(sloppyTarget);
  for (let i = 0; i < 5; ++i) {
    const receiver = instance.exports.call_import();
    assertSame(globalThis, receiver);
    assertNotSame(foreignGlobal, receiver);
  }
})();

// Test 2: Sloppy closure JS function (capturing a local variable so
// JSFunction::context is a FunctionContext rather than directly a NativeContext)
// imported into the foreign realm's Wasm instance. Verify `this === globalThis`
// and that captured variable access remains intact across tier-up.
(function testSloppyClosure() {
  let counter = 0;
  function sloppyClosure() {
    counter++;
    return this;
  }

  const {instance, foreignGlobal} = instantiateInForeignRealm(sloppyClosure);
  for (let i = 0; i < 5; ++i) {
    const receiver = instance.exports.call_import();
    assertSame(globalThis, receiver);
    assertNotSame(foreignGlobal, receiver);
    assertEquals(i + 1, counter);
  }
})();

// Test 3: Strict JS function imported across realms (`this === undefined`
// across tier-up).
(function testStrictFunction() {
  function strictTarget() {
    'use strict';
    return this;
  }

  const {instance, foreignGlobal} = instantiateInForeignRealm(strictTarget);
  for (let i = 0; i < 5; ++i) {
    const receiver = instance.exports.call_import();
    assertSame(undefined, receiver);
  }
})();

// Test 4: Reverse realm direction with arity mismatch.
// Wasm instance in the main realm importing a sloppy JS function with parameters
// (a, b) defined in foreignRealm. Verify `this === foreignGlobal` and
// `this !== globalThis` across tier-up.
(function testReverseRealmWithArityMismatch() {
  const foreignRealm = Realm.createAllowCrossRealmAccess();
  const foreignGlobal = Realm.global(foreignRealm);
  const foreignSloppy = Realm.eval(
      foreignRealm,
      "(function foreignSloppy(a, b) { return this; })");

  const instance = new WebAssembly.Instance(module, {m: {fn: foreignSloppy}});
  for (let i = 0; i < 5; ++i) {
    const receiver = instance.exports.call_import();
    assertSame(foreignGlobal, receiver);
    assertNotSame(globalThis, receiver);
  }
})();
