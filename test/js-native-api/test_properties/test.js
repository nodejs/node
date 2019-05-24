'use strict';
const common = require('../../common');
const assert = require('assert');
const readonlyErrorRE =
  /^TypeError: Cannot assign to read only property '.*' of object '#<Object>'$/;
const getterOnlyErrorRE =
  /^TypeError: Cannot set property .* of #<Object> which has only a getter$/;

// Testing api calls for defining properties
const test_object = require(`./build/${common.buildType}/test_properties`);

assert.strictEqual(test_object.echo('hello'), 'hello');

test_object.readwriteValue = 1;
assert.strictEqual(test_object.readwriteValue, 1);
test_object.readwriteValue = 2;
assert.strictEqual(test_object.readwriteValue, 2);

assert.throws(() => { test_object.readonlyValue = 3; }, readonlyErrorRE);

assert.ok(test_object.hiddenValue);

// Properties with napi_enumerable attribute should be enumerable.
const propertyNames = [];
for (const name in test_object) {
  propertyNames.push(name);
}
assert.ok(propertyNames.includes('echo'));
assert.ok(propertyNames.includes('readwriteValue'));
assert.ok(propertyNames.includes('readonlyValue'));
assert.ok(!propertyNames.includes('hiddenValue'));
assert.ok(propertyNames.includes('NameKeyValue'));
assert.ok(!propertyNames.includes('readwriteAccessor1'));
assert.ok(!propertyNames.includes('readwriteAccessor2'));
assert.ok(!propertyNames.includes('readonlyAccessor1'));
assert.ok(!propertyNames.includes('readonlyAccessor2'));

// Validate property created with symbol
const start = 'Symbol('.length;
const end = start + 'NameKeySymbol'.length;
const symbolDescription =
    String(Object.getOwnPropertySymbols(test_object)[0]).slice(start, end);
assert.strictEqual(symbolDescription, 'NameKeySymbol');

// The napi_writable attribute should be ignored for accessors.
const readwriteAccessor1Descriptor =
  Object.getOwnPropertyDescriptor(test_object, 'readwriteAccessor1');
const readonlyAccessor1Descriptor =
  Object.getOwnPropertyDescriptor(test_object, 'readonlyAccessor1');
assert.ok(readwriteAccessor1Descriptor.get != null);
assert.ok(readwriteAccessor1Descriptor.set != null);
assert.ok(readwriteAccessor1Descriptor.value === undefined);
assert.ok(readonlyAccessor1Descriptor.get != null);
assert.ok(readonlyAccessor1Descriptor.set === undefined);
assert.ok(readonlyAccessor1Descriptor.value === undefined);
test_object.readwriteAccessor1 = 1;
assert.strictEqual(test_object.readwriteAccessor1, 1);
assert.strictEqual(test_object.readonlyAccessor1, 1);
assert.throws(() => { test_object.readonlyAccessor1 = 3; }, getterOnlyErrorRE);
test_object.readwriteAccessor2 = 2;
assert.strictEqual(test_object.readwriteAccessor2, 2);
assert.strictEqual(test_object.readonlyAccessor2, 2);
assert.throws(() => { test_object.readonlyAccessor2 = 3; }, getterOnlyErrorRE);

assert.strictEqual(test_object.hasNamedProperty(test_object, 'echo'), true);
assert.strictEqual(test_object.hasNamedProperty(test_object, 'hiddenValue'),
                   true);
assert.strictEqual(test_object.hasNamedProperty(test_object, 'doesnotexist'),
                   false);
