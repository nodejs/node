'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var fs = require('fs');

if (process.platform === 'win32') {
  console.log('Skipping test, no RLIMIT_NOFILE on Windows.');
  return;
}

for (;;) {
  try {
    fs.openSync(__filename, 'r');
  } catch (err) {
    assert(err.code === 'EMFILE' || err.code === 'ENFILE');
    break;
  }
}

// Should emit an error, not throw.
var proc = spawn(process.execPath, ['-e', '0']);

proc.on('error', common.mustCall(function(err) {
  assert(err.code === 'EMFILE' || err.code === 'ENFILE');
}));

// 'exit' should not be emitted, the process was never spawned.
proc.on('exit', assert.fail);
