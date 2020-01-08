'use strict';

const common = require('../common');
const assert = require('assert');

const theErr = new Error('MyError');

process.on(
  process.uncaughtExceptionMonitor,
  common.mustCall((err, origin) => {
    assert.strictEqual(err, theErr);
    assert.strictEqual(origin, 'uncaughtException');
  }, 2)
);

process.on('uncaughtException', common.mustCall((err, origin) => {
  assert.strictEqual(origin, 'uncaughtException');
  assert.strictEqual(err, theErr);
}));

// Test with uncaughtExceptionCaptureCallback installed
process.nextTick(common.mustCall(() => {
  process.setUncaughtExceptionCaptureCallback(common.mustCall(
    (err) => assert.strictEqual(err, theErr))
  );

  throw theErr;
}));

throw theErr;
