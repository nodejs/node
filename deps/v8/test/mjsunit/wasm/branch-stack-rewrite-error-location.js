// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestBrOnNull() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('br_on_null', makeSig([], [kWasmAnyRef]))
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
      kExprRefNull, kI31RefCode,
      kExprRefNull, kI31RefCode,
      kExprBrOnNull, 0,
      kExprLocalSet, 0,
      kExprLocalSet, 0,
      kExprRefNull, kI31RefCode,
    ]);

  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               /local.set\[0\] expected type i31ref, found br_on_null of type anyref @\+31/);
})();

(function TestBrIf() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('br_if', makeSig([], [kWasmAnyRef]))
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
      kExprRefNull, kI31RefCode,
      kExprI32Const, 0,
      kExprBrIf, 0,
      kExprLocalSet, 0,
      kExprLocalSet, 0,
      kExprRefNull, kI31RefCode,
    ]);

  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               /local.set\[0\] expected type i31ref, found br_if of type anyref @\+31/);
})();

(function TestBrOnNonNull() {
  const builder = new WasmModuleBuilder();
  let sig_index = builder.addType(makeSig([], [kWasmAnyRef, kWasmAnyRef]));
  builder.addFunction('br_on_non_null', makeSig([], [kWasmAnyRef, kWasmAnyRef]))
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
      kExprBlock, sig_index,
        kExprRefNull, kI31RefCode,
        kExprRefNull, kAnyRefCode,
        kExprBrOnNonNull, 0,
        kExprLocalSet, 0,
        kExprRefNull, kAnyRefCode,
      kExprEnd,
    ]);

  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               /local.set\[0\] expected type i31ref, found br_on_non_null of type anyref @\+39/);
})();

(function TestBrIfFastPath() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('br_if_fast_path', makeSig([], [kWasmAnyRef]))
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
      kExprRefNull, kAnyRefCode,
      kExprI32Const, 0,
      kExprBrIf, 0,
      kExprLocalSet, 0,
      kExprRefNull, kI31RefCode,
    ]);

  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               /local.set\[0\] expected type i31ref, found br_if of type anyref @\+31/);
})();

(function TestBlockResultMismatch() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('block_mismatch', makeSig([], []))
    .addLocals(wasmRefNullType(kWasmI31Ref), 1)
    .addBody([
      kExprBlock, 0x6e, // anyref
        kExprRefNull, kI31RefCode,
      kExprEnd,
      kExprLocalSet, 0,
    ]);

  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               /local.set\[0\] expected type i31ref, found block of type anyref @\+26/);
})();
