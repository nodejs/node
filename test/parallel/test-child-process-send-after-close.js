'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fixtures = require('../common/fixtures');

const fixture = fixtures.path('empty.js');
const child = cp.fork(fixture);

child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);

  const testError = common.expectsError({
    type: Error,
    message: 'Channel closed',
    code: 'ERR_IPC_CHANNEL_CLOSED'
  }, 2);

  child.on('error', testError);

  {
    const result = child.send('ping');
    assert.strictEqual(result, false);
  }

  {
    const result = child.send('pong', testError);
    assert.strictEqual(result, false);
  }
}));
