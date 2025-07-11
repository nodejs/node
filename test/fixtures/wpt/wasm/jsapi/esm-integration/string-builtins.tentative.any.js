// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmModule = await import("./resources/js-string-builtins.wasm");

  assert_equals(wasmModule.getLength("hello"), 5);
  assert_equals(wasmModule.concatStrings("hello", " world"), "hello world");
  assert_equals(wasmModule.compareStrings("test", "test"), 1);
  assert_equals(wasmModule.compareStrings("test", "different"), 0);
  assert_equals(wasmModule.testString("hello"), 1);
  assert_equals(wasmModule.testString(42), 0);
}, "String builtins should be supported in imports in ESM integration");
