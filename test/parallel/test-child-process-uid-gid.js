'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const expectedError = common.isWindows ? /\bENOTSUP\b/ : /\bEPERM\b/;

if (common.isWindows || process.getuid() !== 0) {
  assert.throws(() => {
    spawn('echo', ['fhqwhgads'], { uid: 0 });
  }, expectedError);
}

if (common.isWindows || !process.getgroups().some((gid) => gid === 0)) {
  assert.throws(() => {
    spawn('echo', ['fhqwhgads'], { gid: 0 });
  }, expectedError);
}
