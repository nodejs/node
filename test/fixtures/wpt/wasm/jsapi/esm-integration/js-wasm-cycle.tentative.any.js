// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  const { f } = await import("./resources/js-wasm-cycle.js");

  assert_equals(f(), 24);
}, "Check bindings in JavaScript and WebAssembly cycle (JS higher)");
