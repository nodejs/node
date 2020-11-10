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
          kExprLocalGet, 0,
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
        kExprLocalGet, 0,
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
          kExprLocalGet, 0,
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
          kExprLocalGet, 0,
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
(function TestThrowCatchExnRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_e);
  builder.addFunction("throw_catch_param", kSig_e_e)
      .addBody([
        kExprTry, kWasmExnRef,
          kExprLocalGet, 0,
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

// Test storing and loading to/from an exception type table.
(function TestTableExnRef() {
  print(arguments.callee.name);
  let kSig_e_i = makeSig([kWasmI32], [kWasmExnRef]);
  let kSig_v_ie = makeSig([kWasmI32, kWasmExnRef], []);
  let builder = new WasmModuleBuilder();
  let table = builder.addTable(kWasmExnRef, 2).exportAs("table");
  builder.addFunction("table_load", kSig_e_i)
      .addBody([
        kExprLocalGet, 0,
        kExprTableGet, table.index
      ]).exportFunc();
  builder.addFunction("table_store", kSig_v_ie)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprTableSet, table.index
      ]).exportFunc();
  let instance = builder.instantiate();
  let e0 = new Error("my encapsulated error");
  let e1 = new Error("my other encapsulated error");

  assertNull(instance.exports.table_load(0));
  assertNull(instance.exports.table_load(1));
  assertNull(instance.exports.table.get(0));
  assertNull(instance.exports.table.get(1));

  instance.exports.table_store(0, e0);
  assertSame(e0, instance.exports.table_load(0));
  assertNull(instance.exports.table_load(1));
  assertSame(e0, instance.exports.table.get(0));
  assertNull(instance.exports.table.get(1));

  instance.exports.table.set(1, e1);
  assertSame(e0, instance.exports.table_load(0));
  assertSame(e1, instance.exports.table_load(1));
  assertSame(e0, instance.exports.table.get(0));
  assertSame(e1, instance.exports.table.get(1));
})();

// 'br_on_exn' on a null-ref value should trap.
(function TestBrOnExnNullRefSimple() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction('br_on_exn_nullref', kSig_v_v)
      .addBody([
        kExprRefNull,
        kExprBrOnExn, 0, except,
        kExprDrop
      ]).exportFunc();
  let instance = builder.instantiate();

  assertTraps(kTrapBrOnExnNullRef, () => instance.exports.br_on_exn_nullref());
})();

(function TestBrOnExnNullRefFromJS() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  let imp = builder.addImport('imp', 'ort', kSig_i_i);
  let kConstant0 = 11;
  let kNoMatch = 13;
  builder.addFunction('call_import', kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprCallFunction, imp,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprDrop,
          kExprI32Const, kNoMatch,
        kExprEnd
      ]).exportFunc();
  let instance;
  function js_import(i) {
    if (i == 0) return kConstant0;     // Will return kConstant0.
    if (i == 1) throw new Error('1');  // Will not match.
    if (i == 2) throw null;            // Will trap.
    throw undefined;                   // Will not match.
  }
  instance = builder.instantiate({imp: {ort: js_import}});

  assertEquals(kConstant0, instance.exports.call_import(0));
  assertEquals(kNoMatch, instance.exports.call_import(1));
  assertTraps(kTrapBrOnExnNullRef, () => instance.exports.call_import(2));
  assertEquals(kNoMatch, instance.exports.call_import(3));
})();

// 'rethrow' on a null-ref value should trap.
(function TestRethrowNullRefSimple() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_r);
  builder.addFunction('rethrow_nullref', kSig_v_v)
      .addBody([
        kExprRefNull,
        kExprRethrow
      ]).exportFunc();
  let instance = builder.instantiate();

  assertTraps(kTrapRethrowNullRef, () => instance.exports.rethrow_nullref());
})();

(function TestRethrowNullRefFromJS() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  let imp = builder.addImport('imp', 'ort', kSig_i_i);
  let kSuccess = 11;
  builder.addFunction('call_import', kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprCallFunction, imp,
        kExprCatch,
          kExprRethrow,
        kExprEnd
      ]).exportFunc();
  let instance;
  function js_import(i) {
    if (i == 0) return kSuccess;       // Will return kSuccess.
    if (i == 1) throw new Error('1');  // Will rethrow.
    if (i == 2) throw null;            // Will trap.
    throw undefined;                   // Will rethrow.
  }
  instance = builder.instantiate({imp: {ort: js_import}});

  assertEquals(kSuccess, instance.exports.call_import(0));
  assertThrows(() => instance.exports.call_import(1), Error, '1');
  assertTraps(kTrapRethrowNullRef, () => instance.exports.call_import(2));
  assertThrowsEquals(() => instance.exports.call_import(3), undefined);
})();
