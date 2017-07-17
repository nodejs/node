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

  function testError(err) {
    assert.strictEqual(err.message, 'channel closed');
  }

  child.on('error', common.mustCall(testError));

  {
    const result = child.send('ping');
    assert.strictEqual(result, false);
  }

  {
    const result = child.send('pong', common.mustCall(testError));
    assert.strictEqual(result, false);
  }
}));
