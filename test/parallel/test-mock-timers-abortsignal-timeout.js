'use strict';

// eslint-disable-next-line no-unused-vars
const common = require('../common');
const assert = require('assert');
const { MockTimers } = require('internal/test_runner/mock/mock_timers');
const { AbortSignal } = require('internal/abort_controller');

{
  const mock = new MockTimers();
  mock.enable({ apis: ['AbortSignal.timeout'] });

  try {
    const signal = AbortSignal.timeout(50);

    assert.strictEqual(signal.aborted, false);

    mock.tick(49);
    assert.strictEqual(signal.aborted, false);

    mock.tick(1);
    assert.strictEqual(signal.aborted, true);
  } finally {
    mock.reset();
  }
}
