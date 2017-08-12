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

// test version management functions
// expected version is currently 1
assert.strictEqual(test_general.testGetVersion(), 1);

const [ major, minor, patch, release ] = test_general.testGetNodeVersion();
assert.strictEqual(process.version.split('-')[0],
                   `v${major}.${minor}.${patch}`);
assert.strictEqual(release, process.release.name);

[
  123,
  'test string',
  function() {},
  new Object(),
  true,
  undefined,
  Symbol()
].forEach((val) => {
  assert.strictEqual(test_general.testNapiTypeof(val), typeof val);
});

// since typeof in js return object need to validate specific case
// for null
assert.strictEqual(test_general.testNapiTypeof(null), 'null');

const x = {};

// Assert that wrapping twice fails.
test_general.wrap(x, 25);
assert.throws(function() {
  test_general.wrap(x, 'Blah');
}, Error);
