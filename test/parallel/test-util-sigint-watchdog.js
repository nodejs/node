// Flags: --expose-internals
'use strict';
const common = require('../common');
if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}

const { describe, test } = require('node:test');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const binding = internalBinding('contextify');

describe({ concurrency: false }, () => {
  test('with no signal observed', (_, next) => {
    binding.startSigintWatchdog();
    const hadPendingSignals = binding.stopSigintWatchdog();
    assert.strictEqual(hadPendingSignals, false);
    next();
  });
  test('with one call to the watchdog, one signal', (_, next) => {
    binding.startSigintWatchdog();
    process.kill(process.pid, 'SIGINT');
    waitForPendingSignal(common.mustCall(() => {
      const hadPendingSignals = binding.stopSigintWatchdog();
      assert.strictEqual(hadPendingSignals, true);
      next();
    }));
  });
  test('Nested calls are okay', (_, next) => {
    binding.startSigintWatchdog();
    binding.startSigintWatchdog();
    process.kill(process.pid, 'SIGINT');
    waitForPendingSignal(common.mustCall(() => {
      const hadPendingSignals1 = binding.stopSigintWatchdog();
      const hadPendingSignals2 = binding.stopSigintWatchdog();
      assert.strictEqual(hadPendingSignals1, true);
      assert.strictEqual(hadPendingSignals2, false);
      next();
    }));
  });
  test('Signal comes in after first call to stop', (_, done) => {
    binding.startSigintWatchdog();
    binding.startSigintWatchdog();
    const hadPendingSignals1 = binding.stopSigintWatchdog();
    process.kill(process.pid, 'SIGINT');
    waitForPendingSignal(common.mustCall(() => {
      const hadPendingSignals2 = binding.stopSigintWatchdog();
      assert.strictEqual(hadPendingSignals1, false);
      assert.strictEqual(hadPendingSignals2, true);
      done();
    }));
  });
});

function waitForPendingSignal(cb) {
  if (binding.watchdogHasPendingSigint())
    cb();
  else
    setTimeout(waitForPendingSignal, 10, cb);
}
