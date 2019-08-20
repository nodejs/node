// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");
load("test/mjsunit/wasm/exceptions-utils.js");

// Test that rethrow expressions work inside catch blocks.
(function TestRethrowInCatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("rethrow0", kSig_v_v)
      .addBody([
        kExprTry, kWasmStmt,
          kExprThrow, except,
        kExprCatch,
          kExprRethrow,
        kExprEnd,
  ]).exportFunc();
  builder.addFunction("rethrow1", kSig_i_i)
      .addLocals({except_count: 1})
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatch,
          kExprSetLocal, 1,
          kExprGetLocal, 0,
          kExprI32Eqz,
          kExprIf, kWasmStmt,
            kExprGetLocal, 1,
            kExprRethrow,
          kExprEnd,
          kExprI32Const, 23,
        kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [], () => instance.exports.rethrow0());
  assertWasmThrows(instance, except, [], () => instance.exports.rethrow1(0));
  assertEquals(23, instance.exports.rethrow1(1));
})();

// Test that rethrow expressions work properly even in the presence of multiple
// nested handlers being involved.
(function TestRethrowNested() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  builder.addFunction("rethrow_nested", kSig_i_i)
      .addLocals({except_count: 2})
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except2,
        kExprCatch,
          kExprSetLocal, 2,
          kExprTry, kWasmI32,
            kExprThrow, except1,
          kExprCatch,
            kExprSetLocal, 1,
            kExprGetLocal, 0,
            kExprI32Const, 0,
            kExprI32Eq,
            kExprIf, kWasmStmt,
              kExprGetLocal, 1,
              kExprRethrow,
            kExprEnd,
            kExprGetLocal, 0,
            kExprI32Const, 1,
            kExprI32Eq,
            kExprIf, kWasmStmt,
              kExprGetLocal, 2,
              kExprRethrow,
            kExprEnd,
            kExprI32Const, 23,
          kExprEnd,
        kExprEnd,
  ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except1, [], () => instance.exports.rethrow_nested(0));
  assertWasmThrows(instance, except2, [], () => instance.exports.rethrow_nested(1));
  assertEquals(23, instance.exports.rethrow_nested(2));
})();

// Test that an exception being rethrow can be caught by another local catch
// block in the same function without ever unwinding the activation.
(function TestRethrowRecatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("rethrow_recatch", kSig_i_i)
      .addLocals({except_count: 1})
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatch,
          kExprSetLocal, 1,
          kExprTry, kWasmI32,
            kExprGetLocal, 0,
            kExprI32Eqz,
            kExprIf, kWasmStmt,
              kExprGetLocal, 1,
              kExprRethrow,
            kExprEnd,
            kExprI32Const, 42,
          kExprCatch,
            kExprDrop,
            kExprI32Const, 23,
          kExprEnd,
        kExprEnd,
  ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.rethrow_recatch(0));
  assertEquals(42, instance.exports.rethrow_recatch(1));
})();
