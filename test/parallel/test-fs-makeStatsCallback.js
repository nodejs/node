'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const cbTypeError = common.expectsError({code: 'ERR_INVALID_CALLBACK'});
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];

function testMakeStatsCallback(cb) {
  return function() {
    // fs.stat() calls makeStatsCallback() on its second argument
    fs.stat(__filename, cb);
  };
}

// Verify the case where a callback function is provided
assert.doesNotThrow(testMakeStatsCallback(common.noop));

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    assert.throws(testMakeStatsCallback(value), cbTypeError);
  });
}

invalidCallbackThrowsTests();
