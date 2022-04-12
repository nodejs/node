// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=assertions.js

let functions = {};
setup(() => {
  const builder = new WasmModuleBuilder();

  builder
    .addFunction("fn", kSig_v_d)
    .addBody([])
    .exportFunc();
  builder
    .addFunction("fn2", kSig_v_v)
    .addBody([])
    .exportFunc();

  const buffer = builder.toBuffer()
  const module = new WebAssembly.Module(buffer);
  const instance = new WebAssembly.Instance(module, {});
  functions = instance.exports;
});

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_throws_js(TypeError, () => table.get());
}, "Missing arguments: get");

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

  const fn = WebAssembly.Table.prototype.get;

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fn.call(thisValue, argument), `this=${format_value(thisValue)}`);
  }
}, "Branding: get");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_throws_js(TypeError, () => table.set());
}, "Missing arguments: set");

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

  const fn = WebAssembly.Table.prototype.set;

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => fn.call(thisValue, argument, null), `this=${format_value(thisValue)}`);
  }
}, "Branding: set");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null, null, null, null, null]);

  const {fn, fn2} = functions;

  assert_equals(table.set(0, fn), undefined, "set() returns undefined.");
  table.set(2, fn2);
  table.set(4, fn);

  assert_equal_to_array(table, [fn, null, fn2, null, fn]);

  table.set(0, null);
  assert_equal_to_array(table, [null, null, fn2, null, fn]);
}, "Basic");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null, null, null, null, null]);

  const {fn, fn2} = functions;

  table.set(0, fn);
  table.set(2, fn2);
  table.set(4, fn);

  assert_equal_to_array(table, [fn, null, fn2, null, fn]);

  table.grow(4);

  assert_equal_to_array(table, [fn, null, fn2, null, fn, null, null, null, null]);
}, "Growing");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null, null, null, null, null]);

  const {fn} = functions;

  // -1 is the wrong type hence the type check on entry gets this
  // before the range check does.
  assert_throws_js(TypeError, () => table.set(-1, fn));
  assert_throws_js(RangeError, () => table.set(5, fn));
  assert_equal_to_array(table, [null, null, null, null, null]);
}, "Setting out-of-bounds");

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null]);

  const invalidArguments = [
    undefined,
    true,
    false,
    "test",
    Symbol(),
    7,
    NaN,
    {},
  ];
  for (const argument of invalidArguments) {
    assert_throws_js(TypeError, () => table.set(0, argument),
                     `set(${format_value(argument)})`);
  }
  assert_equal_to_array(table, [null]);
}, "Setting non-function");

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null]);

  const fn = function() {};
  assert_throws_js(TypeError, () => table.set(0, fn));
  assert_equal_to_array(table, [null]);
}, "Setting non-wasm function");

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  assert_equal_to_array(table, [null]);

  const fn = () => {};
  assert_throws_js(TypeError, () => table.set(0, fn));
  assert_equal_to_array(table, [null]);
}, "Setting non-wasm arrow function");

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
    assert_throws_js(TypeError, () => table.get(value));
  }, `Getting out-of-range argument: ${format_value(value)}`);

  test(() => {
    const argument = { "element": "anyfunc", "initial": 1 };
    const table = new WebAssembly.Table(argument);
    assert_throws_js(TypeError, () => table.set(value, null));
  }, `Setting out-of-range argument: ${format_value(value)}`);
}

test(() => {
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);
  let called = 0;
  const value = {
    valueOf() {
      called++;
      return 0;
    },
  };
  assert_throws_js(TypeError, () => table.set(value, {}));
  assert_equals(called, 1);
}, "Order of argument conversion");

test(() => {
  const {fn} = functions;
  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument);

  assert_equals(table.get(0, {}), null);
  assert_equals(table.set(0, fn, {}), undefined);
}, "Stray argument");

test(() => {
  const builder = new WasmModuleBuilder();
  builder
    .addFunction("fn", kSig_v_v)
    .addBody([])
    .exportFunc();
  const bin = builder.toBuffer();
  const fn = new WebAssembly.Instance(new WebAssembly.Module(bin)).exports.fn;

  const argument = { "element": "anyfunc", "initial": 1 };
  const table = new WebAssembly.Table(argument, fn);

  assert_equals(table.get(0), fn);
  table.set(0);
  assert_equals(table.get(0), null);

  table.set(0, fn);
  assert_equals(table.get(0), fn);

  assert_throws_js(TypeError, () => table.set(0, {}));
  assert_throws_js(TypeError, () => table.set(0, 37));
}, "Arguments for anyfunc table set");

test(() => {
  const testObject = {};
  const argument = { "element": "externref", "initial": 1 };
  const table = new WebAssembly.Table(argument, testObject);

  assert_equals(table.get(0), testObject);
  table.set(0);
  assert_equals(table.get(0), undefined);

  table.set(0, testObject);
  assert_equals(table.get(0), testObject);

  table.set(0, 37);
  assert_equals(table.get(0), 37);
}, "Arguments for externref table set");
