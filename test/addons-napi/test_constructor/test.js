'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for a constructor that defines properties
const TestConstructor = require(`./build/${common.buildType}/test_constructor`);
const test_object = new TestConstructor();

assert.strictEqual(test_object.echo('hello'), 'hello');

test_object.readwriteValue = 1;
assert.strictEqual(test_object.readwriteValue, 1);
test_object.readwriteValue = 2;
assert.strictEqual(test_object.readwriteValue, 2);

assert.throws(() => { test_object.readonlyValue = 3; });

assert.ok(test_object.hiddenValue);

// All properties except 'hiddenValue' should be enumerable.
const propertyNames = [];
for (const name in test_object) {
  propertyNames.push(name);
}
assert.ok(propertyNames.indexOf('echo') >= 0);
assert.ok(propertyNames.indexOf('readwriteValue') >= 0);
assert.ok(propertyNames.indexOf('readonlyValue') >= 0);
assert.ok(propertyNames.indexOf('hiddenValue') < 0);
