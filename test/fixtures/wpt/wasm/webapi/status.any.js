// META: global=window,worker

const statuses = [
  0,
  300,
  400,
  404,
  500,
  600,
  700,
  999,
];

for (const method of ["compileStreaming", "instantiateStreaming"]) {
  for (const status of statuses) {
    promise_test(t => {
      const response = fetch(`status.py?status=${status}`);
      return promise_rejects_js(t, TypeError, WebAssembly[method](response));
    }, `Response with status ${status}: ${method}`);
  }
}
