'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const expectedError = common.isWindows ? /\bENOTSUP\b/ : /\bEPERM\b/;

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {uid: 0});
}, expectedError);

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {gid: 0});
}, expectedError);
