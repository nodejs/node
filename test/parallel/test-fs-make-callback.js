'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

function test(cb) {
  return function() {
    // fs.stat() calls makeCallback() on its second argument
    fs.stat(__filename, cb);
  };
}

// Verify the case where a callback function is provided
assert.doesNotThrow(test(function() {}));

process.once('warning', common.mustCall((warning) => {
  assert.strictEqual(
    warning.message,
    'Calling an asynchronous function without callback is deprecated.'
  );

  invalidArgumentsTests();
}));

// Passing undefined/nothing calls rethrow() internally, which emits a warning
assert.doesNotThrow(test());

function invalidArgumentsTests() {
  assert.throws(test(null));
  assert.throws(test(true));
  assert.throws(test(false));
  assert.throws(test(1));
  assert.throws(test(0));
  assert.throws(test('foo'));
  assert.throws(test(/foo/));
  assert.throws(test([]));
  assert.throws(test({}));
}
