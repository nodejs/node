'use strict';
const common = require('../common');
// Fails with EINVAL on SmartOS, EBUSY on Windows, EBUSY on AIX.
if (common.isSunOS || common.isWindows || common.isAIX || common.isIBMi) {
  common.skip('cannot rmdir current working directory');
}

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const assert = require('assert');
const fs = require('fs');
const spawn = require('child_process').spawn;

const tmpdir = require('../common/tmpdir');

const dirname = `${tmpdir.path}/cwd-does-not-exist-${process.pid}`;
tmpdir.refresh();
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);

const proc = spawn(process.execPath, ['--interactive']);
proc.stdout.pipe(process.stdout);
proc.stderr.pipe(process.stderr);
proc.stdin.write('require("path");\n');
proc.stdin.write('process.exit(42);\n');

proc.once('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 42);
  assert.strictEqual(signalCode, null);
}));
