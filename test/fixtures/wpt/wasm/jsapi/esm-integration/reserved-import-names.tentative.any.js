// Test that wasm: and wasm-js: reserved cases should cause WebAssembly.LinkError

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    WebAssembly.LinkError,
    import("./resources/invalid-import-name.wasm")
  );
}, "wasm: reserved import names should cause WebAssembly.LinkError");

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    WebAssembly.LinkError,
    import("./resources/invalid-import-name-wasm-js.wasm")
  );
}, "wasm-js: reserved import names should cause WebAssembly.LinkError");

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    WebAssembly.LinkError,
    import("./resources/invalid-export-name.wasm")
  );
}, "wasm: reserved export names should cause WebAssembly.LinkError");

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    WebAssembly.LinkError,
    import("./resources/invalid-export-name-wasm-js.wasm")
  );
}, "wasm-js: reserved export names should cause WebAssembly.LinkError");

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    WebAssembly.LinkError,
    import("./resources/invalid-import-module.wasm")
  );
}, "wasm-js: reserved module names should cause WebAssembly.LinkError");
