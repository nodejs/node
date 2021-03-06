'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  // Since maxBuffer is 0, this should trigger an error.
  console.log('foo');
} else {
  const internalCp = require('internal/child_process');

  // Monkey patch ChildProcess#kill() to kill the process and then throw.
  const kill = internalCp.ChildProcess.prototype.kill;

  internalCp.ChildProcess.prototype.kill = function() {
    kill.apply(this, arguments);
    throw new Error('mock error');
  };

  const cmd = `"${process.execPath}" "${__filename}" child`;
  const options = { maxBuffer: 0, killSignal: 'SIGKILL' };

  const child = cp.exec(cmd, options, common.mustCall((err, stdout, stderr) => {
    // Verify that if ChildProcess#kill() throws, the error is reported.
    assert.strictEqual(err.message, 'mock error', err);
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
    assert.strictEqual(child.killed, true);
  }));
}
