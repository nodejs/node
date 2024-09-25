// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js

// Instantiate a module with an imported global and return the global.
function instantiateImportedGlobal(module, name, type, mutable, importedStringConstants) {
  let builder = new WasmModuleBuilder();
  builder.addImportedGlobal(module, name, type, mutable);
  builder.addExportOfKind("global", kExternalGlobal, 0);
  let bytes = builder.toBuffer();
  let mod = new WebAssembly.Module(bytes, { importedStringConstants });
  let instance = new WebAssembly.Instance(mod, {});
  return instance.exports["global"];
}

const badGlobalTypes = [
  [kWasmAnyRef, false],
  [kWasmAnyRef, true],
  [wasmRefType(kWasmAnyRef), false],
  [wasmRefType(kWasmAnyRef), true],
  [kWasmFuncRef, false],
  [kWasmFuncRef, true],
  [wasmRefType(kWasmFuncRef), false],
  [wasmRefType(kWasmFuncRef), true],
  [kWasmExternRef, true],
  [wasmRefType(kWasmExternRef), true],
];
for ([type, mutable] of badGlobalTypes) {
  test(() => {
    assert_throws_js(WebAssembly.CompileError,
      () => instantiateImportedGlobal("'", "constant", type, mutable, "'"),
      "type mismatch");
  });
}

const goodGlobalTypes = [
  [kWasmExternRef, false],
  [wasmRefType(kWasmExternRef), false],
];
const constants = [
  '',
  '\0',
  '0',
  '0'.repeat(100000),
  '\uD83D\uDE00',
];
const namespaces = [
  "",
  "'",
  "strings"
];

for (let namespace of namespaces) {
  for (let constant of constants) {
    for ([type, mutable] of goodGlobalTypes) {
      test(() => {
        let result = instantiateImportedGlobal(namespace, constant, type, mutable, namespace);
        assert_equals(result.value, constant);
      });
    }
  }
}
