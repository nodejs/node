// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-builtins.wasm");

  assert_true(wasmModuleSource instanceof WebAssembly.Module);

  const instance = new WebAssembly.Instance(wasmModuleSource, {});

  assert_equals(instance.exports.getLength("hello"), 5);
  assert_equals(
    instance.exports.concatStrings("hello", " world"),
    "hello world"
  );
  assert_equals(instance.exports.compareStrings("test", "test"), 1);
  assert_equals(instance.exports.compareStrings("test", "different"), 0);
  assert_equals(instance.exports.testString("hello"), 1);
  assert_equals(instance.exports.testString(42), 0);
}, "String builtins should be supported in source phase imports");

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-builtins.wasm");

  const exports = WebAssembly.Module.exports(wasmModuleSource);
  const exportNames = exports.map((exp) => exp.name);

  assert_true(exportNames.includes("getLength"));
  assert_true(exportNames.includes("concatStrings"));
  assert_true(exportNames.includes("compareStrings"));
  assert_true(exportNames.includes("testString"));
}, "Source phase import should properly expose string builtin exports");

promise_test(async () => {
  const wasmModuleSource = await import.source("./resources/js-string-builtins.wasm");

  const imports = WebAssembly.Module.imports(wasmModuleSource);

  assert_equals(imports.length, 0);
}, "Source phase import should handle string builtin import reflection correctly");
