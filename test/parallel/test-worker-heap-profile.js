'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.on('message', () => {});
  `, { eval: true });

worker.on('online', common.mustCall(async () => {
  assert.throws(() => worker.startHeapProfile('bad'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => worker.startHeapProfile({ sampleInterval: '1024' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => worker.startHeapProfile({ sampleInterval: 1.1 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile({ sampleInterval: 0 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile({ sampleInterval: -1 }), {
    code: 'ERR_OUT_OF_RANGE',
  });

  assert.throws(() => worker.startHeapProfile({ stackDepth: '16' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => worker.startHeapProfile({ stackDepth: 1.1 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile({ stackDepth: -1 }), {
    code: 'ERR_OUT_OF_RANGE',
  });

  assert.throws(() => worker.startHeapProfile({ forceGC: 'true' }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(
    () => worker.startHeapProfile({ includeObjectsCollectedByMajorGC: 1 }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  assert.throws(
    () => worker.startHeapProfile({ includeObjectsCollectedByMinorGC: 1 }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });

  {
    const handle = await worker.startHeapProfile({
      sampleInterval: 1024,
      stackDepth: 8,
      forceGC: true,
      includeObjectsCollectedByMajorGC: true,
      includeObjectsCollectedByMinorGC: true,
    });
    JSON.parse(await handle.stop());
    // Stop again returns cached result.
    JSON.parse(await handle.stop());
  }

  {
    const handle = await worker.startHeapProfile();
    try {
      await worker.startHeapProfile();
    } catch (err) {
      assert.strictEqual(err.code, 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED');
    }
    JSON.parse(await handle.stop());
  }

  worker.terminate();
}));

worker.once('exit', common.mustCall(async () => {
  await assert.rejects(worker.startHeapProfile(), {
    code: 'ERR_WORKER_NOT_RUNNING'
  });
}));
