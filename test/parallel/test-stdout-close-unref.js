'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  var errs = 0;

  process.stdin.resume();
  process.stdin._handle.close();
  process.stdin._handle.unref();  // Should not segfault.
  process.stdin.on('error', function(err) {
    errs++;
  });

  process.on('exit', function() {
    assert.strictEqual(errs, 1);
  });
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
