// Flags: --allow-natives-syntax
'use strict';

// Checks that profiles can be collected repeatedly by restarting the
// profiler, with and without the V8 bug workaround.

require('../common');
const assert = require('node:assert');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

// Profiles lack inlined frames of optimized code that was compiled before the
// profiler started: see pprofTestBusyLoop() in test/common/pprof.js.
eval('%NeverOptimizeFunction(pprofTestBusyLoop)');

for (const workaroundV8Bug of [true, false]) {
  time.start({ intervalMicros: 1000, durationMillis: 100, workaroundV8Bug });
  let previousTimeNanos = 0;
  for (let i = 0; i < 5; i++) {
    pprofTestBusyLoop(100_000);
    const profile = time.stop(true);
    assert.strictEqual(time.isStarted(), true);
    const stuck = time.v8ProfilerStuckEventLoopDetected();
    assert.ok(stuck >= 0 && stuck <= 2, `${stuck}`);

    const { samples, profile: decoded } =
      decodeGzippedProfile(encodeSync(profile));
    assert.ok(decoded.timeNanos > previousTimeNanos);
    previousTimeNanos = decoded.timeNanos;
    assert.ok(samples.some(({ stack }) =>
      stack.some(({ name }) => name === 'pprofTestBusyLoop')));
  }
  time.stop();
  assert.strictEqual(time.isStarted(), false);
  assert.strictEqual(time.v8ProfilerStuckEventLoopDetected(), 0);
}
