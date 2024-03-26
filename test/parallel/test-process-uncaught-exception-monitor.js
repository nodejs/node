'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { execFile } = require('node:child_process');
const { EventEmitter } = require('node:events');

{
  // Verify exit behavior is unchanged
  const fixture = fixtures.path('uncaught-exceptions', 'uncaught-monitor1.js');
  execFile(
    process.execPath,
    [fixture],
    common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err.code, 1);
      assert.strictEqual(Object.getPrototypeOf(err).name, 'Error');
      assert.strictEqual(stdout, 'Monitored: Shall exit\n');
      const errLines = stderr.trim().split(/[\r\n]+/);
      const errLine = errLines.find((l) => /^Error/.exec(l));
      assert.strictEqual(errLine, 'Error: Shall exit');
    })
  );
}

{
  // Verify exit behavior is unchanged
  const fixture = fixtures.path('uncaught-exceptions', 'uncaught-monitor2.js');
  execFile(
    process.execPath,
    [fixture],
    common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err.code, 7);
      assert.strictEqual(Object.getPrototypeOf(err).name, 'Error');
      assert.strictEqual(stdout, 'Monitored: Shall exit, will throw now\n');
      const errLines = stderr.trim().split(/[\r\n]+/);
      const errLine = errLines.find((l) => /^ReferenceError/.exec(l));
      assert.strictEqual(
        errLine,
        'ReferenceError: missingFunction is not defined'
      );
    })
  );
}

const theErr = new Error('MyError');

// Test the unhandled error events
// Refs: https://github.com/nodejs/node/issues/51202
{
  process.on('uncaughtException', common.mustCall((err, origin) => {
    assert.strictEqual(origin, 'unhandledErrorEvent');
  }));

  const ee = new EventEmitter();
  // Handles generic error events
  ee.emit('error', 'meow');
  // Handles "Error" typed events
  ee.emit('error', theErr);
}

process.on(
  'uncaughtExceptionMonitor',
  common.mustCall((err, origin) => {
    assert.strictEqual(err, theErr);
    assert.strictEqual(origin, 'uncaughtException');
  }, 2)
);

process.on('uncaughtException', common.mustCall((err, origin) => {
  assert.strictEqual(origin, 'uncaughtException');
  assert.strictEqual(err, theErr);

  process.nextTick(common.mustCall(() => {
    // Test with uncaughtExceptionCaptureCallback installed
    process.setUncaughtExceptionCaptureCallback(common.mustCall(
      (err) => assert.strictEqual(err, theErr))
    );

    throw theErr;
  }));
}));

throw theErr;
