// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --allow-natives-syntax

// Note that this test does not pass --experimental-wasm-anyref on purpose so
// that we make sure the two flags can be controlled separately/independently.

load("test/mjsunit/wasm/wasm-module-builder.js");

// First we just test that "except_ref" global variables are allowed.
(function TestGlobalExceptRefSupported() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExceptRef);
  builder.addFunction("push_and_drop_except_ref", kSig_v_v)
      .addBody([
        kExprGetGlobal, g.index,
        kExprDrop,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(instance.exports.push_and_drop_except_ref);
})();

// Test default value that global "except_ref" variables are initialized with.
(function TestGlobalExceptRefDefaultValue() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExceptRef);
  builder.addFunction('push_and_return_except_ref', kSig_e_v)
      .addBody([kExprGetGlobal, g.index])
      .exportFunc();
  let instance = builder.instantiate();

  assertEquals(null, instance.exports.push_and_return_except_ref());
})();

// Test storing a caught exception into an exported mutable "except_ref" global.
(function TestGlobalExceptRefSetCaught() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  let g = builder.addGlobal(kWasmExceptRef, true).exportAs("exn");
  builder.addFunction('catch_and_set_except_ref', kSig_v_i)
      .addBody([
        kExprTry, kWasmStmt,
          kExprGetLocal, 0,
          kExprThrow, except,
        kExprCatch,
          kExprSetGlobal, g.index,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(() => instance.exports.catch_and_set_except_ref(23));
  let exception = instance.exports.exn.value;  // Exported mutable global.
  assertInstanceof(exception, WebAssembly.RuntimeError);
  assertEquals(except, %GetWasmExceptionId(exception, instance));
})();

// Test storing a parameter into an exported mutable "except_ref" global.
(function TestGlobalExceptRefSetParameter() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExceptRef, true).exportAs("exn");
  builder.addFunction('set_param_except_ref', kSig_v_e)
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

  assertDoesNotThrow(() => instance.exports.set_param_except_ref(exception));
  assertEquals(exception, instance.exports.exn.value);
})();

// Test loading an imported "except_ref" global and re-throwing the exception.
(function TestGlobalExceptRefGetImportedAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g_index = builder.addImportedGlobal("m", "exn", kWasmExceptRef);
  builder.addFunction('rethrow_except_ref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g_index,
        kExprRethrow,
      ]).exportFunc();
  let exception = "my fancy exception";
  let instance = builder.instantiate({ "m": { "exn": exception }});

  assertThrowsEquals(() => instance.exports.rethrow_except_ref(), exception);
})();

// Test loading an exported mutable "except_ref" being changed from the outside.
(function TestGlobalExceptRefGetExportedMutableAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g = builder.addGlobal(kWasmExceptRef, true).exportAs("exn");
  builder.addFunction('rethrow_except_ref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g.index,
        kExprRethrow,
      ]).exportFunc();
  let instance = builder.instantiate();

  let exception1 = instance.exports.exn.value = "my fancy exception";
  assertThrowsEquals(() => instance.exports.rethrow_except_ref(), exception1);
  let exception2 = instance.exports.exn.value = "an even fancier exception";
  assertThrowsEquals(() => instance.exports.rethrow_except_ref(), exception2);
})();

// TODO(mstarzinger): Add the following test once proposal makes it clear how
// far interaction with the mutable globals proposal is intended to go.
// Test loading an imported mutable "except_ref" being changed from the outside.
/*(function TestGlobalExceptRefGetImportedMutableAndRethrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g_index = builder.addImportedGlobal("m", "exn", kWasmExceptRef, true);
  builder.addFunction('rethrow_except_ref', kSig_v_v)
      .addBody([
        kExprGetGlobal, g_index,
        kExprRethrow,
      ]).exportFunc();
  let exception1 = "my fancy exception";
  let desc = { value: 'except_ref', mutable: true };
  let mutable_global = new WebAssembly.Global(desc, exception1);
  let instance = builder.instantiate({ "m": { "exn": mutable_global }});

  assertThrowsEquals(() => instance.exports.rethrow_except_ref(), exception1);
  let exception2 = mutable_global.value = "an even fancier exception";
  assertThrowsEquals(() => instance.exports.rethrow_except_ref(), exception2);
})();*/

// Test custom initialization index for a global "except_ref" variable.
(function TestGlobalExceptRefInitIndex() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let g1_index = builder.addImportedGlobal("m", "exn", kWasmExceptRef);
  let g2 = builder.addGlobal(kWasmExceptRef);
  g2.init_index = g1_index;  // Initialize {g2} to equal {g1}.
  builder.addFunction('push_and_return_except_ref', kSig_e_v)
      .addBody([kExprGetGlobal, g2.index])
      .exportFunc();
  let exception = { x: "my fancy exception" };
  let instance = builder.instantiate({ "m": { "exn": exception }});

  assertSame(exception, instance.exports.push_and_return_except_ref());
})();
