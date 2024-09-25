// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  let builder = new WasmModuleBuilder();

  // Import a string constant
  builder.addImportedGlobal("constants", "constant", kWasmExternRef, false);

  // Import a builtin function
  builder.addImport(
    "wasm:js-string",
    "test",
    {params: [kWasmExternRef], results: [kWasmI32]});

  let buffer = builder.toBuffer();
  let module = new WebAssembly.Module(buffer, {
    builtins: ["js-string"],
    importedStringConstants: "constants"
  });
  let imports = WebAssembly.Module.imports(module);

  // All imports that refer to a builtin module are suppressed from import
  // reflection.
  assert_equals(imports.length, 0);
});
