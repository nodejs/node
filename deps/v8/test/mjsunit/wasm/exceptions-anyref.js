// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh --experimental-wasm-anyref --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");
load("test/mjsunit/wasm/exceptions-utils.js");

// Test the encoding of a thrown exception with a null-ref value.
(function TestThrowRefNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction("throw_null", kSig_v_v)
      .addBody([
        kExprRefNull,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [null], () => instance.exports.throw_null());
})();

// Test throwing/catching the null-ref value.
(function TestThrowCatchRefNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction("throw_catch_null", kSig_i_i)
      .addBody([
        kExprTry, kWasmAnyRef,
          kExprGetLocal, 0,
          kExprI32Eqz,
          kExprIf, kWasmAnyRef,
            kExprRefNull,
            kExprThrow, except,
          kExprElse,
            kExprI32Const, 42,
            kExprReturn,
          kExprEnd,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
        kExprRefIsNull,
        kExprIf, kWasmI32,
          kExprI32Const, 23,
        kExprElse,
          kExprUnreachable,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.throw_catch_null(0));
  assertEquals(42, instance.exports.throw_catch_null(1));
})();

// Test the encoding of a thrown exception with a reference type value.
(function TestThrowRefParam() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction("throw_param", kSig_v_r)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();
  let o = new Object();

  assertWasmThrows(instance, except, [o], () => instance.exports.throw_param(o));
  assertWasmThrows(instance, except, [1], () => instance.exports.throw_param(1));
  assertWasmThrows(instance, except, [2.3], () => instance.exports.throw_param(2.3));
  assertWasmThrows(instance, except, ["str"], () => instance.exports.throw_param("str"));
})();

// Test throwing/catching the reference type value.
(function TestThrowCatchRefParam() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction("throw_catch_param", kSig_r_r)
      .addBody([
        kExprTry, kWasmAnyRef,
          kExprGetLocal, 0,
          kExprThrow, except,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();
  let o = new Object();

  assertEquals(o, instance.exports.throw_catch_param(o));
  assertEquals(1, instance.exports.throw_catch_param(1));
  assertEquals(2.3, instance.exports.throw_catch_param(2.3));
  assertEquals("str", instance.exports.throw_catch_param("str"));
})();

// Test throwing/catching a function reference type value.
(function TestThrowCatchAnyFunc() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_a);
  builder.addFunction("throw_catch_local", kSig_r_v)
      .addLocals({anyfunc_count: 1})
      .addBody([
        kExprTry, kWasmAnyFunc,
          kExprGetLocal, 0,
          kExprThrow, except,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(null, instance.exports.throw_catch_local());
})();

// Test throwing/catching an encapsulated exception type value.
(function TestThrowCatchExceptRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_e);
  builder.addFunction("throw_catch_param", kSig_e_e)
      .addBody([
        kExprTry, kWasmExceptRef,
          kExprGetLocal, 0,
          kExprThrow, except,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();
  let e = new Error("my encapsulated error");

  assertEquals(e, instance.exports.throw_catch_param(e));
  assertEquals(1, instance.exports.throw_catch_param(1));
  assertEquals(2.3, instance.exports.throw_catch_param(2.3));
  assertEquals("str", instance.exports.throw_catch_param("str"));
})();
