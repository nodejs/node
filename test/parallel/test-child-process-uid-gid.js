'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (!common.isWindows && process.getuid() === 0) {
  common.skip('as this test should not be run as `root`');
  return;
}

const expectedError = common.isWindows ? /\bENOTSUP\b/ : /\bEPERM\b/;

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {uid: 0});
}, expectedError);

assert.throws(() => {
  spawn('echo', ['fhqwhgads'], {gid: 0});
}, expectedError);
