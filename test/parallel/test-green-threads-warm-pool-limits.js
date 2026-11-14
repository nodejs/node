// Flags: --expose-internals --experimental-green-threads --no-warnings
'use strict';

// setWarmPoolSize() fills a pool for each isolate group and set of memory
// limits, which only green threads with the same ones use, and the limits
// apply to them as to green threads that start without the warm pool.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { internalBinding } = require('internal/test/binding');
const { kTestingFlags } = require('internal/green_threads/testing');
const {
  GreenThread,
  IsolateGroup,
  setWarmPoolSize,
} = require('node:green_threads');

const { kFlagTestingGlobals } = internalBinding('green_thread');
const kMB = 1024 * 1024;

assert.throws(() => setWarmPoolSize(1, null),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => setWarmPoolSize(1, { resourceLimits: 1 }),
              { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(
  () => setWarmPoolSize(1, { resourceLimits: { maxMemoryMb: -1 } }),
  { code: 'ERR_OUT_OF_RANGE' });
assert.throws(() => setWarmPoolSize(1, { isolateGroup: 'shared' }),
              { code: 'ERR_INVALID_ARG_VALUE' });

// Resolves with what __greenThreadInfo() returns in a green thread created
// with `options`, and terminates it.
async function info(options = {}, source = '') {
  const greenThread = new GreenThread(
    `${source}; postMessage(__greenThreadInfo());`,
    { ...options, [kTestingFlags]: kFlagTestingGlobals });
  const [result] = await once(greenThread, 'message');
  await greenThread.terminate();
  return result;
}

(async () => {
  const limited = { resourceLimits: { maxMemoryMb: 64 } };
  await setWarmPoolSize(2, limited);
  // Only green threads with the same isolate options use the pool. CPU time
  // limits don't matter.
  assert.strictEqual((await info(limited)).fromWarmPool, true);
  assert.strictEqual((await info({
    resourceLimits: { maxMemoryMb: 64, maxTaskCpuTimeMs: 1000 },
  })).fromWarmPool, true);
  assert.strictEqual(
    (await info({ resourceLimits: { maxMemoryMb: 32 } })).fromWarmPool, false);
  assert.strictEqual((await info()).fromWarmPool, false);

  // The memory limit applies to green threads from the pool.
  await setWarmPoolSize(2, limited);
  {
    const greenThread = new GreenThread(`
      const items = [];
      for (;;) items.push(new Array(1e4));
    `, { ...limited, [kTestingFlags]: kFlagTestingGlobals });
    const [error] = await once(greenThread, 'error');
    assert.strictEqual(error.code, 'ERR_GREEN_THREAD_OUT_OF_MEMORY');
  }
  await setWarmPoolSize(0, limited);
  assert.strictEqual((await info(limited)).fromWarmPool, false);

  // So does a limit for the young generation alone.
  {
    const young = { resourceLimits: { maxYoungGenerationSizeMb: 1 } };
    await setWarmPoolSize(1, young);
    const { fromWarmPool, max } = await new Promise((resolve) => {
      const greenThread = new GreenThread(`
        let max = 0;
        let objects = [];
        for (let i = 0; i < 1e6; i++) {
          objects.push({ i });
          if (objects.length > 1000) objects = [];
          if (i % 10000 === 0) {
            max = Math.max(max, __greenThreadInfo().heapSpaces.new_space.size);
          }
        }
        postMessage({ fromWarmPool: __greenThreadInfo().fromWarmPool, max });
      `, { ...young, [kTestingFlags]: kFlagTestingGlobals });
      greenThread.once('message', (value) => {
        greenThread.terminate();
        resolve(value);
      });
    });
    assert.strictEqual(fromWarmPool, true);
    assert(max <= kMB, `${max}`);
    await setWarmPoolSize(0, young);
  }

  // Pools for isolate groups.
  if (IsolateGroup.supported) {
    const group = new IsolateGroup();
    await setWarmPoolSize(1, { isolateGroup: group });
    const inGroup = await info({ isolateGroup: group });
    assert.strictEqual(inGroup.fromWarmPool, true);
    assert.strictEqual(
      (await info({ isolateGroup: new IsolateGroup() })).fromWarmPool, false);
    // It is in that group.
    assert.strictEqual((await info({ isolateGroup: group })).isolateGroup,
                       inGroup.isolateGroup);
    await setWarmPoolSize(0, { isolateGroup: group });

    // 'default' is a different pool than the default, 'new'.
    await setWarmPoolSize(1, { isolateGroup: 'default' });
    assert.strictEqual((await info()).fromWarmPool, false);
    const inDefault = await info({ isolateGroup: 'default' });
    assert.strictEqual(inDefault.fromWarmPool, true);
    assert.strictEqual(inDefault.isolateGroup, 0);
    await setWarmPoolSize(0, { isolateGroup: 'default' });
  }

  // The default pool is still there.
  await setWarmPoolSize(1);
  assert.strictEqual((await info()).fromWarmPool, true);
  await setWarmPoolSize(0);
})().then(common.mustCall());
