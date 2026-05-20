// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const wasmModule = await import("./resources/js-string-constants.wasm");

  assert_equals(wasmModule.empty, "");
  assert_equals(wasmModule.nullByte, "\0");
  assert_equals(wasmModule.hello, "hello");
  assert_equals(wasmModule.emoji, "\u{1F600}");
}, "String constants from wasm:js/string-constants should be supported in ESM integration");
