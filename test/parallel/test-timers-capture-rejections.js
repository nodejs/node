'use strict';

const common = require('../common');
const assert = require('assert');
const timers = require('timers');

// Capture rejections is false by default
assert(!timers.captureRejections);

[1, '', {}, [], null, undefined, 1n].forEach((i) => {
  assert.throws(() => timers.captureRejections = i, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

common.disableCrashOnUnhandledRejection();

// Unhandled rejection must called only three times.
// If the captureRejection = true below is not working,
// it will have been called 6 times;
process.on('unhandledRejection', common.mustCall(3));

setTimeout(async () => { throw new Error('foo'); },
           common.platformTimeout(10));

const i = setInterval(async () => {
  // Be sure to clear the interval here or test will hang.
  clearInterval(i);
  throw new Error('foo');
}, common.platformTimeout(10));

setImmediate(async () => { throw new Error('foo'); });

timers.captureRejections = true;

setTimeout(async () => { throw new Error('foo'); },
           common.platformTimeout(10));

// The interval will be cleared automatically when it fails.
setInterval(async () => { throw new Error('foo'); },
            common.platformTimeout(10));

setImmediate(async () => { throw new Error('foo'); });

process.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'foo');
}, 3));
