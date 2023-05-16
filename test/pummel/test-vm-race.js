'use strict';
require('../common');
const vm = require('vm');

// We're testing a race condition so we just have to spin this in a loop
// for a little while and see if it breaks. The condition being tested
// is an `isolate->TerminateExecution()` reaching the main JS stack from
// the timeout watchdog.
const sandbox = { timeout: 5 };
const context = vm.createContext(sandbox);
const script = new vm.Script(
  'var d = Date.now() + timeout;while (d > Date.now());',
);
const immediate = setImmediate(function() {
  throw new Error('Detected vm race condition!');
});

// When this condition was first discovered this test would fail in 50ms
// or so. A better, but still incorrect implementation would fail after
// 100 seconds or so. If you're messing with vm timeouts you might
// consider increasing this timeout to hammer out races.
const giveUp = Date.now() + 5000;
do {
  // The loop adjusts the timeout up or down trying to hit the race
  try {
    script.runInContext(context, { timeout: 5 });
    ++sandbox.timeout;
  } catch {
    --sandbox.timeout;
  }
} while (Date.now() < giveUp);

clearImmediate(immediate);
