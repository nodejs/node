// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-constants.wasm");

  const instance = new WebAssembly.Instance(wasmModuleSource, {});

  assert_equals(instance.exports.empty.value, "");
  assert_equals(instance.exports.nullByte.value, "\0");
  assert_equals(instance.exports.hello.value, "hello");
  assert_equals(instance.exports.emoji.value, "\u{1F600}");
}, "String constants from wasm:js/string-constants should be supported in source phase imports");

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-constants.wasm");

  const exports = WebAssembly.Module.exports(wasmModuleSource);
  const exportNames = exports.map((exp) => exp.name);

  assert_true(exportNames.includes("empty"));
  assert_true(exportNames.includes("nullByte"));
  assert_true(exportNames.includes("hello"));
  assert_true(exportNames.includes("emoji"));
}, "Source phase import should properly expose string constants exports");

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-constants.wasm");

  const imports = WebAssembly.Module.imports(wasmModuleSource);

  assert_equals(imports.length, 0);
}, "Source phase import should handle string constants import reflection correctly");
