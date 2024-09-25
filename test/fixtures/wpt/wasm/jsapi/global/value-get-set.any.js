// META: global=window,dedicatedworker,jsshell,shadowrealm

test(() => {
  const thisValues = [
    undefined,
    null,
    true,
    "",
    Symbol(),
    1,
    {},
    WebAssembly.Global,
    WebAssembly.Global.prototype,
  ];

  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Global.prototype, "value");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  const setter = desc.set;
  assert_equals(typeof setter, "function");

  for (const thisValue of thisValues) {
    assert_throws_js(TypeError, () => getter.call(thisValue), `getter with this=${format_value(thisValue)}`);
    assert_throws_js(TypeError, () => setter.call(thisValue, 1), `setter with this=${format_value(thisValue)}`);
  }
}, "Branding");

for (const type of ["i32", "i64", "f32", "f64"]) {
  const [initial, value, invalid] = type === "i64" ? [0n, 1n, 2] : [0, 1, 2n];
  const immutableOptions = [
    [{}, "missing"],
    [{ "mutable": undefined }, "undefined"],
    [{ "mutable": null }, "null"],
    [{ "mutable": false }, "false"],
    [{ "mutable": "" }, "empty string"],
    [{ "mutable": 0 }, "zero"],
  ];
  for (const [opts, name] of immutableOptions) {
    test(() => {
      opts.value = type;
      const global = new WebAssembly.Global(opts);
      assert_equals(global.value, initial, "initial value");
      assert_equals(global.valueOf(), initial, "initial valueOf");

      assert_throws_js(TypeError, () => global.value = value);

      assert_equals(global.value, initial, "post-set value");
      assert_equals(global.valueOf(), initial, "post-set valueOf");
    }, `Immutable ${type} (${name})`);

    test(t => {
      opts.value = type;
      const global = new WebAssembly.Global(opts);
      assert_equals(global.value, initial, "initial value");
      assert_equals(global.valueOf(), initial, "initial valueOf");

      const value = {
        valueOf: t.unreached_func("should not call valueOf"),
        toString: t.unreached_func("should not call toString"),
      };
      assert_throws_js(TypeError, () => global.value = value);

      assert_equals(global.value, initial, "post-set value");
      assert_equals(global.valueOf(), initial, "post-set valueOf");
    }, `Immutable ${type} with ToNumber side-effects (${name})`);
  }

  const mutableOptions = [
    [{ "mutable": true }, "true"],
    [{ "mutable": 1 }, "one"],
    [{ "mutable": "x" }, "string"],
    [Object.create({ "mutable": true }), "true on prototype"],
  ];
  for (const [opts, name] of mutableOptions) {
    test(() => {
      opts.value = type;
      const global = new WebAssembly.Global(opts);
      assert_equals(global.value, initial, "initial value");
      assert_equals(global.valueOf(), initial, "initial valueOf");

      global.value = value;

      assert_throws_js(TypeError, () => global.value = invalid);

      assert_equals(global.value, value, "post-set value");
      assert_equals(global.valueOf(), value, "post-set valueOf");
    }, `Mutable ${type} (${name})`);
  }
}

test(() => {
  const argument = { "value": "i64", "mutable": true };
  const global = new WebAssembly.Global(argument);

  assert_equals(global.value, 0n, "initial value using ToJSValue");

  const valid = [
    [123n, 123n],
    [2n ** 63n, - (2n ** 63n)],
    [true, 1n],
    [false, 0n],
    ["456", 456n],
  ];
  for (const [input, output] of valid) {
    global.value = input;
    assert_equals(global.valueOf(), output, "post-set valueOf");
  }

  const invalid = [
    undefined,
    null,
    0,
    1,
    4.2,
    Symbol(),
  ];
  for (const input of invalid) {
    assert_throws_js(TypeError, () => global.value = input);
  }
}, "i64 mutability");

test(() => {
  const argument = { "value": "i32", "mutable": true };
  const global = new WebAssembly.Global(argument);
  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Global.prototype, "value");
  assert_equals(typeof desc, "object");

  const setter = desc.set;
  assert_equals(typeof setter, "function");

  assert_throws_js(TypeError, () => setter.call(global));
}, "Calling setter without argument");

test(() => {
  const argument = { "value": "i32", "mutable": true };
  const global = new WebAssembly.Global(argument);
  const desc = Object.getOwnPropertyDescriptor(WebAssembly.Global.prototype, "value");
  assert_equals(typeof desc, "object");

  const getter = desc.get;
  assert_equals(typeof getter, "function");

  const setter = desc.set;
  assert_equals(typeof setter, "function");

  assert_equals(getter.call(global, {}), 0);
  assert_equals(setter.call(global, 1, {}), undefined);
  assert_equals(global.value, 1);
}, "Stray argument");
