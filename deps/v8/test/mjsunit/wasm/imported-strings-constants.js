// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-imported-strings

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const kRefExtern = wasmRefType(kWasmExternRef);

(function() {
  let invalid_builder = new WasmModuleBuilder();
  invalid_builder.addImport("'", "foo", kSig_v_v);
  assertThrows(
      () => invalid_builder.instantiate(
          {"'": {foo: () => {}}}, {importedStringConstants: true}),
      WebAssembly.LinkError,
      'WebAssembly.Module(): String constant import #0 "foo" must be ' +
          'an immutable global of type (ref extern) @+17');

  let invalid_builder2 = new WasmModuleBuilder();
  invalid_builder2.addImportedGlobal("'", "bar", kWasmI32);
  assertThrows(
      () => invalid_builder2.instantiate(
          {"'": {bar: 42}}, {importedStringConstants: true}),
      WebAssembly.LinkError,
      'WebAssembly.Module(): String constant import #0 "bar" must be ' +
          'an immutable global of type (ref extern) @+11');
})();

let builder = new WasmModuleBuilder();
let $log = builder.addImport("m", "log", kSig_v_r);
let hello_world = "Hello, world!";
let multi_byte = "\uD83D\uDE02";  // U+1F602
let $hello = builder.addImportedGlobal("'", hello_world, kRefExtern, false);
let $multi = builder.addImportedGlobal("'", multi_byte, kRefExtern, false);

builder.addFunction("main", kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $hello,
  kExprCallFunction, $log,
]);
builder.addFunction("multi", kSig_v_v).exportFunc().addBody([
  kExprGlobalGet, $multi,
  kExprCallFunction, $log,
]);


var callback_value = "the test has not yet run";
function log(x) {
  callback_value = x;
}
let kBuiltins = {importedStringConstants: true};
let kImports = {
  m: {log},
  "'": {
    [hello_world]: "This isn't the right string",
    [multi_byte]: "This also isn't the right string",
  },
};
let instance = builder.instantiate(kImports, kBuiltins);
instance.exports.main();
assertEquals(hello_world, callback_value);
instance.exports.multi();
assertEquals(multi_byte, callback_value);
// The magically satisfied string import isn't in the imports list.
let runtime_imports = WebAssembly.Module.imports(builder.toModule(kBuiltins));
assertArrayEquals(
    [{module: 'm', name: 'log', kind: 'function'}], runtime_imports);
// If we don't set the flag, we do get the plain old imports.
let instance2 = builder.instantiate(kImports, {importedStringConstants: false});
instance2.exports.main();
assertEquals("This isn't the right string", callback_value);
