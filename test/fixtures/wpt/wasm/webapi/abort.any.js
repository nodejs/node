const methods = [
  "compileStreaming",
  "instantiateStreaming",
];

for (const method of methods) {
  promise_test(async t => {
    const controller = new AbortController();
    const signal = controller.signal;
    controller.abort();
    const request = fetch('../incrementer.wasm', { signal });
    return promise_rejects_dom(t, 'AbortError', WebAssembly[method](request),
                              `${method} should reject`);
  }, `${method}() on an already-aborted request should reject with AbortError`);

  promise_test(async t => {
    const controller = new AbortController();
    const signal = controller.signal;
    const request = fetch('../incrementer.wasm', { signal });
    const promise = WebAssembly[method](request);
    controller.abort();
    return promise_rejects_dom(t, 'AbortError', promise, `${method} should reject`);
  }, `${method}() synchronously followed by abort should reject with AbortError`);

  promise_test(async t => {
    const controller = new AbortController();
    const signal = controller.signal;
    return fetch('../incrementer.wasm', { signal })
    .then(response => {
      Promise.resolve().then(() => controller.abort());
      return WebAssembly[method](response);
    })
    .catch(err => {
      assert_equals(err.name, "AbortError");
    });
  }, `${method}() asynchronously racing with abort should succeed or reject with AbortError`);
}
