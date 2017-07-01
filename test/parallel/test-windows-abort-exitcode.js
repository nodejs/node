'use strict';
const common = require('../common');
if (!common.isWindows)
  common.skip('test is windows specific');

const assert = require('assert');
const spawn = require('child_process').spawn;

// This test makes sure that an aborted node process
// exits with code 3 on Windows.
// Spawn a child, force an abort, and then check the
// exit code in the parent.

if (process.argv[2] === 'child') {
  process.abort();
} else {
  const child = spawn(process.execPath, [__filename, 'child']);
  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 3);
    assert.strictEqual(signal, null);
  }));
}
