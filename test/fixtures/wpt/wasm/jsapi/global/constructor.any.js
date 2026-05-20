// META: global=window,dedicatedworker,jsshell,shadowrealm
// META: script=/wasm/jsapi/assertions.js

function assert_Global(actual, expected) {
  assert_equals(Object.getPrototypeOf(actual), WebAssembly.Global.prototype,
                "prototype");
  assert_true(Object.isExtensible(actual), "extensible");

  assert_equals(actual.value, expected, "value");
  assert_equals(actual.valueOf(), expected, "valueOf");
}

test(() => {
  assert_function_name(WebAssembly.Global, "Global", "WebAssembly.Global");
}, "name");

test(() => {
  assert_function_length(WebAssembly.Global, 1, "WebAssembly.Global");
}, "length");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly.Global());
}, "No arguments");

test(() => {
  const argument = { "value": "i32" };
  assert_throws_js(TypeError, () => WebAssembly.Global(argument));
}, "Calling");

test(() => {
  const order = [];

  new WebAssembly.Global({
    get value() {
      order.push("descriptor value");
      return {
        toString() {
          order.push("descriptor value toString");
          return "f64";
        },
      };
    },

    get mutable() {
      order.push("descriptor mutable");
      return false;
    },
  }, {
    valueOf() {
      order.push("value valueOf()");
    }
  });

  assert_array_equals(order, [
    "descriptor mutable",
    "descriptor value",
    "descriptor value toString",
    "value valueOf()",
  ]);
}, "Order of evaluation");

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
                     () => new WebAssembly.Global(invalidArgument),
                     `new Global(${format_value(invalidArgument)})`);
  }
}, "Invalid descriptor argument");

test(() => {
  const invalidTypes = ["i16", "i128", "f16", "f128", "u32", "u64", "i32\0"];
  for (const value of invalidTypes) {
    const argument = { value };
    assert_throws_js(TypeError, () => new WebAssembly.Global(argument));
  }
}, "Invalid type argument");

test(() => {
  const argument = { "value": "v128" };
  assert_throws_js(TypeError, () => new WebAssembly.Global(argument));
}, "Construct v128 global");

test(() => {
  const argument = { "value": "i64" };
  const global = new WebAssembly.Global(argument);
  assert_Global(global, 0n);
}, "i64 with default");

for (const type of ["i32", "f32", "f64"]) {
  test(() => {
    const argument = { "value": type };
    const global = new WebAssembly.Global(argument);
    assert_Global(global, 0);
  }, `Default value for type ${type}`);

  const valueArguments = [
    [undefined, 0],
    [null, 0],
    [true, 1],
    [false, 0],
    [2, 2],
    ["3", 3],
    [{ toString() { return "5" } }, 5, "object with toString returning string"],
    [{ valueOf() { return "8" } }, 8, "object with valueOf returning string"],
    [{ toString() { return 6 } }, 6, "object with toString returning number"],
    [{ valueOf() { return 9 } }, 9, "object with valueOf returning number"],
  ];
  for (const [value, expected, name = format_value(value)] of valueArguments) {
    test(() => {
      const argument = { "value": type };
      const global = new WebAssembly.Global(argument, value);
      assert_Global(global, expected);
    }, `Explicit value ${name} for type ${type}`);
  }

  test(() => {
    const argument = { "value": type };
    assert_throws_js(TypeError, () => new WebAssembly.Global(argument, 0n));
  }, `BigInt value for type ${type}`);
}

const valueArguments = [
  [undefined, 0n],
  [true, 1n],
  [false, 0n],
  ["3", 3n],
  [123n, 123n],
  [{ toString() { return "5" } }, 5n, "object with toString returning string"],
  [{ valueOf() { return "8" } }, 8n, "object with valueOf returning string"],
  [{ toString() { return 6n } }, 6n, "object with toString returning bigint"],
  [{ valueOf() { return 9n } }, 9n, "object with valueOf returning bigint"],
];
for (const [value, expected, name = format_value(value)] of valueArguments) {
  test(() => {
    const argument = { "value": "i64" };
    const global = new WebAssembly.Global(argument, value);
    assert_Global(global, expected);
  }, `Explicit value ${name} for type i64`);
}

const invalidBigints = [
  null,
  666,
  { toString() { return 5 } },
  { valueOf() { return 8 } },
  Symbol(),
];
for (const invalidBigint of invalidBigints) {
  test(() => {
    var argument = { "value": "i64" };
    assert_throws_js(TypeError, () => new WebAssembly.Global(argument, invalidBigint));
  }, `Pass non-bigint as i64 Global value: ${format_value(invalidBigint)}`);
}

test(() => {
  const argument = { "value": "i32" };
  const global = new WebAssembly.Global(argument, 0, {});
  assert_Global(global, 0);
}, "Stray argument");
