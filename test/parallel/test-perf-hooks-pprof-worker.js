// Flags: --allow-natives-syntax
'use strict';

// Checks that the time profiler runs in the main thread and in workers at the
// same time, including when workers are terminated while profiling.

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { Worker, isMainThread, parentPort } = require('node:worker_threads');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

// Profiles lack inlined frames of optimized code that was compiled before the
// profiler started: see pprofTestBusyLoop() in test/common/pprof.js.
eval('%NeverOptimizeFunction(pprofTestBusyLoop)');

const collectCpuTime = !common.isWindows;

/**
 * Profiles `pprofTestBusyLoop` on the current thread, and asserts that the
 * profile contains samples of it.
 * @throws {AssertionError} If the profile has no samples of the function.
 */
function profileLoop() {
  // The duration sizes the buffer that CPU time is attributed with: make it
  // large enough for the loop to run on a loaded machine.
  time.start({ collectCpuTime, durationMillis: 30_000 });
  pprofTestBusyLoop(200_000);
  const { samples } = decodeGzippedProfile(encodeSync(time.stop()));
  assert.ok(samples.some(({ stack }) =>
    stack.some(({ name }) => name === 'pprofTestBusyLoop')));
}

if (!isMainThread) {
  if (process.argv[2] === 'terminate') {
    time.start({ collectCpuTime });
    parentPort.postMessage('started');
    // Keep sampling until terminated.
    for (;;) pprofTestBusyLoop(10_000);
  }
  profileLoop();
  parentPort.postMessage('done');
  return;
}

// Workers that profile and stop normally, alongside the main thread.
for (let i = 0; i < 2; i++) {
  const worker = new Worker(__filename);
  worker.on('message', common.mustCall((message) => {
    assert.strictEqual(message, 'done');
  }));
  worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
}
profileLoop();

// Workers that are terminated while profiling.
/**
 * Terminates workers while they are profiling, `count` at a time, `rounds`
 * times.
 * @param {number} rounds The number of times to start and terminate
 *   workers.
 * @param {number} count The number of workers to run at the same time.
 * @returns {Promise<void>} Resolves once all the workers were terminated.
 * @throws {Error} Rejects if a worker sends another message than `started`.
 */
async function terminateWhileProfiling(rounds, count) {
  for (let round = 0; round < rounds; round++) {
    await Promise.all(Array.from({ length: count }, async () => {
      const worker = new Worker(__filename, { argv: ['terminate'] });
      const [message] = await once(worker, 'message');
      if (message !== 'started') throw new Error(`Unexpected ${message}`);
      await worker.terminate();
    }));
  }
}

terminateWhileProfiling(3, 5).then(common.mustCall(() => {
  // The main thread can still profile afterwards.
  profileLoop();
}));
