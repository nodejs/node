// META: global=window,worker

for (const method of ["compileStreaming", "instantiateStreaming"]) {
  promise_test(t => {
    const url = "http://{{domains[www]}}:{{ports[http][0]}}/wasm/incrementer.wasm";
    const response = fetch(url, { "mode": "no-cors" });
    return promise_rejects_js(t, TypeError, WebAssembly[method](response));
  }, `Opaque response: ${method}`);

  promise_test(t => {
    const url = "/fetch/api/resources/redirect.py?redirect_status=301&location=/wasm/incrementer.wasm";
    const response = fetch(url, { "mode": "no-cors", "redirect": "manual" });
    return promise_rejects_js(t, TypeError, WebAssembly[method](response));
  }, `Opaque redirect response: ${method}`);
}
