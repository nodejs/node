'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (common.isWindows) {
  common.skip('platform not supported.');
  return;
}

if (process.argv[2] === 'child') {
  try {
    process.stdout.write('stdout', function() {
      try {
        process.stderr.write('stderr', function() {
          process.exit(42);
        });
      } catch (e) {
        process.exit(84);
      }
    });
  } catch (e) {
    assert.strictEqual(e.code, 'EBADF');
    assert.strictEqual(e.message, 'EBADF: bad file descriptor, write');
    process.exit(126);
  }
  return;
}

// Run the script in a shell but close stdout and stderr.
const cmd = `"${process.execPath}" "${__filename}" child 1>&- 2>&-`;
const proc = spawn('/bin/sh', ['-c', cmd], { stdio: 'inherit' });

proc.on('exit', common.mustCall(function(exitCode) {
  assert.strictEqual(exitCode, common.isAix ? 126 : 42);
}));
