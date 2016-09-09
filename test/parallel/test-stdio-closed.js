'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (common.isWindows) {
  common.skip('platform not supported.');
  return;
}

if (process.argv[2] === 'child') {
  process.stdout.on('error', (err) => {
    process.exit(41);
  });

  process.stderr.on('error', (err) => {
    process.exit(40);
  });

  process.stdout.write('stdout', function() {
    process.stderr.write('stderr', function() {
      process.exit(42);
    });
  });
  return;
}

// Run the script in a shell but close stdout and stderr.
const cmd = `"${process.execPath}" "${__filename}" child 1>&- 2>&-`;
const proc = spawn('/bin/sh', ['-c', cmd], { stdio: 'inherit' });

proc.on('exit', common.mustCall(function(exitCode) {
  assert.strictEqual(exitCode, 42);
}));
