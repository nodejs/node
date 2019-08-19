'use strict';

const { mustCall } = require('../common');
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
      stdio: 'inherit'
    });
  setTimeout(() => cp.kill('SIGINT'), 1 * 1000);
  cp.on('exit', mustCall((code, signal) => {
    assert.strictEqual(signal, 'SIGINT');
    assert.strictEqual(code, null);
  }));
}

function main() {
  // Deactivate colors even if the tty does support colors.
  process.env.NODE_DISABLE_COLORS = '1';
  setTimeout(() => {}, 10 * 1000);
}
