'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const spawn = require('child_process').spawn;

// Fails with EINVAL on SmartOS, EBUSY on Windows, EBUSY on AIX.
if (common.isSunOS || common.isWindows || common.isAix) {
  common.skip('cannot rmdir current working directory');
  return;
}

const dirname = common.tmpDir + '/cwd-does-not-exist-' + process.pid;
const abspathFile = require('path').join(common.fixturesDir, 'a.js');
common.refreshTmpDir();
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);


const proc = spawn(process.execPath, ['-r', abspathFile, '-e', '0']);
proc.stdout.pipe(process.stdout);
proc.stderr.pipe(process.stderr);

proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(signalCode, null);
}));
