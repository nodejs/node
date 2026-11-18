// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const otherRealm = Realm.createAllowCrossRealmAccess();
const otherGlobal = Realm.global(otherRealm);

function strict() {
  'use strict';
  return this;
}
function sloppy() {
  return this;
}
const otherSloppy = Realm.eval(otherRealm, '(function() { return this; })');

(function TestSuspendingImports() {
  const suspending = fn => new WebAssembly.Suspending(fn);
  const imports = {
    strict: suspending(strict),
    sloppy: suspending(sloppy),
    otherSloppy: suspending(otherSloppy),
  };
  const builder = new WasmModuleBuilder();
  const names = Object.keys(imports);
  const indices = names.map(name => builder.addImport('m', name, kSig_r_v));
  names.forEach(
      (name, i) => builder.addFunction(name, kSig_r_v)
                       .addBody([kExprCallFunction, indices[i]])
                       .exportFunc());
  const instance = new WebAssembly.Instance(builder.toModule(), {m: imports});

  // Suspending imports bypass the generic wrapper and compile a Turboshaft
  // wrapper eagerly with suspend == kSuspend.
  const expected = {
    strict: undefined,
    sloppy: globalThis,
    otherSloppy: otherGlobal,
  };
  for (const [name, receiver] of Object.entries(expected)) {
    assertPromiseResult(
        WebAssembly.promising(instance.exports[name])(),
        value => assertSame(receiver, value));
  }
})();
