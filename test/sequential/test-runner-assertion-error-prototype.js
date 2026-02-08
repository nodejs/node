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

function createAssertionError() {
  try {
    assert.deepStrictEqual(new ExtendedArray('hello'), ['hello']);
  } catch (error) {
    return error;
  }
  assert.fail('Expected AssertionError');
}

const assertionError = createAssertionError();
const assertionPrototypeMetadata = collectAssertionPrototypeMetadata(assertionError);
assert.ok(assertionPrototypeMetadata);
assert.strictEqual(assertionPrototypeMetadata.actual.constructorName, 'ExtendedArray');

const defaultSerializedError = deserializeError(serializeError(assertionError));
assert.strictEqual(defaultSerializedError.actual.constructor.name, 'Array');

applyAssertionPrototypeMetadata(defaultSerializedError, assertionPrototypeMetadata);
// Must be idempotent when metadata application is triggered more than once.
applyAssertionPrototypeMetadata(defaultSerializedError, assertionPrototypeMetadata);
assert.strictEqual(defaultSerializedError.actual.constructor.name, 'ExtendedArray');
