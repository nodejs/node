'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// SIGWINCH is a non-terminal signal: sending it must not terminate the target
// process. On Windows, kill() must surface ENOSYS instead of coercing
// SIGWINCH into a SIGKILL (which would wrongly terminate the process).
// Refs: https://github.com/nodejs/node/issues/64324

const child = spawn(process.execPath, ['-e', 'setInterval(() => {}, 1000)']);

// The process must survive the SIGWINCH; it is only ever torn down by the
// explicit SIGKILL in the cleanup below (after the listener is removed).
child.on('exit', common.mustNotCall('child must survive SIGWINCH'));

child.on('spawn', common.mustCall(() => {
  if (common.isWindows) {
    assert.throws(() => child.kill('SIGWINCH'), { code: 'ENOSYS' });
  } else {
    assert.strictEqual(child.kill('SIGWINCH'), true);
  }

  assert.strictEqual(child.signalCode, null);
  assert.strictEqual(child.exitCode, null);

  setTimeout(common.mustCall(() => {
    assert.strictEqual(child.signalCode, null);
    assert.strictEqual(child.exitCode, null);

    child.removeAllListeners('exit');
    child.kill('SIGKILL');
  }), common.platformTimeout(500));
}));
