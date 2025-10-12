'use strict';

const { mustCall } = require('../common');
const childProcess = require('child_process');
const assert = require('assert');
const util = require('util');

if (process.env.CHILD === 'true') {
  main();
} else {
  // Use inherited stdio child process to prevent test tools from determining
  // the case as crashed from SIGINT
  const cp = childProcess.spawn(
    process.execPath,
    [__filename],
    {
      env: { ...process.env, CHILD: 'true' },
      stdio: 'inherit',
    });
  cp.on('exit', mustCall((code, signal) => {
    assert.strictEqual(signal, 'SIGINT');
    assert.strictEqual(code, null);
  }));
}

function main() {
  util.setTraceSigInt(true);
  // Deactivate colors even if the tty does support colors.
  process.env.NODE_DISABLE_COLORS = '1';
  process.kill(process.pid, 'SIGINT');
  while (true);
}
