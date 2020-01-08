'use strict';

const common = require('../common');
const assert = require('assert');

const theErr = new Error('MyError');

process.on(
  process.uncaughtExceptionMonitor,
  common.mustCall(function onUncaughtExceptionMonitor(err, origin) {
    assert.strictEqual(err, theErr);
    assert.strictEqual(origin, 'uncaughtException');
  }, 2)
);

process.on('uncaughtException', common.mustCall(
  function onUncaughtException(err, origin) {
    assert.strictEqual(origin, 'uncaughtException');
    assert.strictEqual(err, theErr);
  })
);

process.nextTick(common.mustCall(
  function withExceptionCaptureCallback() {
    process.setUncaughtExceptionCaptureCallback(common.mustCall(
      function uncaughtExceptionCaptureCallback(err) {
        assert.strictEqual(err, theErr);
      })
    );

    throw theErr;
  })
);

throw theErr;
