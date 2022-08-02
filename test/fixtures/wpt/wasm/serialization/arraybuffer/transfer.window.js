test(() => {
  const buffer = new WebAssembly.Memory({initial: 4}).buffer;
  postMessage(buffer, '*');
}, "Serializing a WebAssembly.Memory-backed ArrayBuffer works");

test(() => {
  const buffer = new WebAssembly.Memory({initial: 4}).buffer;
  assert_throws_js(TypeError, () => {
    postMessage('foo', '*', [buffer]);
  });
}, "Transfering a WebAssembly.Memory-backed ArrayBuffer throws");
