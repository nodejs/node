'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var spawn = require('child_process').spawn;

// Fails with EINVAL on SmartOS, EBUSY on Windows.
if (process.platform === 'sunos' || process.platform === 'win32') {
  console.log('1..0 # Skipped: cannot rmdir current working directory');
  return;
}

var dirname = common.tmpDir + '/cwd-does-not-exist-' + process.pid;
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);

var proc = spawn(process.execPath, ['-e', '0']);
proc.stdout.pipe(process.stdout);
proc.stderr.pipe(process.stderr);

proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.equal(exitCode, 0);
  assert.equal(signalCode, null);
}));
