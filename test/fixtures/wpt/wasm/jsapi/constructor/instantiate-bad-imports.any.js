// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/bad-imports.js

test_bad_imports((name, error, build, ...arguments) => {
  promise_test(t => {
    const builder = new WasmModuleBuilder();
    build(builder);
    const buffer = builder.toBuffer();
    const module = new WebAssembly.Module(buffer);
    return promise_rejects_js(t, error, WebAssembly.instantiate(module, ...arguments));
  }, `WebAssembly.instantiate(module): ${name}`);
});

test_bad_imports((name, error, build, ...arguments) => {
  promise_test(t => {
    const builder = new WasmModuleBuilder();
    build(builder);
    const buffer = builder.toBuffer();
    return promise_rejects_js(t, error, WebAssembly.instantiate(buffer, ...arguments));
  }, `WebAssembly.instantiate(buffer): ${name}`);
});
