// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// Test that a catch-all block handles all exceptions thrown locally, but is
// applied only after typed catch blocks have been handled.
(function TestCatchAllLocal() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  builder.addFunction("catchall_local", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprI32Const, 1,
          kExprI32Eq,
          kExprIf, kWasmStmt,
            kExprThrow, except1,
          kExprEnd,
          kExprGetLocal, 0,
          kExprI32Const, 2,
          kExprI32Eq,
          kExprIf, kWasmStmt,
            kExprThrow, except2,
          kExprEnd,
          kExprI32Const, 61,
        kExprCatch, except1,
          kExprI32Const, 23,
        kExprCatchAll,
          kExprI32Const, 42,
        kExprEnd
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.catchall_local(1));
  assertEquals(42, instance.exports.catchall_local(2));
  assertEquals(61, instance.exports.catchall_local(3));
})();

// Test that a catch-all block handles all exceptions thrown externally, even
// those originating from JavaScript instead of WebAssembly.
(function TestCatchAllExternal() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_v_v);
  let fun = builder.addImport("m", "f", sig_index);
  let except = builder.addException(kSig_v_v);
  builder.addFunction("throw", kSig_v_v)
      .addBody([
        kExprThrow, except
      ]).exportFunc();
  builder.addFunction("catchall_external", kSig_i_v)
      .addBody([
        kExprTry, kWasmI32,
          kExprCallFunction, fun,
          kExprUnreachable,
        kExprCatch, except,
          kExprI32Const, 23,
        kExprCatchAll,
          kExprI32Const, 42,
        kExprEnd,
      ]).exportFunc();
  let ex_obj = new Error("my exception");
  let instance = builder.instantiate({ m: { f: function() { throw ex_obj }}});

  assertThrows(() => instance.exports.throw(), WebAssembly.RuntimeError);
  assertEquals(42, instance.exports.catchall_external());  // From JavaScript.
  try {
    instance.exports.throw();
  } catch (e) {
    ex_obj = e;
  }
  assertEquals(23, instance.exports.catchall_external());  // From WebAssembly.
})();

// Test that expressions in a catch-all block are considered to be outside of
// the corresponding try block. Exceptions raised in them will percolate up.
(function TestCatchAllThrowing() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  builder.addFunction("catchall", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprThrow, except1,
      kExprCatchAll,
        kExprThrow, except2,
      kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertThrows(() => instance.exports.catchall(), WebAssembly.RuntimeError);
})();

// Test that empty try blocks (with no expression that could potentially throw)
// are supported properly, even in the presence of unreachable catch blocks.
(function TestCatchAllEmptyBlock() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  builder.addFunction("catchall", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
      kExprCatch, except1,
        kExprThrow, except2,
      kExprCatchAll,
        kExprThrow, except2,
      kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(() => instance.exports.catchall());
})();
