'use strict';
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const expectedError = process.platform === 'win32' ? /\bENOTSUP\b/ : /\bEPERM\b/;

if (process.platform === 'win32' || process.getuid() !== 0) {
  assert.throws(() => {
    spawn('echo', ['fhqwhgads'], { uid: 0 });
  }, expectedError);
}

if (process.platform === 'win32' ||
    !process.getgroups().some((gid) => gid === 0)) {
  assert.throws(() => {
    spawn('echo', ['fhqwhgads'], { gid: 0 });
  }, expectedError);
}
