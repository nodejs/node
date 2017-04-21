'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const cbTypeError = common.expectsError({code: 'ERR_INVALID_CALLBACK'});
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];

const { sep } = require('path');

common.refreshTmpDir();

function testMakeCallback(cb) {
  return function() {
    // fs.mkdtemp() calls makeCallback() on its third argument
    fs.mkdtemp(`${common.tmpDir}${sep}`, {}, cb);
  };
}

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    assert.throws(testMakeCallback(value), cbTypeError);
  });
}

invalidCallbackThrowsTests();
