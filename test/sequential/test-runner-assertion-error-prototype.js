// Flags: --expose-internals
'use strict';

// Regression test for https://github.com/nodejs/node/issues/50397:
// verify test runner assertion metadata restores actual constructor names.

require('../common');
const assert = require('assert');
const { serializeError, deserializeError } = require('internal/error_serdes');
const {
  applyAssertionPrototypeMetadata,
  collectAssertionPrototypeMetadata,
} = require('internal/test_runner/assertion_error_prototype');

class ExtendedArray extends Array {}
class ExtendedObject {
  constructor(value) {
    this.value = value;
  }
}

function createAssertionError(actual, expected) {
  try {
    assert.deepStrictEqual(actual, expected);
  } catch (error) {
    return error;
  }
  assert.fail('Expected AssertionError');
}

const arrayAssertionError = createAssertionError(new ExtendedArray('hello'), ['hello']);
const arrayPrototypeMetadata = collectAssertionPrototypeMetadata(arrayAssertionError);
assert.ok(arrayPrototypeMetadata);
assert.strictEqual(arrayPrototypeMetadata.actual.constructorName, 'ExtendedArray');

const defaultSerializedArrayError = deserializeError(serializeError(arrayAssertionError));
assert.strictEqual(defaultSerializedArrayError.actual.constructor.name, 'Array');

applyAssertionPrototypeMetadata(defaultSerializedArrayError, arrayPrototypeMetadata);
// Must be idempotent when metadata application is triggered more than once.
applyAssertionPrototypeMetadata(defaultSerializedArrayError, arrayPrototypeMetadata);
assert.strictEqual(defaultSerializedArrayError.actual.constructor.name, 'ExtendedArray');

const objectAssertionError = createAssertionError(new ExtendedObject(42), { value: 42 });
const objectPrototypeMetadata = collectAssertionPrototypeMetadata(objectAssertionError);
assert.ok(objectPrototypeMetadata);
assert.strictEqual(objectPrototypeMetadata.actual.constructorName, 'ExtendedObject');

const defaultSerializedObjectError = deserializeError(serializeError(objectAssertionError));
assert.strictEqual(defaultSerializedObjectError.actual.constructor.name, 'Object');

applyAssertionPrototypeMetadata(defaultSerializedObjectError, objectPrototypeMetadata);
assert.strictEqual(defaultSerializedObjectError.actual.constructor.name, 'ExtendedObject');

const expectedArrayAssertionError = createAssertionError(['hello'], new ExtendedArray('hello'));
const expectedArrayPrototypeMetadata = collectAssertionPrototypeMetadata(expectedArrayAssertionError);
assert.ok(expectedArrayPrototypeMetadata);
assert.strictEqual(expectedArrayPrototypeMetadata.actual, undefined);
assert.strictEqual(expectedArrayPrototypeMetadata.expected.constructorName, 'ExtendedArray');

const defaultSerializedExpectedArrayError = deserializeError(serializeError(expectedArrayAssertionError));
assert.strictEqual(defaultSerializedExpectedArrayError.actual.constructor.name, 'Array');
assert.strictEqual(defaultSerializedExpectedArrayError.expected.constructor.name, 'Array');

applyAssertionPrototypeMetadata(defaultSerializedExpectedArrayError, expectedArrayPrototypeMetadata);
assert.strictEqual(defaultSerializedExpectedArrayError.actual.constructor.name, 'Array');
assert.strictEqual(defaultSerializedExpectedArrayError.expected.constructor.name, 'ExtendedArray');

// AssertionError wrapped in ERR_TEST_FAILURE should still be supported.
const wrappedMetadata = collectAssertionPrototypeMetadata({
  code: 'ERR_TEST_FAILURE',
  cause: arrayAssertionError,
});
assert.deepStrictEqual(wrappedMetadata, arrayPrototypeMetadata);

// Unsupported metadata shapes must be ignored safely.
// These calls are expected to be ignored (no throw, no mutation).
applyAssertionPrototypeMetadata(arrayAssertionError, null);
applyAssertionPrototypeMetadata(arrayAssertionError, {
  actual: { baseType: 'map', constructorName: 'ExtendedMap' },
});

// Base type mismatch should not rewrite the value prototype.
const mismatchError = deserializeError(serializeError(objectAssertionError));
applyAssertionPrototypeMetadata(mismatchError, {
  actual: {
    __proto__: null,
    baseType: 'array',
    constructorName: 'ExtendedArray',
  },
});
assert.strictEqual(mismatchError.actual.constructor.name, 'Object');
