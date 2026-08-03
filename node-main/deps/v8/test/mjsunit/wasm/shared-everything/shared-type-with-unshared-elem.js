// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(manoskouk): Fix error messages once we support shared globals, tables,
// and functions
// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestSharedTableWithUnsharedElementType() {
  const builder = new WasmModuleBuilder();
  builder.addTable(kWasmFuncRef, 0, undefined, undefined, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /shared tables are not supported yet/);
})();

(function TestSharedGlobalWithUnsharedElementType() {
  const builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmFuncRef, false, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /shared globals are not supported yet/);
})();

(function TestImportedSharedTableWithUnsharedElementType() {
  const builder = new WasmModuleBuilder();
  builder.addImportedTable("import", "table", 0, 10, kWasmFuncRef, /*is_shared*/ true);
  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
    /shared tables are not supported yet/);
})();

(function TestImportedSharedGlobalWithUnsharedElementType() {
  const builder = new WasmModuleBuilder();
  builder.addImportedGlobal("import", "global", kWasmFuncRef, true, /*is_shared*/ true);
  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
    /shared globals are not supported yet/);
})();

(function TestSharedArrayWithUnsharedElement() {
  const builder = new WasmModuleBuilder();
  builder.addArray(kWasmFuncRef, false, kNoSuperType, false, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /shared array must have shared element type/);
})();

(function TestSharedStructWithUnsharedField() {
  const builder = new WasmModuleBuilder();
  builder.addStruct(
    [makeField(kWasmI32, false), makeField(kWasmFuncRef, false)],
    kNoSuperType, false, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /Type 0: shared struct must have shared field types, actual type for field 1 is funcref/);
})();

(function TestSharedSignatureWithUnsharedParameter() {
  const builder = new WasmModuleBuilder();
  let sig = makeSig([kWasmFuncRef], [kWasmI32]);
  builder.addType(sig, kNoSuperType, false, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /shared functions\/continuations are not supported yet/);
})();

(function TestSharedSignatureWithUnsharedReturn() {
  const builder = new WasmModuleBuilder();
  let sig = makeSig([kWasmI32], [kWasmFuncRef]);
  builder.addType(sig, kNoSuperType, false, /*is_shared*/ true);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /shared functions\/continuations are not supported yet/);
})();
