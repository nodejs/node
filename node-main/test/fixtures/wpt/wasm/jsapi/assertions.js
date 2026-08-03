function assert_function_name(fn, name, description) {
  const propdesc = Object.getOwnPropertyDescriptor(fn, "name");
  assert_equals(typeof propdesc, "object", `${description} should have name property`);
  assert_false(propdesc.writable, "writable", `${description} name should not be writable`);
  assert_false(propdesc.enumerable, "enumerable", `${description} name should not be enumerable`);
  assert_true(propdesc.configurable, "configurable", `${description} name should be configurable`);
  assert_equals(propdesc.value, name, `${description} name should be ${name}`);
}

function assert_function_length(fn, length, description) {
  const propdesc = Object.getOwnPropertyDescriptor(fn, "length");
  assert_equals(typeof propdesc, "object", `${description} should have length property`);
  assert_false(propdesc.writable, "writable", `${description} length should not be writable`);
  assert_false(propdesc.enumerable, "enumerable", `${description} length should not be enumerable`);
  assert_true(propdesc.configurable, "configurable", `${description} length should be configurable`);
  assert_equals(propdesc.value, length, `${description} length should be ${length}`);
}

function assert_exported_function(fn, { name, length }, description) {
  if (WebAssembly.Function === undefined) {
    assert_equals(Object.getPrototypeOf(fn), Function.prototype,
                  `${description}: prototype`);
  } else {
    assert_equals(Object.getPrototypeOf(fn), WebAssembly.Function.prototype,
                  `${description}: prototype`);
  }

  assert_function_name(fn, name, description);
  assert_function_length(fn, length, description);
}

function assert_Instance(instance, expected_exports) {
  assert_equals(Object.getPrototypeOf(instance), WebAssembly.Instance.prototype,
                "prototype");
  assert_true(Object.isExtensible(instance), "extensible");

  assert_equals(instance.exports, instance.exports, "exports should be idempotent");
  const exports = instance.exports;

  assert_equals(Object.getPrototypeOf(exports), null, "exports prototype");
  assert_false(Object.isExtensible(exports), "extensible exports");
  assert_array_equals(Object.keys(exports), Object.keys(expected_exports), "matching export keys");
  for (const [key, expected] of Object.entries(expected_exports)) {
    const property = Object.getOwnPropertyDescriptor(exports, key);
    assert_equals(typeof property, "object", `${key} should be present`);
    assert_false(property.writable, `${key}: writable`);
    assert_true(property.enumerable, `${key}: enumerable`);
    assert_false(property.configurable, `${key}: configurable`);
    const actual = property.value;
    assert_true(Object.isExtensible(actual), `${key}: extensible`);

    switch (expected.kind) {
    case "function":
      assert_exported_function(actual, expected, `value of ${key}`);
      break;
    case "global":
      assert_equals(Object.getPrototypeOf(actual), WebAssembly.Global.prototype,
                    `value of ${key}: prototype`);
      assert_equals(actual.value, expected.value, `value of ${key}: value`);
      assert_equals(actual.valueOf(), expected.value, `value of ${key}: valueOf()`);
      break;
    case "memory":
      assert_equals(Object.getPrototypeOf(actual), WebAssembly.Memory.prototype,
                    `value of ${key}: prototype`);
      assert_equals(Object.getPrototypeOf(actual.buffer), ArrayBuffer.prototype,
                    `value of ${key}: prototype of buffer`);
      assert_equals(actual.buffer.byteLength, 0x10000 * expected.size, `value of ${key}: size of buffer`);
      const array = new Uint8Array(actual.buffer);
      assert_equals(array[0], 0, `value of ${key}: first element of buffer`);
      assert_equals(array[array.byteLength - 1], 0, `value of ${key}: last element of buffer`);
      break;
    case "table":
      assert_equals(Object.getPrototypeOf(actual), WebAssembly.Table.prototype,
                    `value of ${key}: prototype`);
      assert_equals(actual.length, expected.length, `value of ${key}: length of table`);
      break;
    }
  }
}

function assert_WebAssemblyInstantiatedSource(actual, expected_exports={}) {
  assert_equals(Object.getPrototypeOf(actual), Object.prototype,
                "Prototype");
  assert_true(Object.isExtensible(actual), "Extensibility");

  const module = Object.getOwnPropertyDescriptor(actual, "module");
  assert_equals(typeof module, "object", "module: type of descriptor");
  assert_true(module.writable, "module: writable");
  assert_true(module.enumerable, "module: enumerable");
  assert_true(module.configurable, "module: configurable");
  assert_equals(Object.getPrototypeOf(module.value), WebAssembly.Module.prototype,
                "module: prototype");

  const instance = Object.getOwnPropertyDescriptor(actual, "instance");
  assert_equals(typeof instance, "object", "instance: type of descriptor");
  assert_true(instance.writable, "instance: writable");
  assert_true(instance.enumerable, "instance: enumerable");
  assert_true(instance.configurable, "instance: configurable");
  assert_Instance(instance.value, expected_exports);
}
