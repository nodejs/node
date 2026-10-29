// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/wasm-module-builder.js
// META: script=/wasm/jsapi/assertions.js
// META: script=/wasm/jsapi/table/assertions.js

test(() => {
  assert_function_name(WebAssembly.Table, "Table", "WebAssembly.Table");
}, "name");

test(() => {
  assert_function_length(WebAssembly.Table, 1, "WebAssembly.Table");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Table());
}, "No arguments");

test(() => {
  const argument = { "element": "anyfunc", "initial": 0 };
  assert_throws_js(TypeError, () => WebAssembly.Table(argument));
}, "Calling");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Table({}));
}, "Empty descriptor");

test(() => {
  const invalidArguments = [
    undefined,
    null,
    false,
    true,
    "",
    "test",
    Symbol(),
    1,
    NaN,
    {},
  ];
  for (const invalidArgument of invalidArguments) {
    assert_throws_js(TypeError,
                     () => new WebAssembly.Table(invalidArgument),
                     `new Table(${format_value(invalidArgument)})`);
  }
}, "Invalid descriptor argument");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Table({ "element": "anyfunc", "initial": undefined }));
}, "Undefined initial value in descriptor");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Table({ "element": undefined, "initial": 0 }));
}, "Undefined element value in descriptor");

const outOfRangeValues = [
  NaN,
  Infinity,
  -Infinity,
  -1,
  0x100000000,
  0x1000000000,
];

for (const value of outOfRangeValues) {
  test(() => {
    assert_throws_js(TypeError, () => new WebAssembly.Table({ "element": "anyfunc", "initial": value }));
  }, `Out-of-range initial value in descriptor: ${format_value(value)}`);

  test(() => {
    assert_throws_js(TypeError, () => new WebAssembly.Table({ "element": "anyfunc", "initial": 0, "maximum": value }));
  }, `Out-of-range maximum value in descriptor: ${format_value(value)}`);
}

test(() => {
  assert_throws_js(RangeError, () => new WebAssembly.Table({ "element": "anyfunc", "initial": 10, "maximum": 9 }));
}, "Initial value exceeds maximum");

test(() => {
  const argument = { "element": "anyfunc", "initial": 0 };
  const table = new WebAssembly.Table(argument);
  assert_Table(table, { "length": 0 });
}, "Basic (zero)");

test(() => {
  const argument = { "element": "anyfunc", "initial": 5 };
  const table = new WebAssembly.Table(argument);
  assert_Table(table, { "length": 5 });
}, "Basic (non-zero)");

test(() => {
  const argument = { "element": "anyfunc", "initial": 0 };
  const table = new WebAssembly.Table(argument, null, {});
  assert_Table(table, { "length": 0 });
}, "Stray argument");

test(() => {
  const proxy = new Proxy({}, {
    has(o, x) {
      assert_unreached(`Should not call [[HasProperty]] with ${x}`);
    },
    get(o, x) {
      switch (x) {
      case "element":
        return "anyfunc";
      case "initial":
      case "maximum":
        return 0;
      default:
        return undefined;
      }
    },
  });
  const table = new WebAssembly.Table(proxy);
  assert_Table(table, { "length": 0 });
}, "Proxy descriptor");

test(() => {
  const table = new WebAssembly.Table({
    "element": {
      toString() { return "anyfunc"; },
    },
    "initial": 1,
  });
  assert_Table(table, { "length": 1 });
}, "Type conversion for descriptor.element");

test(() => {
  const order = [];

  new WebAssembly.Table({
    get maximum() {
      order.push("maximum");
      return {
        valueOf() {
          order.push("maximum valueOf");
          return 1;
        },
      };
    },

    get initial() {
      order.push("initial");
      return {
        valueOf() {
          order.push("initial valueOf");
          return 1;
        },
      };
    },

    get element() {
      order.push("element");
      return {
        toString() {
          order.push("element toString");
          return "anyfunc";
        },
      };
    },
  });

  assert_array_equals(order, [
    "element",
    "element toString",
    "initial",
    "initial valueOf",
    "maximum",
    "maximum valueOf",
  ]);
}, "Order of evaluation for descriptor");

test(() => {
  const testObject = {};
  const argument = { "element": "externref", "initial": 3 };
  const table = new WebAssembly.Table(argument, testObject);
  assert_equals(table.length, 3);
  assert_equals(table.get(0), testObject);
  assert_equals(table.get(1), testObject);
  assert_equals(table.get(2), testObject);
}, "initialize externref table with default value");

test(() => {
  const argument = { "element": "i32", "initial": 3 };
  assert_throws_js(TypeError, () => new WebAssembly.Table(argument));
}, "initialize table with a wrong element value");

test(() => {
  const builder = new WasmModuleBuilder();
  builder
    .addFunction("fn", kSig_v_v)
    .addBody([])
    .exportFunc();
  const bin = builder.toBuffer();
  const fn = new WebAssembly.Instance(new WebAssembly.Module(bin)).exports.fn;
  const argument = { "element": "anyfunc", "initial": 3 };
  const table = new WebAssembly.Table(argument, fn);
  assert_equals(table.length, 3);
  assert_equals(table.get(0), fn);
  assert_equals(table.get(1), fn);
  assert_equals(table.get(2), fn);
}, "initialize anyfunc table with default value");

test(() => {
  const argument = { "element": "anyfunc", "initial": 3 };
  assert_throws_js(TypeError, () => new WebAssembly.Table(argument, {}));
  assert_throws_js(TypeError, () => new WebAssembly.Table(argument, "cannot be used as a wasm function"));
  assert_throws_js(TypeError, () => new WebAssembly.Table(argument, 37));
}, "initialize anyfunc table with a bad default value");

test(() => {
  assert_throws_js(RangeError, () =>
    new WebAssembly.Table({ "element": "anyfunc", "initial": 3, "maximum": 2 }, 37));
}, "initialize anyfunc table with a bad default value and a bad descriptor");
