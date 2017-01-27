'use strict';
// Flags: --expose_internals
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const internalCp = require('internal/child_process');

if (process.argv[2] === 'child') {
  // Keep the process alive and printing to stdout.
  setInterval(() => { console.log('foo'); }, 1);
} else {
  // Monkey patch ChildProcess#kill() to kill the process and then throw.
  const kill = internalCp.ChildProcess.prototype.kill;

  internalCp.ChildProcess.prototype.kill = function() {
    kill.apply(this, arguments);
    throw new Error('mock error');
  };

  const cmd = `${process.execPath} ${__filename} child`;
  const options = { maxBuffer: 0 };
  const child = cp.exec(cmd, options, common.mustCall((err, stdout, stderr) => {
    // Verify that if ChildProcess#kill() throws, the error is reported.
    assert(/^Error: mock error$/.test(err));
    assert.strictEqual(stdout, '');
    assert.strictEqual(stderr, '');
    assert.strictEqual(child.killed, true);
  }));
}
