// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
d8.file.execute("test/mjsunit/wasm/exceptions-utils.js");

// Test the encoding of a thrown exception with a null-ref value.
(function TestThrowRefNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_r);
  builder.addFunction("throw_null", kSig_v_v)
      .addBody([
        kExprRefNull, kExternRefCode,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [null],
                   () => instance.exports.throw_null());
})();

// Test throwing/catching the null-ref value.
(function TestThrowCatchRefNull() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_r);
  builder.addFunction("throw_catch_null", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmI32,
            kExprRefNull, kExternRefCode,
            kExprThrow, except,
          kExprElse,
            kExprI32Const, 42,
          kExprEnd,
        kExprCatch, except,
          kExprRefIsNull,
          kExprIf, kWasmI32,
            kExprI32Const, 23,
          kExprElse,
            kExprUnreachable,
          kExprEnd,
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
  let except = builder.addTag(kSig_v_r);
  builder.addFunction("throw_param", kSig_v_r)
      .addBody([
        kExprLocalGet, 0,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();
  let o = new Object();

  assertWasmThrows(instance, except, [o],
                   () => instance.exports.throw_param(o));
  assertWasmThrows(instance, except, [1],
                   () => instance.exports.throw_param(1));
  assertWasmThrows(instance, except, [2.3],
                   () => instance.exports.throw_param(2.3));
  assertWasmThrows(instance, except, ["str"],
                   () => instance.exports.throw_param("str"));
})();

// Test throwing/catching the reference type value.
(function TestThrowCatchRefParam() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addTag(kSig_v_r);
  builder.addFunction("throw_catch_param", kSig_r_r)
      .addBody([
        kExprTry, kExternRefCode,
          kExprLocalGet, 0,
          kExprThrow, except,
        kExprCatch, except,
          // fall-through
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();
  let o = new Object();

  assertEquals(o, instance.exports.throw_catch_param(o));
  assertEquals(1, instance.exports.throw_catch_param(1));
  assertEquals(2.3, instance.exports.throw_catch_param(2.3));
  assertEquals("str", instance.exports.throw_catch_param("str"));
})();
