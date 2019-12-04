// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --allow-natives-syntax

// Note that this test does not pass --experimental-wasm-anyref on purpose so
// that we make sure the two flags can be controlled separately/independently.

load("test/mjsunit/wasm/wasm-module-builder.js");

// First we just test that "exnref" global variables are allowed.
(function TestGlobalExnRefSupported() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExnRef);
  builder.addFunction("push_and_drop_exnref", kSig_v_v)
      .addBody([
        kExprGetGlobal, g.index,
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
      .addBody([kExprGetGlobal, g.index])
      .exportFunc();
  let instance = builder.instantiate();

  assertEquals(null, instance.exports.push_and_return_exnref());
})();

// Test storing a caught exception into an exported mutable "exnref" global.
(function TestGlobalExnRefSetCaught() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  let g = builder.addGlobal(kWasmExnRef, true).exportAs("exn");
  builder.addFunction('catch_and_set_exnref', kSig_v_i)
      .addBody([
        kExprTry, kWasmStmt,
          kExprGetLocal, 0,
          kExprThrow, except,
        kExprCatch,
          kExprSetGlobal, g.index,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(() => instance.exports.catch_and_set_exnref(23));
  let exception = instance.exports.exn.value;  // Exported mutable global.
  assertInstanceof(exception, WebAssembly.RuntimeError);
  assertEquals(except, %GetWasmExceptionId(exception, instance));
})();

// Test storing a parameter into an exported mutable "exnref" global.
(function TestGlobalExnRefSetParameter() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExnRef, true).exportAs("exn");
  builder.addFunction('set_param_exnref', kSig_v_e)
      .addBody([
        kExprTry, kWasmStmt,
          kExprGetLocal, 0,
          kExprRethrow,
        kExprCatch,
          kExprSetGlobal, g.index,
        kExprEnd,
      ]).exportFunc();
  let exception = "my fancy exception";
  let instance = builder.instantiate();

  assertDoesNotThrow(() => instance.exports.set_param_exnref(exception));
  assertEquals(exception, instance.exports.exn.value);
})();

// Test loading an imported "exnref" global and re-throwing the exception.
(function TestGlobalExnRefGetImportedAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g_index = builder.addImportedGlobal("m", "exn", kWasmExnRef);
  builder.addFunction('rethrow_exnref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g_index,
        kExprRethrow,
      ]).exportFunc();
  let exception = "my fancy exception";
  let instance = builder.instantiate({ "m": { "exn": exception }});

  assertThrowsEquals(() => instance.exports.rethrow_exnref(), exception);
})();

// Test loading an exported mutable "exnref" being changed from the outside.
(function TestGlobalExnRefGetExportedMutableAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExnRef, true).exportAs("exn");
  builder.addFunction('rethrow_exnref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g.index,
        kExprRethrow,
      ]).exportFunc();
  let instance = builder.instantiate();

  let exception1 = instance.exports.exn.value = "my fancy exception";
  assertThrowsEquals(() => instance.exports.rethrow_exnref(), exception1);
  let exception2 = instance.exports.exn.value = "an even fancier exception";
  assertThrowsEquals(() => instance.exports.rethrow_exnref(), exception2);
})();

// Test loading an imported mutable "exnref" being changed from the outside.
(function TestGlobalExnRefGetImportedMutableAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g_index = builder.addImportedGlobal("m", "exn", kWasmExnRef, true);
  builder.addFunction('rethrow_exnref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g_index,
        kExprRethrow,
      ]).exportFunc();
  let exception1 = "my fancy exception";
  let desc = { value: 'exnref', mutable: true };
  let mutable_global = new WebAssembly.Global(desc, exception1);
  let instance = builder.instantiate({ "m": { "exn": mutable_global }});

  assertThrowsEquals(() => instance.exports.rethrow_exnref(), exception1);
  let exception2 = mutable_global.value = "an even fancier exception";
  assertThrowsEquals(() => instance.exports.rethrow_exnref(), exception2);
})();

// Test custom initialization index for a global "exnref" variable.
(function TestGlobalExnRefInitIndex() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g1_index = builder.addImportedGlobal("m", "exn", kWasmExnRef);
  let g2 = builder.addGlobal(kWasmExnRef);
  g2.init_index = g1_index;  // Initialize {g2} to equal {g1}.
  builder.addFunction('push_and_return_exnref', kSig_e_v)
      .addBody([kExprGetGlobal, g2.index])
      .exportFunc();
  let exception = { x: "my fancy exception" };
  let instance = builder.instantiate({ "m": { "exn": exception }});

  assertSame(exception, instance.exports.push_and_return_exnref());
})();
