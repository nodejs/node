// Flags: --allow-natives-syntax
'use strict';

// Checks the content of the time profiles that node:perf_hooks/pprof
// collects.

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

const intervalMicros = 1000;
const startNanos = BigInt(Date.now()) * 1000000n;
time.start({ intervalMicros });
pprofTestBusyLoop(500_000);
const profile = time.stop();
const endNanos = BigInt(Date.now()) * 1000000n;
assert.strictEqual(time.isStarted(), false);

const { strings, sampleTypes, samples, profile: decoded } =
  decodeGzippedProfile(encodeSync(profile));

assert.deepStrictEqual(sampleTypes, ['sample', 'wall']);
assert.deepStrictEqual(decoded.sampleType.map((t) => strings[t.unit]), [
  'count',
  'nanoseconds',
]);
assert.strictEqual(strings[decoded.periodType.type], 'wall');
assert.strictEqual(strings[decoded.periodType.unit], 'nanoseconds');
// The period is the recomputed sampling interval, which is bounded to
// [interval, 2 * interval].
assert.ok(decoded.period >= intervalMicros * 1000, `${decoded.period}`);
assert.ok(decoded.period <= intervalMicros * 2000, `${decoded.period}`);

// The profile starts within the collection period.
const timeNanos = BigInt(decoded.timeNanos);
const slackNanos = 50n * 1000000n;
assert.ok(timeNanos >= startNanos - slackNanos, `${timeNanos} ${startNanos}`);
assert.ok(timeNanos <= endNanos, `${timeNanos} ${endNanos}`);
assert.ok(decoded.durationNanos > 0);
assert.ok(BigInt(decoded.durationNanos) <= endNanos - startNanos + slackNanos);

assert.ok(samples.length > 0);
for (const { stack, values } of samples) {
  assert.ok(stack.length > 0);
  // `(program)` and `(idle)` samples are removed.
  for (const frame of stack) {
    assert.notStrictEqual(frame.name, '(program)');
    assert.notStrictEqual(frame.name, '(idle)');
    assert.notStrictEqual(frame.name, '(root)');
  }
  assert.ok(values.sample > 0);
  assert.strictEqual(values.wall, values.sample * decoded.period);
}

// The busy loop is in the profile.
const busySamples = samples.filter(({ stack }) =>
  stack.some(({ name }) => name === 'pprofTestBusyLoop'));
assert.ok(busySamples.length > 0);
const busyFrame = busySamples[0].stack.find(
  ({ name }) => name === 'pprofTestBusyLoop');
assert.strictEqual(busyFrame.file, require.resolve('../common/pprof'));
// The share of the samples that the busy loop's own frame gets is not
// checked: V8's sampler attributes some of the samples of a function to its
// callers, more so when the machine is loaded.

// Garbage collection is reported under a Node.js frame.
for (const { stack } of samples) {
  const index = stack.findIndex(({ name }) => name === 'Garbage Collection');
  if (index !== -1) {
    assert.strictEqual(index, stack.length - 2);
    assert.strictEqual(stack.at(-1).name, 'Node.js');
  }
}
