// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --allow-natives-syntax

// Note that this test does not pass --experimental-wasm-reftypes on purpose so
// that we make sure the two flags can be controlled separately/independently.

load("test/mjsunit/wasm/wasm-module-builder.js");

// First we just test that "exnref" global variables are allowed.
(function TestGlobalExnRefSupported() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExnRef);
  builder.addFunction("push_and_drop_exnref", kSig_v_v)
      .addBody([
        kExprGlobalGet, g.index,
        kExprDrop,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(instance.exports.push_and_drop_exnref);
})();

// Test default value that global "exnref" variables are initialized with.
(function TestGlobalExnRefDefaultValue() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExnRef);
  builder.addFunction('push_and_return_exnref', kSig_e_v)
      .addBody([kExprGlobalGet, g.index])
      .exportFunc();
  let instance = builder.instantiate();

  assertEquals(null, instance.exports.push_and_return_exnref());
})();

// Test custom initialization index for a global "exnref" variable.
(function TestGlobalExnRefInitIndex() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g1_index = builder.addImportedGlobal("m", "exn", kWasmExnRef);
  let g2 = builder.addGlobal(kWasmExnRef);
  g2.init_index = g1_index;  // Initialize {g2} to equal {g1}.
  builder.addFunction('push_and_return_exnref', kSig_e_v)
      .addBody([kExprGlobalGet, g2.index])
      .exportFunc();
  let exception = { x: "my fancy exception" };
  let instance = builder.instantiate({ "m": { "exn": exception }});

  assertSame(exception, instance.exports.push_and_return_exnref());
})();
