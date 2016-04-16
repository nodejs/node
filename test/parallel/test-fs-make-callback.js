'use strict';
require('../common');
var assert = require('assert');
var fs = require('fs');

function test(cb) {
  return function() {
    // fs.stat() calls makeCallback() on its second argument
    fs.stat(__filename, cb);
  };
}

// Verify the case where a callback function is provided
assert.doesNotThrow(test(function() {}));

// Passing undefined calls rethrow() internally, which is fine
assert.doesNotThrow(test(undefined));

// Anything else should throw
assert.throws(test(null));
assert.throws(test(true));
assert.throws(test(false));
assert.throws(test(1));
assert.throws(test(0));
assert.throws(test('foo'));
assert.throws(test(/foo/));
assert.throws(test([]));
assert.throws(test({}));
