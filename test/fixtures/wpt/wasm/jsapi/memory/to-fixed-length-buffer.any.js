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

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Memory.prototype, "toFixedLengthBuffer");
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

  const buffer2 = memory.toFixedLengthBuffer();
  assert_equals(buffer1, buffer2, "Not changing resizability does not make a new object");

  const buffer3 = memory.toResizableBuffer();
  assert_not_equals(buffer2, buffer3, "Changing resizability makes a new object");
  assert_true(buffer3.resizable);
  assert_true(buffer2.detached);
  assert_equals(memory.buffer, buffer3);
}, "toFixedLengthBuffer caching behavior");
