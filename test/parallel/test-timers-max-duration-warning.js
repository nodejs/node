'use strict';

const common = require('../common');
const assert = require('assert');
const timers = require('timers');

const OVERFLOW = Math.pow(2, 31); // TIMEOUT_MAX is 2^31-1

function timerNotCanceled() {
  assert.fail('Timer should be canceled');
}

process.on('warning', common.mustCall((warning) => {
  const lines = warning.message.split('\n');

  assert.strictEqual(warning.name, 'TimeoutOverflowWarning');
  assert.strictEqual(lines[0], `${OVERFLOW} does not fit into a 32-bit signed` +
                               ' integer.');
  assert.strictEqual(lines.length, 2);
}, 3));


{
  const timeout = setTimeout(timerNotCanceled, OVERFLOW);
  clearTimeout(timeout);
}

{
  const interval = setInterval(timerNotCanceled, OVERFLOW);
  clearInterval(interval);
}

{
  const timer = {
    _onTimeout: timerNotCanceled
  };
  timers.enroll(timer, OVERFLOW);
  timers.active(timer);
  timers.unenroll(timer);
}
