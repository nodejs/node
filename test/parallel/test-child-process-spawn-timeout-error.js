'use strict';

// Verify the timeout timer is cleared when the child fails to spawn, which
// emits 'error' without 'exit'.

const { mustCall } = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

function pendingTimers() {
  return process.getActiveResourcesInfo()
    .filter((type) => type === 'Timeout').length;
}

assert.strictEqual(pendingTimers(), 0);

const cp = spawn('this-command-does-not-exist', {
  timeout: 6000,
});

assert.strictEqual(pendingTimers(), 1);

cp.on('error', mustCall((err) => {
  assert.strictEqual(err.code, 'ENOENT');
}));

cp.on('close', mustCall(() => {
  setImmediate(mustCall(() => {
    assert.strictEqual(pendingTimers(), 0);
  }));
}));
