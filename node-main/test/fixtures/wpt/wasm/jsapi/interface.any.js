// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js

function test_operations(object, object_name, operations) {
  for (const [name, length] of operations) {
    test(() => {
      const propdesc = Object.getOwnPropertyDescriptor(object, name);
      assert_equals(typeof propdesc, "object");
      assert_true(propdesc.writable, "writable");
      assert_true(propdesc.enumerable, "enumerable");
      assert_true(propdesc.configurable, "configurable");
      assert_equals(propdesc.value, object[name]);
    }, `${object_name}.${name}`);

    test(() => {
      assert_function_name(object[name], name, `${object_name}.${name}`);
    }, `${object_name}.${name}: name`);

    test(() => {
      assert_function_length(object[name], length, `${object_name}.${name}`);
    }, `${object_name}.${name}: length`);
  }
}

function test_attributes(object, object_name, attributes) {
  for (const [name, mutable] of attributes) {
    test(() => {
      const propdesc = Object.getOwnPropertyDescriptor(object, name);
      assert_equals(typeof propdesc, "object");
      assert_true(propdesc.enumerable, "enumerable");
      assert_true(propdesc.configurable, "configurable");
    }, `${object_name}.${name}`);

    test(() => {
      const propdesc = Object.getOwnPropertyDescriptor(object, name);
      assert_equals(typeof propdesc, "object");
      assert_equals(typeof propdesc.get, "function");
      assert_function_name(propdesc.get, "get " + name, `getter for "${name}"`);
      assert_function_length(propdesc.get, 0, `getter for "${name}"`);
    }, `${object_name}.${name}: getter`);

    test(() => {
      const propdesc = Object.getOwnPropertyDescriptor(object, name);
      assert_equals(typeof propdesc, "object");
      if (mutable) {
        assert_equals(typeof propdesc.set, "function");
        assert_function_name(propdesc.set, "set " + name, `setter for "${name}"`);
        assert_function_length(propdesc.set, 1, `setter for "${name}"`);
      } else {
        assert_equals(typeof propdesc.set, "undefined");
      }
    }, `${object_name}.${name}: setter`);
  }
}

test(() => {
  const propdesc = Object.getOwnPropertyDescriptor(this, "WebAssembly");
  assert_equals(typeof propdesc, "object");
  assert_true(propdesc.writable, "writable");
  assert_false(propdesc.enumerable, "enumerable");
  assert_true(propdesc.configurable, "configurable");
  assert_equals(propdesc.value, this.WebAssembly);
}, "WebAssembly: property descriptor");

test(() => {
  assert_throws_js(TypeError, () => WebAssembly());
}, "WebAssembly: calling");

test(() => {
  assert_throws_js(TypeError, () => new WebAssembly());
}, "WebAssembly: constructing");

const interfaces = [
  "Module",
  "Instance",
  "Memory",
  "Table",
  "Global",
  "CompileError",
  "LinkError",
  "RuntimeError",
];

for (const name of interfaces) {
  test(() => {
    const propdesc = Object.getOwnPropertyDescriptor(WebAssembly, name);
    assert_equals(typeof propdesc, "object");
    assert_true(propdesc.writable, "writable");
    assert_false(propdesc.enumerable, "enumerable");
    assert_true(propdesc.configurable, "configurable");
    assert_equals(propdesc.value, WebAssembly[name]);
  }, `WebAssembly.${name}: property descriptor`);

  test(() => {
    const interface_object = WebAssembly[name];
    const propdesc = Object.getOwnPropertyDescriptor(interface_object, "prototype");
    assert_equals(typeof propdesc, "object");
    assert_false(propdesc.writable, "writable");
    assert_false(propdesc.enumerable, "enumerable");
    assert_false(propdesc.configurable, "configurable");
  }, `WebAssembly.${name}: prototype`);

  test(() => {
    const interface_object = WebAssembly[name];
    const interface_prototype_object = interface_object.prototype;
    const propdesc = Object.getOwnPropertyDescriptor(interface_prototype_object, "constructor");
    assert_equals(typeof propdesc, "object");
    assert_true(propdesc.writable, "writable");
    assert_false(propdesc.enumerable, "enumerable");
    assert_true(propdesc.configurable, "configurable");
    assert_equals(propdesc.value, interface_object);
  }, `WebAssembly.${name}: prototype.constructor`);
}

test_operations(WebAssembly, "WebAssembly", [
  ["validate", 1],
  ["compile", 1],
  ["instantiate", 1],
]);


test_operations(WebAssembly.Module, "WebAssembly.Module", [
  ["exports", 1],
  ["imports", 1],
  ["customSections", 2],
]);


test_attributes(WebAssembly.Instance.prototype, "WebAssembly.Instance", [
  ["exports", false],
]);


test_operations(WebAssembly.Memory.prototype, "WebAssembly.Memory", [
  ["grow", 1],
]);

test_attributes(WebAssembly.Memory.prototype, "WebAssembly.Memory", [
  ["buffer", false],
]);


test_operations(WebAssembly.Table.prototype, "WebAssembly.Table", [
  ["grow", 1],
  ["get", 1],
  ["set", 1],
]);

test_attributes(WebAssembly.Table.prototype, "WebAssembly.Table", [
  ["length", false],
]);


test_operations(WebAssembly.Global.prototype, "WebAssembly.Global", [
  ["valueOf", 0],
]);

test_attributes(WebAssembly.Global.prototype, "WebAssembly.Global", [
  ["value", true],
]);
