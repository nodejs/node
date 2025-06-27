// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/exceptions-utils.js");

// Test that rethrow expressions can target catch blocks.
(function TestRethrowInCatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  builder.addFunction("rethrow0", kSig_v_v)
      .addBody([
        kExprTry, kWasmVoid,
          kExprThrow, except,
        kExprCatch, except,
          kExprRethrow, 0,
        kExprEnd,
  ]).exportFunc();
  builder.addFunction("rethrow1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatch, except,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprRethrow, 1,
          kExprEnd,
          kExprI32Const, 23,
        kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [], () => instance.exports.rethrow0());
  assertWasmThrows(instance, except, [], () => instance.exports.rethrow1(0));
  assertEquals(23, instance.exports.rethrow1(1));
})();

// Test that rethrow expressions can target catch-all blocks.
(function TestRethrowInCatchAll() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_v);
  builder.addFunction("rethrow0", kSig_v_v)
      .addBody([
        kExprTry, kWasmVoid,
          kExprThrow, except,
        kExprCatchAll,
          kExprRethrow, 0,
        kExprEnd,
  ]).exportFunc();
  builder.addFunction("rethrow1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatchAll,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprRethrow, 1,
          kExprEnd,
          kExprI32Const, 23,
        kExprEnd
  ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [], () => instance.exports.rethrow0());
  assertWasmThrows(instance, except, [], () => instance.exports.rethrow1(0));
  assertEquals(23, instance.exports.rethrow1(1));
})();

// Test that rethrow expression properly target the correct surrounding try
// block even in the presence of multiple handlers being involved.
(function TestRethrowNested() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addTag(kSig_v_v);
  let except2 = builder.addTag(kSig_v_v);
  builder.addFunction("rethrow_nested", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except2,
        kExprCatch, except2,
          kExprTry, kWasmI32,
            kExprThrow, except1,
          kExprCatch, except1,
            kExprLocalGet, 0,
            kExprI32Const, 0,
            kExprI32Eq,
            kExprIf, kWasmVoid,
              kExprRethrow, 1,
            kExprEnd,
            kExprLocalGet, 0,
            kExprI32Const, 1,
            kExprI32Eq,
            kExprIf, kWasmVoid,
              kExprRethrow, 2,
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
  let except = builder.addTag(kSig_v_v);
  builder.addFunction("rethrow_recatch", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, except,
        kExprCatch, except,
          kExprTry, kWasmI32,
            kExprLocalGet, 0,
            kExprI32Eqz,
            kExprIf, kWasmVoid,
              kExprRethrow, 2,
            kExprEnd,
            kExprI32Const, 42,
          kExprCatch, except,
            kExprI32Const, 23,
          kExprEnd,
        kExprEnd,
  ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.rethrow_recatch(0));
  assertEquals(42, instance.exports.rethrow_recatch(1));
})();
