// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

test(() => {
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Memory,
    WebAssembly.Memory.prototype,
  ];

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Memory.prototype, "toResizableBuffer");
  assert_equals(typeof desc, "object");

  const fun = desc.value;
  assert_equals(typeof desc.value, "function");

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fun.call(thisValue), `this=${format_value(thisValue)}`);
  }
}, "API surface");

test(() => {
  const memory = new WebAssembly.Memory({ initial: 0, maximum: 1 });
  const buffer1 = memory.buffer;

  assert_false(buffer1.resizable, "By default the AB is initially not resizable");

  const buffer2 = memory.toResizableBuffer();
  assert_true(buffer2.resizable);
  assert_not_equals(buffer1, buffer2, "Changing resizability makes a new object");
  assert_true(buffer1.detached);
  assert_equals(memory.buffer, buffer2, "The buffer created by the most recent toFooBuffer call is cached");

  const buffer3 = memory.toResizableBuffer();
  assert_equals(buffer2, buffer3, "toResizableBuffer does nothing if buffer is already resizable")
  assert_equals(memory.buffer, buffer3);

}, "toResizableBuffer caching behavior");

test(() => {
  {
    const maxNumPages = 4;
    const memory = new WebAssembly.Memory({ initial: 0, maximum: maxNumPages });
    const buffer = memory.toResizableBuffer();
    assert_equals(buffer.maxByteLength, kPageSize * maxNumPages, "Memory maximum is same as maxByteLength");
  }
}, "toResizableBuffer max size");

test(() => {
  const maxNumPages = 4;
  const memory = new WebAssembly.Memory({ initial: 0, maximum: maxNumPages });
  const buffer = memory.toResizableBuffer();

  assert_equals(buffer.byteLength, 0);
  buffer.resize(2 * kPageSize);
  assert_equals(buffer.byteLength, 2 * kPageSize);

  assert_throws_js(RangeError, () => buffer.resize(3 * kPageSize - 1), "Can only grow by page multiples");
  assert_throws_js(RangeError, () => buffer.resize(1 * kPageSize), "Cannot shrink");
}, "Resizing a Memory's resizable buffer");

test(() => {
  const memory = new WebAssembly.Memory({ initial: 0, maximum: 1 });
  const buffer = memory.toResizableBuffer();
  assert_throws_js(TypeError, () => buffer.transfer(), "Cannot be detached by JS");
}, "Resizable buffers from Memory cannot be detached by JS");
