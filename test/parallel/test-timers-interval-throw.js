'use strict';

const common = require('../common');
const assert = require('assert');

// To match browser behaviour, interval should continue
// being rescheduled even if it throws.

let count = 2;
const interval = setInterval(() => { throw new Error('IntervalError'); }, 1);

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'IntervalError');
  if (--count === 0) {
    clearInterval(interval);
  }
}, 2));
