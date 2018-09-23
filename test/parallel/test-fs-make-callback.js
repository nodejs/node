'use strict';
const common = require('../common');
const fs = require('fs');
const callbackThrowValues = [null, true, false, 0, 1, 'foo', /foo/, [], {}];

const { sep } = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function testMakeCallback(cb) {
  return function() {
    // fs.mkdtemp() calls makeCallback() on its third argument
    fs.mkdtemp(`${tmpdir.path}${sep}`, {}, cb);
  };
}

function invalidCallbackThrowsTests() {
  callbackThrowValues.forEach((value) => {
    common.expectsError(testMakeCallback(value), {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError
    });
  });
}

invalidCallbackThrowsTests();
