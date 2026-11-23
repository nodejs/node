// Flags: --allow-natives-syntax
'use strict';

// Checks the CPU time that the time profiler attributes to samples.

const common = require('../common');
if (common.isWindows) {
  common.skip('CPU time collection relies on SIGPROF');
}

const assert = require('node:assert');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

// The CPU time that each profile spends in `pprofTestBusyLoop`.
const loopCpuMicros = 300_000;

// Profiles lack inlined frames of optimized code that was compiled before the
// profiler started: see pprofTestBusyLoop() in test/common/pprof.js.
eval('%NeverOptimizeFunction(pprofTestBusyLoop)');

/**
 * Returns the CPU time that the current thread used since `previous`.
 * @param {{ user: number, system: number }} previous A value returned by
 *   `process.threadCpuUsage()`.
 * @returns {number} The CPU time, in nanoseconds.
 */
function threadCpuNanosSince(previous) {
  const { user, system } = process.threadCpuUsage(previous);
  return (user + system) * 1000;
}

/**
 * Asserts that a profile reports CPU time, and that the profiled thread and
 * the threads that do not run JavaScript account for it as expected.
 * @param {object} profile A profile returned by `time.stop()`.
 * @param {number} maxThreadCpuNanos The CPU time that the thread used while
 *   the profile was collected, and more: the profile cannot attribute more.
 * @throws {AssertionError} If the profile does not report CPU time as
 *   expected.
 */
function check(profile, maxThreadCpuNanos) {
  const { sampleTypes, samples } = decodeGzippedProfile(encodeSync(profile));
  assert.deepStrictEqual(sampleTypes, [
    'sample',
    'wall',
    'cpu',
  ]);
  // The CPU time of the profiled thread is attributed to its samples. Which
  // frame of a stack a sample is attributed to depends on V8's sampler,
  // which attributes some of the samples of a function to its caller, more
  // so when the machine is loaded, so the CPU time of the thread is checked
  // as a whole, and the loop's frame only has to get some of it.
  let threadCpu = 0;
  let loopCpu = 0;
  for (const { stack, values } of samples) {
    assert.ok(values.cpu >= 0);
    if (stack[0].name === 'Non JS threads activity') {
      assert.strictEqual(stack.length, 2);
      assert.strictEqual(stack[1].name, 'Node.js');
      assert.strictEqual(values.sample, 0);
      assert.strictEqual(values.wall, 0);
      assert.ok(values.cpu > 0);
      continue;
    }
    threadCpu += values.cpu;
    if (stack.some(({ name }) => name === 'pprofTestBusyLoop')) {
      loopCpu += values.cpu;
    }
  }
  // The loop alone uses `loopCpuMicros` of CPU time. A few samples can miss
  // their CPU time, e.g. the last ones before the profile stops.
  assert.ok(threadCpu >= loopCpuMicros * 1000 * 0.8,
            `${threadCpu} < 80% of ${loopCpuMicros * 1000}`);
  // `process.threadCpuUsage()` and the profiler can read the CPU time of the
  // thread from different clocks, which can disagree slightly.
  const maxCpu = maxThreadCpuNanos * 1.1 + 10e6;
  assert.ok(threadCpu <= maxCpu, `${threadCpu} > ${maxCpu}`);
  assert.ok(loopCpu > 0);
}

for (const workaroundV8Bug of [true, false]) {
  // The duration sizes the buffer that CPU time is attributed with: make it
  // large enough for the loop to run on a loaded machine.
  let before = process.threadCpuUsage();
  time.start({ collectCpuTime: true, durationMillis: 30_000, workaroundV8Bug });
  pprofTestBusyLoop(loopCpuMicros);
  const restarted = process.threadCpuUsage();
  const first = time.stop(true);
  const firstMax = threadCpuNanosSince(before);
  before = restarted;
  pprofTestBusyLoop(loopCpuMicros);
  const second = time.stop();
  const secondMax = threadCpuNanosSince(before);
  check(first, firstMax);
  check(second, secondMax);
}
