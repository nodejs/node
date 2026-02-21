// META: global=window,dedicatedworker,jsshell,shadowrealm

promise_test(async () => {
  globalThis.log = [];

  const { logExec } = await import("./resources/wasm-import-from-wasm.wasm");
  logExec();

  assert_equals(globalThis.log.length, 1, "log should have one entry");
  assert_equals(globalThis.log[0], "executed");

  // Clean up
  delete globalThis.log;
}, "Check import and export between WebAssembly modules");
