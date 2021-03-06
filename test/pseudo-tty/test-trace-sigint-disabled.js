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
  cp.on('exit', mustCall((code, signal) => {
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 0);
  }));
}

function main() {
  // Deactivate colors even if the tty does support colors.
  process.env.NODE_DISABLE_COLORS = '1';

  const noop = mustCall(() => {
    process.exit(0);
  });
  process.on('SIGINT', noop);
  // Try testing re-add 'SIGINT' listeners
  process.removeListener('SIGINT', noop);
  process.on('SIGINT', noop);

  process.kill(process.pid, 'SIGINT');
  setTimeout(() => { assert.fail('unreachable path'); }, 10 * 1000);
}
