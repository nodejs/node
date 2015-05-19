'use strict';
const assert = require('assert');
const execFile = require('child_process').execFile;

// test --version
execFile(process.execPath, ['--version'], function(err, stdout, stderr) {
  assert.equal(err, null);
  assert.equal(stderr, '');
  // just in case the version ever has anything appended to it (nightlies?)
  assert.equal(/v([\d]+).([\d]+).([\d]+)(.*)/.test(stdout), true);
});
