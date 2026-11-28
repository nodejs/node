// Flags: --expose-internals
'use strict';

// Checks how `time.stop(true)` recovers when it finds V8's CPU profiler
// stuck. A stuck profiler cannot be produced on demand, so the binding reports
// one.

require('../common');
const assert = require('node:assert');
const { internalBinding } = require('internal/test/binding');
const { time } = require('node:perf_hooks/pprof');

const { prototype } = internalBinding('pprof').WallProfiler;
const { start, v8ProfilerStuckEventLoopDetected } = prototype;

/**
 * Calls `time.stop(true)` while the binding reports V8's CPU profiler as
 * stuck, and while `startOverride`, if any, replaces the binding's `start()`.
 * @param {Function} [startOverride] A replacement for the binding's
 *   `start()`, used to make the restart fail.
 * @returns {object} The profile that `time.stop(true)` returns.
 */
function stopAndRestartStuckProfiler(startOverride) {
  prototype.v8ProfilerStuckEventLoopDetected = () => 2;
  if (startOverride !== undefined) {
    prototype.start = startOverride;
  }
  try {
    return time.stop(true);
  } finally {
    prototype.v8ProfilerStuckEventLoopDetected =
      v8ProfilerStuckEventLoopDetected;
    prototype.start = start;
  }
}

// The profiler is restarted with a new V8 CPU profiler.
time.start();
let profile = stopAndRestartStuckProfiler();
assert.strictEqual(typeof profile.encode, 'function');
assert.strictEqual(time.isStarted(), true);
assert.strictEqual(time.v8ProfilerStuckEventLoopDetected(), 2);
time.stop();
assert.strictEqual(time.isStarted(), false);

// When restarting fails, the profile is still returned, and the profiler is
// stopped.
time.start();
profile = stopAndRestartStuckProfiler(() => {
  throw new Error('start failed');
});
assert.strictEqual(typeof profile.encode, 'function');
assert.strictEqual(time.isStarted(), false);
assert.strictEqual(time.v8ProfilerStuckEventLoopDetected(), 0);
assert.throws(() => time.stop(), { code: 'ERR_INVALID_STATE' });

// The profiler can be started again.
time.start();
assert.strictEqual(time.isStarted(), true);
time.stop();
