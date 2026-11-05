// Flags: --experimental-green-threads --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { spawnSyncAndAssert } = require('../common/child_process');
const { GreenThread, setWarmPoolSize } = require('node:green_threads');

assert.throws(() => setWarmPoolSize(-1), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => setWarmPoolSize(1.5), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => setWarmPoolSize(2048), { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => setWarmPoolSize('1'), { code: 'ERR_INVALID_ARG_TYPE' });

async function echo(options) {
  const greenThread = new GreenThread(
    'onmessage = ({ data }) => postMessage(data + 1);', options);
  greenThread.postMessage(41);
  const [value] = await once(greenThread, 'message');
  assert.strictEqual(value, 42);
  assert.strictEqual(await greenThread.terminate(), 1);
}

(async () => {
  setWarmPoolSize(4);
  // More green threads than the pool holds, some of them created before the
  // pool has refilled, and some with options that warm isolates can't be
  // used for.
  await Promise.all(Array.from({ length: 30 }, (_, i) => echo(
    i % 3 === 0 ? { resourceLimits: { maxMemoryMb: 64 } } :
      i % 3 === 1 ? { resourceLimits: { maxTaskCpuTimeMs: 1000 } } :
        undefined)));

  // Warm isolates are fresh: nothing leaks from one green thread to the next.
  for (let i = 0; i < 8; i++) {
    const greenThread = new GreenThread('');
    assert.strictEqual(await greenThread.evaluate(
      'const seen = typeof globalThis.leaked; globalThis.leaked = 1; seen'),
                       'undefined');
    await greenThread.terminate();
  }

  // A CPU limit works on a warm isolate.
  {
    const greenThread = new GreenThread('for (;;);', {
      resourceLimits: { maxTaskCpuTimeMs: 20 },
    });
    const [error] = await once(greenThread, 'error').catch((e) => [e]);
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_CPU_LIMIT');
  }

  setWarmPoolSize(0);
  await echo();
})().then(common.mustCall());

// Warm isolates are disposed of when the process exits, both normally and
// through process.exit().
for (const exit of ['', 'process.exit(0);']) {
  spawnSyncAndAssert(process.execPath, [
    '--experimental-green-threads',
    '--no-warnings',
    '-e',
    `
    const { GreenThread, setWarmPoolSize } = require('node:green_threads');
    setWarmPoolSize(16);
    const greenThread = new GreenThread('postMessage(0);');
    greenThread.on('message', () => {
      greenThread.terminate();
      ${exit}
    });
    `,
  ], { status: 0, stdout: '', stderr: '' });
}

// A Worker can fill the pool, and it outlives the Worker.
spawnSyncAndAssert(process.execPath, [
  '--experimental-green-threads',
  '--no-warnings',
  '-e',
  `
  const { Worker } = require('worker_threads');
  const worker = new Worker(
    "require('node:green_threads').setWarmPoolSize(4);", { eval: true });
  worker.on('exit', async () => {
    const { GreenThread } = require('node:green_threads');
    const greenThread = new GreenThread('');
    console.log(await greenThread.evaluate('6 * 7'));
    greenThread.terminate();
  });
  `,
], { status: 0, stdout: '42\n', stderr: '' });
