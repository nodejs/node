'use strict';
require('../common');
const assert = require('assert');
const { exec } = require('child_process');

Object.defineProperty(Array.prototype, '2', { set: function () {} });

// child_process.exec() used to crash due to missing Array properties from prototype pollution.
// It should now throw a TypeError from C++ ProcessWrap::ParseStdioOptions instead of a fatal error.
assert.throws(
  () => {
    exec('echo 1');
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: /options\.stdio elements must be objects/
  }
);
