'use strict';

const common = require('../../common');
const test_general = require(`./build/${common.buildType}/test_general`);
const assert = require('assert');

const val1 = '1';
const val2 = 1;
const val3 = 1;

class BaseClass {
}

class ExtendedClass extends BaseClass {
}

const baseObject = new BaseClass();
const extendedObject = new ExtendedClass();

// test napi_strict_equals
assert.ok(test_general.testStrictEquals(val1, val1));
assert.strictEqual(test_general.testStrictEquals(val1, val2), false);
assert.ok(test_general.testStrictEquals(val2, val3));

// test napi_get_prototype
assert.strictEqual(test_general.testGetPrototype(baseObject),
                   Object.getPrototypeOf(baseObject));
assert.strictEqual(test_general.testGetPrototype(extendedObject),
                   Object.getPrototypeOf(extendedObject));
assert.ok(test_general.testGetPrototype(baseObject) !==
          test_general.testGetPrototype(extendedObject),
          'Prototypes for base and extended should be different');
