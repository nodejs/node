'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const cbTypeError = /^TypeError: "callback" argument must be a function$/;
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];
const warn = 'Calling an asynchronous function without callback is deprecated.';

function testMakeStatsCallback(cb) {
  return function() {
    // fs.stat() calls makeStatsCallback() on its second argument
    fs.stat(__filename, cb);
  };
}

common.expectWarning('DeprecationWarning', warn);

// Verify the case where a callback function is provided
assert.doesNotThrow(testMakeStatsCallback(common.mustCall()));

// Passing undefined/nothing calls rethrow() internally, which emits a warning
assert.doesNotThrow(testMakeStatsCallback());

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    assert.throws(testMakeStatsCallback(value), cbTypeError);
  });
}

invalidCallbackThrowsTests();
