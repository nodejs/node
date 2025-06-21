// META: global=window,worker
// META: script=/wasm/jsapi/wasm-module-builder.js

["compileStreaming", "instantiateStreaming"].forEach(method => {
  promise_test(async t => {
    const buffer = new WasmModuleBuilder().toBuffer();
    const argument = new Response(buffer, { headers: { "Content-Type": "test/test" } });
    argument.headers.set("Content-Type", "application/wasm");
    // This should resolve successfully
    await WebAssembly[method](argument);
    // Ensure body can only be read once
    return promise_rejects_js(t, TypeError, argument.blob());
  }, `${method} with Content-Type set late`);

  promise_test(async t => {
    const buffer = new WasmModuleBuilder().toBuffer();
    const argument = new Response(buffer, { headers: { "Content-Type": "application/wasm" } });
    argument.headers.delete("Content-Type");
    // Ensure Wasm cannot be created
    await promise_rejects_js(t, TypeError, WebAssembly[method](argument));
    // This should resolve successfully
    await argument.arrayBuffer();
  }, `${method} with Content-Type removed late`);
});
