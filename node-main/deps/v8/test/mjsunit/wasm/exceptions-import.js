// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Helper function to return a new exported exception with the {kSig_v_v} type
// signature from an anonymous module. The underlying module is thrown away.
// This allows tests to reason solely about importing exceptions.
function NewExportedTag() {
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_v);
  builder.addExportOfKind("t", kExternalTag, tag);
  let instance = builder.instantiate();
  return instance.exports.t;
}

(function TestImportSimple() {
  print(arguments.callee.name);
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertDoesNotThrow(() => builder.instantiate({ m: { ex: exported }}));
})();

(function TestImportMultiple() {
  print(arguments.callee.name);
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except1 = builder.addImportedTag("m", "ex1", kSig_v_v);
  let except2 = builder.addImportedTag("m", "ex2", kSig_v_v);
  let except3 = builder.addTag(kSig_v_v);
  builder.addExportOfKind("ex2", kExternalTag, except2);
  builder.addExportOfKind("ex3", kExternalTag, except3);
  let instance = builder.instantiate({ m: { ex1: exported, ex2: exported }});

  assertTrue(except1 < except3 && except2 < except3);
  assertEquals(undefined, instance.exports.ex1);
  assertSame(exported, instance.exports.ex2);
  assertNotSame(exported, instance.exports.ex3);
})();

(function TestImportMissing() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertThrows(
      () => builder.instantiate({}), TypeError,
      /module is not an object or function/);
  assertThrows(
      () => builder.instantiate({ m: {}}), WebAssembly.LinkError,
      /tag import requires a WebAssembly.Tag/);
})();

(function TestImportValueMismatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);

  assertThrows(
      () => builder.instantiate({ m: { ex: 23 }}), WebAssembly.LinkError,
      /tag import requires a WebAssembly.Tag/);
  assertThrows(
      () => builder.instantiate({ m: { ex: {} }}), WebAssembly.LinkError,
      /tag import requires a WebAssembly.Tag/);
  var monkey = Object.create(NewExportedTag());
  assertThrows(
      () => builder.instantiate({ m: { ex: monkey }}), WebAssembly.LinkError,
      /tag import requires a WebAssembly.Tag/);
})();

(function TestImportSignatureMismatch() {
  print(arguments.callee.name);
  let exported = NewExportedTag();
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_i);

  assertThrows(
      () => builder.instantiate({ m: { ex: exported }}), WebAssembly.LinkError,
      /imported tag does not match the expected type/);
})();

(function TestImportModuleGetImports() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addImportedTag("m", "ex", kSig_v_v);
  let module = new WebAssembly.Module(builder.toBuffer());

  let imports = WebAssembly.Module.imports(module);
  assertArrayEquals([{ module: "m", name: "ex", kind: "tag" }], imports);
})();
