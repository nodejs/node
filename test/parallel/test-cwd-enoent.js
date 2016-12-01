'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var spawn = require('child_process').spawn;

// Fails with EINVAL on SmartOS, EBUSY on Windows, EBUSY on AIX.
if (common.isSunOS || common.isWindows || common.isAix) {
  common.skip('cannot rmdir current working directory');
  return;
}

var dirname = common.tmpDir + '/cwd-does-not-exist-' + process.pid;
common.refreshTmpDir();
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);

var proc = spawn(process.execPath, ['-e', '0']);
proc.stdout.pipe(process.stdout);
proc.stderr.pipe(process.stderr);

proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(signalCode, null);
}));
