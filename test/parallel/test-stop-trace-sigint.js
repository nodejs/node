'use strict';

const { mustCall } = require('../common');
const childProcess = require('child_process');
const util = require('util');
const assert = require('assert');

if (!process.env.CHILD) {
  {
    const cp = childProcess.spawn(
      process.execPath,
      ['--trace-sigint', __filename],
      {
        env: { ...process.env, CHILD: '1' },
      });
    let data;
    cp.stderr.on('data', (chunk) => {
      data = data ? data + chunk : chunk;
    });
    cp.stderr.on('end', () => {
      assert.strictEqual(data === undefined, true);
    });
    cp.on('exit', mustCall((code, signal) => {
      assert.strictEqual(signal, 'SIGINT');
      assert.strictEqual(code, null);
    }));
  }
} else {
  util.setTraceSigInt(false);
  // Deactivate colors even if the tty does support colors.
  process.env.NODE_DISABLE_COLORS = '1';
  process.kill(process.pid, 'SIGINT');
  while (true);
}
