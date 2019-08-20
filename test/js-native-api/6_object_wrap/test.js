'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/binding`);

const getterOnlyErrorRE =
  /^TypeError: Cannot set property .* of #<.*> which has only a getter$/;

const valueDescriptor = Object.getOwnPropertyDescriptor(
  addon.MyObject.prototype, 'value');
const valueReadonlyDescriptor = Object.getOwnPropertyDescriptor(
  addon.MyObject.prototype, 'valueReadonly');
const plusOneDescriptor = Object.getOwnPropertyDescriptor(
  addon.MyObject.prototype, 'plusOne');
assert.strictEqual(typeof valueDescriptor.get, 'function');
assert.strictEqual(typeof valueDescriptor.set, 'function');
assert.strictEqual(valueDescriptor.value, undefined);
assert.strictEqual(valueDescriptor.enumerable, false);
assert.strictEqual(valueDescriptor.configurable, false);
assert.strictEqual(typeof valueReadonlyDescriptor.get, 'function');
assert.strictEqual(valueReadonlyDescriptor.set, undefined);
assert.strictEqual(valueReadonlyDescriptor.value, undefined);
assert.strictEqual(valueReadonlyDescriptor.enumerable, false);
assert.strictEqual(valueReadonlyDescriptor.configurable, false);

assert.strictEqual(plusOneDescriptor.get, undefined);
assert.strictEqual(plusOneDescriptor.set, undefined);
assert.strictEqual(typeof plusOneDescriptor.value, 'function');
assert.strictEqual(plusOneDescriptor.enumerable, false);
assert.strictEqual(plusOneDescriptor.configurable, false);

const obj = new addon.MyObject(9);
assert.strictEqual(obj.value, 9);
obj.value = 10;
assert.strictEqual(obj.value, 10);
assert.strictEqual(obj.valueReadonly, 10);
assert.throws(() => { obj.valueReadonly = 14; }, getterOnlyErrorRE);
assert.strictEqual(obj.plusOne(), 11);
assert.strictEqual(obj.plusOne(), 12);
assert.strictEqual(obj.plusOne(), 13);

assert.strictEqual(obj.multiply().value, 13);
assert.strictEqual(obj.multiply(10).value, 130);

const newobj = obj.multiply(-1);
assert.strictEqual(newobj.value, -13);
assert.strictEqual(newobj.valueReadonly, -13);
assert.notStrictEqual(obj, newobj);
