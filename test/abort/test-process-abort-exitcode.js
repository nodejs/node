'use strict';
const common = require('../common');
const assert = require('assert');

// This test makes sure that an aborted node process
// exits with code 3 on Windows, and SIGABRT on POSIX.
// Spawn a child, force an abort, and then check the
// exit code in the parent.

const spawn = require('child_process').spawn;
if (process.argv[2] === 'child') {
  process.abort();
} else {
  const child = spawn(process.execPath, [__filename, 'child']);
  child.on('exit', common.mustCall((code, signal) => {
    if (common.isWindows) {
      assert.strictEqual(code, 134);
      assert.strictEqual(signal, null);
    } else {
      assert.strictEqual(code, null);
      assert.strictEqual(signal, 'SIGABRT');
    }
  }));
}
