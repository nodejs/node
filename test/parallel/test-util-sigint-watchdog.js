'use strict';
const common = require('../common');
if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}

const assert = require('assert');
const binding = process.binding('util');

[(next) => {
  // Test with no signal observed.
  binding.startSigintWatchdog();
  const hadPendingSignals = binding.stopSigintWatchdog();
  assert.strictEqual(hadPendingSignals, false);
  next();
},
 (next) => {
   // Test with one call to the watchdog, one signal.
   binding.startSigintWatchdog();
   process.kill(process.pid, 'SIGINT');
   waitForPendingSignal(common.mustCall(() => {
     const hadPendingSignals = binding.stopSigintWatchdog();
     assert.strictEqual(hadPendingSignals, true);
     next();
   }));
 },
 (next) => {
   // Nested calls are okay.
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
 },
 () => {
   // Signal comes in after first call to stop.
   binding.startSigintWatchdog();
   binding.startSigintWatchdog();
   const hadPendingSignals1 = binding.stopSigintWatchdog();
   process.kill(process.pid, 'SIGINT');
   waitForPendingSignal(common.mustCall(() => {
     const hadPendingSignals2 = binding.stopSigintWatchdog();
     assert.strictEqual(hadPendingSignals1, false);
     assert.strictEqual(hadPendingSignals2, true);
   }));
 }].reduceRight((a, b) => common.mustCall(b).bind(null, a))();

function waitForPendingSignal(cb) {
  if (binding.watchdogHasPendingSigint())
    cb();
  else
    setTimeout(waitForPendingSignal, 10, cb);
}
