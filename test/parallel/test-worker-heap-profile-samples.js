// Flags: --sampling-heap-profiler-suppress-randomness
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { Worker } = require('worker_threads');

async function main() {
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    const retained = [];
    parentPort.on('message', () => {
      for (let i = 0; i < 512; i++) {
        retained.push(new Array(64).fill(i));
      }
      parentPort.postMessage('allocated');
    });
  `, { eval: true });
  await once(worker, 'online');
  const handle = await worker.startHeapProfile({
    sampleInterval: 1024,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
  });

  for (const value of [undefined, null, true, '1024', {}, 1n]) {
    assert.throws(() => handle.setSampleInterval(value), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
  for (const value of [0, -1, 1.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1]) {
    assert.throws(() => handle.setSampleInterval(value), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }

  worker.postMessage('allocate');
  await once(worker, 'message');
  const before = await handle.getSamples();
  assert(before.length > 0);
  assert(before.every((sample) => sample.sampleInterval === 1024));

  assert.strictEqual(await handle.setSampleInterval(4096), undefined);
  worker.postMessage('allocate');
  await once(worker, 'message');
  const after = await handle.getSamples();
  assert(after.some((sample) => sample.sampleInterval === 4096));
  const byOrdinal = new Map(after.map((sample) => [sample.ordinal, sample]));
  for (const sample of before) {
    const current = byOrdinal.get(sample.ordinal);
    assert(current);
    assert.strictEqual(current.sampleInterval, sample.sampleInterval);
    assert.strictEqual(current.objectCount, sample.objectCount);
  }

  const stop = handle.stop();
  await assert.rejects(handle.getSamples(), {
    code: 'ERR_HEAP_PROFILE_NOT_STARTED',
  });
  await assert.rejects(handle.setSampleInterval(1), {
    code: 'ERR_HEAP_PROFILE_NOT_STARTED',
  });
  const profile = JSON.parse(await stop);
  const saved = new Map(profile.samples.map((sample) => [sample.ordinal, sample]));
  for (const sample of after) {
    assert.strictEqual(saved.get(sample.ordinal).sampleInterval, sample.sampleInterval);
    assert.strictEqual(sample.size, sample.objectSize * sample.objectCount);
    assert.strictEqual(typeof sample.nodeId, 'number');
    assert.strictEqual(typeof sample.ordinal, 'number');
    assert.strictEqual(typeof sample.isLive, 'boolean');
  }

  const next = await worker.startHeapProfile({ sampleInterval: 1024 });
  await assert.rejects(handle.setSampleInterval(1), {
    code: 'ERR_HEAP_PROFILE_NOT_STARTED',
  });
  await assert.rejects(handle.getSamples(), {
    code: 'ERR_HEAP_PROFILE_NOT_STARTED',
  });
  worker.postMessage('allocate');
  await once(worker, 'message');
  const nextSamples = await next.getSamples();
  assert(nextSamples.length > 0);
  assert(nextSamples.every((sample) => sample.sampleInterval === 1024));
  await next[Symbol.asyncDispose]();
  await assert.rejects(next.getSamples(), {
    code: 'ERR_HEAP_PROFILE_NOT_STARTED',
  });

  await worker.terminate();
}

main().then(common.mustCall());
