'use strict';
const common = require('../common');
const fs = require('fs');
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];

function testMakeStatsCallback(cb) {
  return function() {
    // fs.stat() calls makeStatsCallback() on its second argument
    fs.stat(__filename, cb);
  };
}

// Verify the case where a callback function is provided
testMakeStatsCallback(common.mustCall())();

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    common.expectsError(testMakeStatsCallback(value), {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
  });
}

invalidCallbackThrowsTests();
