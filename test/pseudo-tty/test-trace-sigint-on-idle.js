'use strict';
const { platformTimeout, mustCall } = require('../common');
const childProcess = require('child_process');
const assert = require('assert');

if (process.env.CHILD === 'true') {
  main();
} else {
  // Use inherited stdio child process to prevent test tools from determining
  // the case as crashed from SIGINT
  const cp = childProcess.spawn(
    process.execPath,
    ['--trace-sigint', __filename],
    {
      env: { ...process.env, CHILD: 'true' },
      stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    });
  cp.on('message', mustCall(() => {
    setTimeout(() => cp.kill('SIGINT'), platformTimeout(100));
  }));
  cp.on('exit', mustCall((code, signal) => {
    assert.strictEqual(signal, 'SIGINT');
    assert.strictEqual(code, null);
  }));
}

function main() {
  // Deactivate colors even if the tty does support colors.
  process.env.NODE_DISABLE_COLORS = '1';
  process.channel.ref();  // Keep event loop alive until the signal is received.
  process.send('ready');
}
