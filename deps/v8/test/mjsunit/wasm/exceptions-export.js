// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestExportMultiple() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_i);
  builder.addExportOfKind("ex1a", kExternalException, except1);
  builder.addExportOfKind("ex1b", kExternalException, except1);
  builder.addExportOfKind("ex2", kExternalException, except2);
  let instance = builder.instantiate();

  assertTrue(Object.prototype.hasOwnProperty.call(instance.exports, 'ex1a'));
  assertTrue(Object.prototype.hasOwnProperty.call(instance.exports, 'ex1b'));
  assertTrue(Object.prototype.hasOwnProperty.call(instance.exports, 'ex2'));
  assertSame(instance.exports.ex1a, instance.exports.ex1b);
  assertNotSame(instance.exports.ex1a, instance.exports.ex2);
})();

(function TestExportOutOfBounds() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addExportOfKind("ex_oob", kExternalException, except + 1);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      'WebAssembly.Module(): exception index 1 out of bounds (1 entry) @+30');
})();

(function TestExportSameNameTwice() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addExportOfKind("ex", kExternalException, except);
  builder.addExportOfKind("ex", kExternalException, except);
  assertThrows(
      () => builder.instantiate(), WebAssembly.CompileError,
      'WebAssembly.Module(): Duplicate export name \'ex\' ' +
          'for exception 0 and exception 0 @+28');
})();

(function TestExportModuleGetExports() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addExportOfKind("ex", kExternalException, except);
  let module = new WebAssembly.Module(builder.toBuffer());

  let exports = WebAssembly.Module.exports(module);
  assertArrayEquals([{ name: "ex", kind: "exception" }], exports);
})();
