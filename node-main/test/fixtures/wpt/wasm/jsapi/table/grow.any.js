// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=assertions.js

function nulls(n) {
  return Array(n).fill(null);
}

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_throws_js(TypeError, () => table.grow());
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
    WebAssembly.Table,
    WebAssembly.Table.prototype,
  ];

  const argument = {
    valueOf: t.unreached_func("Should not touch the argument (valueOf)"),
    toString: t.unreached_func("Should not touch the argument (toString)"),
  };

  const fn = WebAssembly.Table.prototype.grow;

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fn.call(thisValue, argument), `this=${format_value(thisValue)}`);
  }
}, "Branding");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, nulls(5), "before");

  const result = table.grow(3);
  assert_equals(result, 5);
  assert_equal_to_array(table, nulls(8), "after");
}, "Basic");

test(() => {
  const argument = { "element": "anyfunc", "initial": 3, "maximum": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, nulls(3), "before");

  const result = table.grow(2);
  assert_equals(result, 3);
  assert_equal_to_array(table, nulls(5), "after");
}, "Reached maximum");

test(() => {
  const argument = { "element": "anyfunc", "initial": 2, "maximum": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, nulls(2), "before");

  assert_throws_js(RangeError, () => table.grow(4));
  assert_equal_to_array(table, nulls(2), "after");
}, "Exceeded maximum");

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
    const argument = { "element": "anyfunc", "initial": 1 };
    const table = new WebAssembly.Table(argument);
    assert_throws_js(TypeError, () => table.grow(value));
  }, `Out-of-range argument: ${format_value(value)}`);
}

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, nulls(5), "before");

  const result = table.grow(3, null, {});
  assert_equals(result, 5);
  assert_equal_to_array(table, nulls(8), "after");
}, "Stray argument");

test(() => {
  const builder = new WasmModuleBuilder();
  builder
    .addFunction("fn", kSig_v_v)
    .addBody([])
    .exportFunc();
  const bin = builder.toBuffer()
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  const fn = new WebAssembly.Instance(new WebAssembly.Module(bin)).exports.fn;
  const result = table.grow(2, fn);
  assert_equals(result, 1);
  assert_equals(table.get(0), null);
  assert_equals(table.get(1), fn);
  assert_equals(table.get(2), fn);
}, "Grow with exported-function argument");

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  assert_throws_js(TypeError, () => table.grow(2, {}));
}, "Grow with non-function argument");

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  assert_throws_js(TypeError, () => table.grow(2, () => true));
}, "Grow with JS-function argument");
