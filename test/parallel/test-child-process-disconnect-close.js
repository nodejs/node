'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

if (process.argv[2] === 'child') {
  process.on('disconnect', common.mustCall(() => {
    process.exit(0);
  }));
  process.send('ready');
} else {
  const child = spawn(process.execPath, [__filename, 'child'], {
    stdio: ['ignore', 'pipe', 'pipe', 'ipc'],
  });

  let closed = false;

  child.on('disconnect', common.mustCall(() => {
    assert.strictEqual(child.connected, false);
  }));

  child.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));

  // Regression test for https://github.com/nodejs/node/issues/65646:
  // the 'close' event must be emitted after the parent calls disconnect()
  // even though the channel was closed by the parent instead of reaching EOF.
  child.on('close', common.mustCall((code, signal) => {
    closed = true;
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));

  child.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'ready');
    child.disconnect();
  }));

  process.on('exit', () => {
    assert.strictEqual(closed, true);
  });
}
