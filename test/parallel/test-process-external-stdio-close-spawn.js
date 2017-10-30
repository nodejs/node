'use strict';
// Refs: https://github.com/nodejs/node/issues/947
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  process.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'go');
    // the following console.log is an integral part
    // of the test. If this regress, this call will
    // cause the process to exit with 1
    console.log('logging should not cause a crash');
    process.disconnect();
  }));
} else {
  // Passing '--inspect', '--inspect-brk' to child.spawn enables
  // the debugger. This test was added to help debug the fork-based
  // test with the same name.
  const child = cp.spawn(process.execPath, [__filename, 'child'], {
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
  });

  child.on('close', common.mustCall((exitCode, signal) => {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signal, null);
  }));

  child.stdout.destroy();
  child.send('go');
}
