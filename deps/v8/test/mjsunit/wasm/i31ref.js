// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function I31RefBasic() {
  print(arguments.callee.name);

  var builder = new WasmModuleBuilder();
  builder.addFunction("signed", kSig_i_i)
    .addLocals(wasmRefType(kWasmI31Ref), 1)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprI31New, kExprLocalTee, 1,
              kGCPrefix, kExprI31GetS])
    .exportFunc();
  builder.addFunction("unsigned", kSig_i_i)
    .addLocals(wasmRefType(kWasmI31Ref), 1)
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprI31New, kExprLocalTee, 1,
              kGCPrefix, kExprI31GetU])
    .exportFunc();

  let instance = builder.instantiate();

  assertEquals(123, instance.exports.signed(123));
  assertEquals(123, instance.exports.unsigned(123));
  // Truncation:
  assertEquals(0x1234, instance.exports.signed(0x80001234));
  assertEquals(0x1234, instance.exports.unsigned(0x80001234));
  // Sign/zero extention:
  assertEquals(-1, instance.exports.signed(0x7fffffff));
  assertEquals(0x7fffffff, instance.exports.unsigned(0x7fffffff));
})();

(function I31RefNullable() {
  print(arguments.callee.name);

  var builder = new WasmModuleBuilder();
  builder.addFunction("i31_null", kSig_i_i)
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
        kExprLocalGet, 0,
        kExprIf, kWasmVoid,
          kExprRefNull, kI31RefCode, kExprLocalSet, 1,
        kExprElse,
          ...wasmI32Const(42), kGCPrefix, kExprI31New, kExprLocalSet, 1,
        kExprEnd,
        kExprLocalGet, 1, kGCPrefix, kExprI31GetS])
    .exportFunc();

  let instance = builder.instantiate();

  assertEquals(42, instance.exports.i31_null(0));
  assertTraps(kTrapNullDereference, () => instance.exports.i31_null(1));
})();
