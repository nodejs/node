// META: global=window,dedicatedworker,jsshell

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

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Memory.prototype, "buffer");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  assert_equals(typeof desc.set, "undefined");

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => getter.call(thisValue), `this=${format_value(thisValue)}`);
  }
}, "Branding");

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  const buffer = memory.buffer;

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Memory.prototype, "buffer");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  assert_equals(getter.call(memory, {}), buffer);
}, "Stray argument");

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  const memory2 = new WebAssembly.Memory(argument);
  const buffer = memory.buffer;
  assert_not_equals(buffer, memory2.buffer, "Need two distinct buffers");
  memory.buffer = memory2.buffer;
  assert_equals(memory.buffer, buffer, "Should not change the buffer");
}, "Setting (sloppy mode)");

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  const memory2 = new WebAssembly.Memory(argument);
  const buffer = memory.buffer;
  assert_not_equals(buffer, memory2.buffer, "Need two distinct buffers");
  assert_throws_js(TypeError, () => {
    "use strict";
    memory.buffer = memory2.buffer;
  });
  assert_equals(memory.buffer, buffer, "Should not change the buffer");
}, "Setting (strict mode)");
