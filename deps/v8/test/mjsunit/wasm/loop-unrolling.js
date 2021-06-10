// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-typed-funcref --experimental-wasm-eh
// Flags: --wasm-loop-unrolling --experimental-wasm-return-call
// Needed for exceptions-utils.js.
// Flags: --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");
load("test/mjsunit/wasm/exceptions-utils.js");

// Test the interaction between multireturn and loop unrolling.
(function MultiBlockResultTest() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_i)
    .addBody([
      ...wasmI32Const(1),
        kExprLet, kWasmVoid, 1, 1, kWasmI32,
        kExprLoop, kWasmVoid,
          ...wasmI32Const(10),
          kExprLet, kWasmVoid, 1, 1, kWasmI32,
            kExprLocalGet, 0,
            kExprLocalGet, 1,
            kExprI32Sub,
            kExprLocalGet, 2,
            kExprI32Add,
            kExprReturn, // (second let) - (first let) + parameter
          kExprEnd,
        kExprEnd,
      kExprEnd,
      ...wasmI32Const(0)])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(100), 109);
})();

// Test the interaction between tail calls and loop unrolling.
(function TailCallTest() {
  let builder = new WasmModuleBuilder();

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0]);

  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 0,
        kExprIf, kWasmVoid,
          kExprLocalGet, 0,
          kExprReturnCall, callee.index,
        kExprElse,
          kExprBr, 1,
        kExprEnd,
      kExprEnd,
      kExprUnreachable
    ])
    .exportAs("main");

  let module = new WebAssembly.Module(builder.toBuffer());
  let instance = new WebAssembly.Instance(module);
  assertEquals(instance.exports.main(1), 1);
})();

// Test the interaction between the eh proposal and loop unrolling.

(function TestRethrowNested() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  builder.addFunction("rethrow_nested", kSig_i_i)
    .addBody([
      kExprLoop, kWasmI32,
      kExprTry, kWasmI32,
        kExprLoop, kWasmI32,
          kExprThrow, except2,
        kExprEnd,
      kExprCatch, except2,
        kExprTry, kWasmI32,
          kExprThrow, except1,
        kExprCatch, except1,
          kExprLocalGet, 0,
          kExprI32Const, 0,
          kExprI32Eq,
          kExprIf, kWasmVoid,
            kExprLoop, kWasmVoid,
              kExprRethrow, 2,
            kExprEnd,
          kExprEnd,
          kExprLocalGet, 0,
          kExprI32Const, 1,
          kExprI32Eq,
          kExprIf, kWasmVoid,
            kExprLoop, kWasmVoid,
              kExprRethrow, 3,
            kExprEnd,
          kExprEnd,
          kExprI32Const, 23,
        kExprEnd,
      kExprEnd,
      kExprEnd])
    .exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except1, [],
                   () => instance.exports.rethrow_nested(0));
  assertWasmThrows(instance, except2, [],
                   () => instance.exports.rethrow_nested(1));
  assertEquals(23, instance.exports.rethrow_nested(2));
})();

(function TestThrow() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  builder.addFunction("throw", kSig_i_i)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 0,
        kExprI32Const, 10,
        kExprI32GtS,
        kExprIf, kWasmVoid,
          kExprThrow, except1,
        kExprElse,
          kExprLocalGet, 0,
          kExprI32Const, 1,
          kExprI32Add,
          kExprLocalSet, 0,
          kExprBr, 1,
        kExprEnd,
      kExprEnd,
      kExprLocalGet, 0
    ])
    .exportFunc();

  let instance = builder.instantiate();
  assertWasmThrows(instance, except1, [], ()=>instance.exports.throw(0));
})();

(function TestThrowCatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  builder.addFunction("throw_catch", kSig_i_i)
    .addBody([
      kExprLoop, kWasmI32,
        kExprTry, kWasmI32,
          kExprLocalGet, 0,
          kExprI32Const, 10,
          kExprI32GtS,
          kExprIf, kWasmVoid,
            kExprThrow, except1,
          kExprElse,
            kExprLocalGet, 0,
            kExprI32Const, 1,
            kExprI32Add,
            kExprLocalSet, 0,
            kExprBr, 2,
          kExprEnd,
          kExprLocalGet, 0,
        kExprCatch, except1,
          kExprLocalGet, 0,
        kExprEnd,
      kExprEnd])
    .exportFunc();

  let instance = builder.instantiate();
  assertEquals(11, instance.exports.throw_catch(0));
})();
