// META: global=window,worker
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/bad-imports.js

test_bad_imports((name, error, build, ...args) => {
  promise_test(t => {
    const builder = new WasmModuleBuilder();
    build(builder);
    const buffer = builder.toBuffer();
    const response = new Response(buffer, { "headers": { "Content-Type": "application/wasm" } });
    return promise_rejects_js(t, error, WebAssembly.instantiateStreaming(response, ...args));
  }, name);
});
