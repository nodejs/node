// META: global=window,worker

const invalidArguments = [
  [() => new Response(undefined, { headers: { "Content-Type": "application/wasm" } }), "no body"],
  [() => new Response("", { headers: { "Content-Type": "application/wasm" } }), "empty body"],
];

for (const method of ["compileStreaming", "instantiateStreaming"]) {
  for (const [argumentFactory, name] of invalidArguments) {
    promise_test(t => {
      const argument = argumentFactory();
      return promise_rejects_js(t, WebAssembly.CompileError, WebAssembly[method](argument));
    }, `${method}: ${name}`);

    promise_test(t => {
      const argument = Promise.resolve(argumentFactory());
      return promise_rejects_js(t, WebAssembly.CompileError, WebAssembly[method](argument));
    }, `${method}: ${name} in a promise`);
  }
}
