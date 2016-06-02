'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

if (common.isWindows) {
  common.skip('platform not supported.');
  return;
}

if (process.argv[2] === 'child') {
  process.stdout.write('stdout', function() {
    process.stderr.write('stderr', function() {
      process.exit(42);
    });
  });
  return;
}

// Run the script in a shell but close stdout and stderr.
var cmd = `"${process.execPath}" "${__filename}" child 1>&- 2>&-`;
var proc = spawn('/bin/sh', ['-c', cmd], { stdio: 'inherit' });

proc.on('exit', common.mustCall(function(exitCode) {
  assert.equal(exitCode, 42);
}));
