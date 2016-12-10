'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdin.resume();
  process.stdin._handle.close();
  process.stdin._handle.unref();  // Should not segfault.
  process.stdin.on('error', common.mustCall(function(err) {}));
  return;
}

// Use spawn so that we can be sure that stdin has a _handle property.
// Refs: https://github.com/nodejs/node/pull/5916
const proc = spawn(process.execPath, [__filename, 'child'], { stdio: 'pipe' });

proc.stderr.pipe(process.stderr);
proc.on('exit', common.mustCall(function(exitCode) {
  if (exitCode !== 0)
    process.exitCode = exitCode;
}));
