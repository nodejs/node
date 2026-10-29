// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");

(function() {
if (typeof WebAssembly === 'undefined') return;

Error.stackTraceLimit = 20;

let capturedSites = null;
Error.prepareStackTrace = (_error, callSites) => {
  capturedSites = callSites;
  return callSites;
};

const realm1 = Realm.create();

let builder2 = new WasmModuleBuilder();
builder2.addImport("env", "trigger", kSig_v_v);
builder2.addFunction("main", kSig_v_v).addBody([
  kExprCallFunction, 0
]).exportFunc();
const wasmBytes2 = builder2.toArray();

const captureInRealm1 = Realm.eval(realm1, `
  (function capture(wasmBytes, target) {
    let importObj = {
      env: {
        trigger: function() {
          Error.captureStackTrace(target);
        }
      }
    };

    let instance = new WebAssembly.Instance(
      new WebAssembly.Module(new Uint8Array(wasmBytes)),
      importObj
    );

    instance.exports.main();
  })
`);

const target = {};
captureInRealm1(wasmBytes2, target);
void target.stack;

assertTrue(Array.isArray(capturedSites));
assertTrue(capturedSites.length > 0);

// The first three frames should be:
// 1. trigger (from realm1)
// 2. main (Wasm frame, instantiated in realm1)
// 3. capture (from realm1)
// The remaining frames are from the main realm (e.g. the top-level script).
for (let i = 0; i < 3; i++) {
  const site = capturedSites[i];
  assertEquals("", site.toString());
  assertEquals(undefined, site.getFunction());
  assertEquals(undefined, site.getThis());
  assertEquals(null, site.getFunctionName());
  assertEquals(null, site.getMethodName());
}
})();
