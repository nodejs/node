'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  setInterval(() => {}, 1000);
} else {
  const { SIGKILL } = process.binding('constants').os.signals;

  function spawn(killSignal) {
    const child = cp.spawnSync(process.execPath,
                               [__filename, 'child'],
                               {killSignal, timeout: 100});

    assert.strictEqual(child.status, null);
    assert.strictEqual(child.error.code, 'ETIMEDOUT');
    return child;
  }

  // Verify that an error is thrown for unknown signals.
  assert.throws(() => {
    spawn('SIG_NOT_A_REAL_SIGNAL');
  }, common.expectsError({ code: 'ERR_UNKNOWN_SIGNAL' }));

  // Verify that the default kill signal is SIGTERM.
  {
    const child = spawn();

    assert.strictEqual(child.signal, 'SIGTERM');
    assert.strictEqual(child.options.killSignal, undefined);
  }

  // Verify that a string signal name is handled properly.
  {
    const child = spawn('SIGKILL');

    assert.strictEqual(child.signal, 'SIGKILL');
    assert.strictEqual(child.options.killSignal, SIGKILL);
  }

  // Verify that a numeric signal is handled properly.
  {
    const child = spawn(SIGKILL);

    assert.strictEqual(typeof SIGKILL, 'number');
    assert.strictEqual(child.signal, 'SIGKILL');
    assert.strictEqual(child.options.killSignal, SIGKILL);
  }
}
