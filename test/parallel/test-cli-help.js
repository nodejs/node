'use strict';
const assert = require('assert');
const execFile = require('child_process').execFile;

// test --help
execFile(process.execPath, ['--help'], function(err, stdout, stderr) {
  assert.equal(err, null);
  assert.equal(stderr, '');
  assert.equal(/Usage/.test(stdout), true);
});
