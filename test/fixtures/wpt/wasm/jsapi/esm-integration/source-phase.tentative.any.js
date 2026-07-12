// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const exportedNamesSource = await import.source("./resources/exports.wasm");

  assert_true(exportedNamesSource instanceof WebAssembly.Module);
  const AbstractModuleSource = Object.getPrototypeOf(WebAssembly.Module);
  assert_equals(AbstractModuleSource.name, "AbstractModuleSource");
  assert_true(exportedNamesSource instanceof AbstractModuleSource);

  assert_array_equals(
    WebAssembly.Module.exports(exportedNamesSource)
      .map(({ name }) => name)
      .sort(),
    [
      "a\u200Bb\u0300c",
      "func",
      "glob",
      "mem",
      "tab",
      "value with spaces",
      "🎯test-func!",
    ]
  );

  const wasmImportFromWasmSource = await import.source(
    "./resources/wasm-import-from-wasm.wasm"
  );

  assert_true(wasmImportFromWasmSource instanceof WebAssembly.Module);

  let logged = false;
  const instance = await WebAssembly.instantiate(wasmImportFromWasmSource, {
    "./wasm-export-to-wasm.wasm": {
      log() {
        logged = true;
      },
    },
  });
  instance.exports.logExec();
  assert_true(logged, "WebAssembly instance should execute imported function");
}, "Source phase imports");

promise_test(async () => {
  const { mod1, mod2, mod3, mod4 } = await import('./resources/source-phase-identity.js');

  assert_equals(mod1, mod2);
  assert_equals(mod3, mod4);
}, "Source phase identities");
