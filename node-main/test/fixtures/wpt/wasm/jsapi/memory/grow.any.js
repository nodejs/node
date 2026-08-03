// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/memory/assertions.js

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  assert_throws_js(TypeError, () => memory.grow());
}, "Missing arguments");

test(t => {
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

  const argument = {
    valueOf: t.unreached_func("Should not touch the argument (valueOf)"),
    toString: t.unreached_func("Should not touch the argument (toString)"),
  };

  const fn = WebAssembly.Memory.prototype.grow;

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fn.call(thisValue, argument), `this=${format_value(thisValue)}`);
  }
}, "Branding");

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 0 }, "Buffer before growing");

  const result = memory.grow(2);
  assert_equals(result, 0);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 2 }, "New buffer after growing");
}, "Zero initial");

test(() => {
  const argument = { "initial": { valueOf() { return 0 } } };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 0 }, "Buffer before growing");

  const result = memory.grow({ valueOf() { return 2 } });
  assert_equals(result, 0);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 2 }, "New buffer after growing");
}, "Zero initial with valueOf");

test(() => {
  const argument = { "initial": 3 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 3 }, "Buffer before growing");

  const result = memory.grow(2);
  assert_equals(result, 3);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 5 }, "New buffer after growing");
}, "Non-zero initial");

test(() => {
  const argument = { "initial": 0, "maximum": 2 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 0 }, "Buffer before growing");

  const result = memory.grow(2);
  assert_equals(result, 0);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 2 }, "New buffer after growing");
}, "Zero initial with respected maximum");

test(() => {
  const argument = { "initial": 0, "maximum": 2 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 0 }, "Buffer before growing");

  const result = memory.grow(1);
  assert_equals(result, 0);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing once");
  assert_ArrayBuffer(newMemory, { "size": 1 }, "New buffer after growing once");

  const result2 = memory.grow(1);
  assert_equals(result2, 1);

  const newestMemory = memory.buffer;
  assert_not_equals(newMemory, newestMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "New buffer after growing twice");
  assert_ArrayBuffer(newMemory, { "detached": true }, "New buffer after growing twice");
  assert_ArrayBuffer(newestMemory, { "size": 2 }, "Newest buffer after growing twice");
}, "Zero initial with respected maximum grown twice");

test(() => {
  const argument = { "initial": 1, "maximum": 2 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 1 }, "Buffer before growing");

  assert_throws_js(RangeError, () => memory.grow(2));
  assert_equals(memory.buffer, oldMemory);
  assert_ArrayBuffer(memory.buffer, { "size": 1 }, "Buffer before trying to grow");
}, "Zero initial growing too much");

const outOfRangeValues = [
  undefined,
  NaN,
  Infinity,
  -Infinity,
  -1,
  0x100000000,
  0x1000000000,
  "0x100000000",
  { valueOf() { return 0x100000000; } },
];

for (const value of outOfRangeValues) {
  test(() => {
    const argument = { "initial": 0 };
    const memory = new WebAssembly.Memory(argument);
    assert_throws_js(TypeError, () => memory.grow(value));
  }, `Out-of-range argument: ${format_value(value)}`);
}

test(() => {
  const argument = { "initial": 0 };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 0 }, "Buffer before growing");

  const result = memory.grow(2, {});
  assert_equals(result, 0);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "detached": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 2 }, "New buffer after growing");
}, "Stray argument");

test(() => {
  const argument = { "initial": 1, "maximum": 2, "shared": true };
  const memory = new WebAssembly.Memory(argument);
  const oldMemory = memory.buffer;
  assert_ArrayBuffer(oldMemory, { "size": 1, "shared": true }, "Buffer before growing");

  const result = memory.grow(1);
  assert_equals(result, 1);

  const newMemory = memory.buffer;
  assert_not_equals(oldMemory, newMemory);
  assert_ArrayBuffer(oldMemory, { "size": 1, "shared": true }, "Old buffer after growing");
  assert_ArrayBuffer(newMemory, { "size": 2, "shared": true }, "New buffer after growing");

  // The old and new buffers must have the same value for the
  // [[ArrayBufferData]] internal slot.
  const oldArray = new Uint8Array(oldMemory);
  const newArray = new Uint8Array(newMemory);
  assert_equals(oldArray[0], 0, "old first element");
  assert_equals(newArray[0], 0, "new first element");
  oldArray[0] = 1;
  assert_equals(oldArray[0], 1, "old first element");
  assert_equals(newArray[0], 1, "new first element");

}, "Growing shared memory does not detach old buffer");
