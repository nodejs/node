// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const memory = new WebAssembly.Memory({ initial: 0, maximum: 4, shared: true });
  const buffer1 = memory.buffer;

  assert_false(buffer1.growable, "By default the SAB is initially not growable");

  const buffer2 = memory.toResizableBuffer();
  assert_true(buffer2.growable);
  assert_not_equals(buffer1, buffer2, "Changing resizability makes a new object");
  assert_equals(memory.buffer, buffer2, "The buffer created by the most recent toFooBuffer call is cached");

  const buffer3 = memory.toResizableBuffer();
  assert_equals(buffer2, buffer3, "toResizableBuffer does nothing if buffer is already resizable")
  assert_equals(memory.buffer, buffer3);
}, "toResizableBuffer caching behavior");

test(() => {
  const maxNumPages = 4;
  const memory = new WebAssembly.Memory({ initial: 0, maximum: maxNumPages, shared: true });
  const buffer = memory.toResizableBuffer();
  assert_equals(buffer.maxByteLength, kPageSize * maxNumPages, "Memory maximum is same as maxByteLength");
}, "toResizableBuffer max size");

test(() => {
  const memory = new WebAssembly.Memory({ initial: 0, maximum: 4, shared: true });
  const buffer = memory.toResizableBuffer();

  assert_equals(buffer.byteLength, 0);
  buffer.grow(2 * kPageSize);
  assert_equals(buffer.byteLength, 2 * kPageSize);

  assert_throws_js(RangeError, () => buffer.grow(3 * kPageSize - 1), "Can only grow by page multiples");
}, "Resizing a Memory's resizable buffer");
