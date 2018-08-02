'use strict';

const common = require('../common');
const assert = require('assert');
const timers = require('timers');

const OVERFLOW = Math.pow(2, 31); // TIMEOUT_MAX is 2^31-1
const TEST_MATRIX = [OVERFLOW, BigInt(OVERFLOW)];

function timerNotCanceled() {
  assert.fail('Timer should be canceled');
}

process.on('warning', common.mustCall((warning) => {
  if (warning.name === 'DeprecationWarning') return;

  const lines = warning.message.split('\n');

  assert.strictEqual(warning.name, 'TimeoutOverflowWarning');
  assert.strictEqual(lines[0], `${OVERFLOW} does not fit into a 32-bit signed` +
                               ' integer.');
  assert.strictEqual(lines.length, 2);
}, 8));

for (let i = 0; i < TEST_MATRIX.length; i++) {
  {
    const timeout = setTimeout(timerNotCanceled, TEST_MATRIX[i]);
    clearTimeout(timeout);
  }

  {
    const interval = setInterval(timerNotCanceled, TEST_MATRIX[i]);
    clearInterval(interval);
  }

  {
    const timer = {
      _onTimeout: timerNotCanceled
    };
    timers.enroll(timer, TEST_MATRIX[i]);
    timers.active(timer);
    timers.unenroll(timer);
  }
}
