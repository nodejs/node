// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --experimental-wasm-exnref

// Test that using a mix of legacy and new exception handling instructions in
// the same module is invalid without --wasm-allow-mixed-eh-for-testing.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestTryAndTryTableInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprTry, kWasmVoid,
          kExprEnd,
          kExprTryTable, kWasmVoid, 0,
          kExprEnd,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();

(function TestTryTableAndTryInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprTryTable, kWasmVoid, 0,
          kExprEnd,
          kExprTry, kWasmVoid,
          kExprEnd,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();

(function TestLegacyTryAndNullExnRefInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprTry, kWasmVoid,
          kExprEnd,
          kExprRefNull, kExnRefCode,
          kExprDrop,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();

(function TestNullExnRefAndLegacyTryInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprRefNull, kExnRefCode,
          kExprDrop,
          kExprTry, kWasmVoid,
          kExprEnd,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();

(function TestLegacyTryAndExnRefBlockInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprTry, kWasmVoid,
          kExprEnd,
          kExprBlock, kExnRefCode,
          kExprEnd,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();

(function TestExnRefBlockAndLegacyTryInvalid() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("", kSig_v_v)
      .addBody([
          kExprBlock, kExnRefCode,
          kExprUnreachable,
          kExprEnd,
          kExprDrop,
          kExprTry, kWasmVoid,
          kExprEnd,
      ]).exportFunc();
  assertThrows(() => builder.instantiate(),
      WebAssembly.CompileError, /module uses a mix of legacy and new exception handling instructions/);
})();
