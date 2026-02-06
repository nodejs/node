'use strict';

require('../common');
const assert = require('assert');
const { mock } = require('node:test');

{
  mock.timers.enable({ apis: ['AbortSignal.timeout'] });

  try {
    const signal = AbortSignal.timeout(50);

    assert.strictEqual(signal.aborted, false);

    mock.timers.tick(49);
    assert.strictEqual(signal.aborted, false);

    mock.timers.tick(1);
    assert.strictEqual(signal.aborted, true);
  } finally {
    mock.timers.reset();
  }
}
