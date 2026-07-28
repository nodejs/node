// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const memory = new WebAssembly.Memory({ initial: 0, maximum: 4, shared: true });
  const buffer1 = memory.buffer;

  assert_false(buffer1.growable, "By default the SAB is initially not growable");

  const buffer2 = memory.toFixedLengthBuffer();
  assert_equals(buffer1, buffer2, "Not changing resizability does not make a new object");

  const buffer3 = memory.toResizableBuffer();
  assert_not_equals(buffer2, buffer3, "Changing resizability makes a new object");
  assert_true(buffer3.growable);
  assert_equals(memory.buffer, buffer3);
}, "toFixedLengthBuffer caching behavior");
