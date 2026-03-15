'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');
const { Worker } = require('worker_threads');
const { heapProfilerConstants } = v8;

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.on('message', () => {});
  `, { eval: true });

worker.on('online', common.mustCall(async () => {
  assert.throws(() => worker.startHeapProfile('1024'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => worker.startHeapProfile(1.1), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile(0), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile(-1), {
    code: 'ERR_OUT_OF_RANGE',
  });

  assert.throws(() => worker.startHeapProfile(1024, '16'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => worker.startHeapProfile(1024, 1.1), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile(1024, -1), {
    code: 'ERR_OUT_OF_RANGE',
  });


  assert.throws(() => worker.startHeapProfile(1024, 16, '0'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => worker.startHeapProfile(1024, 16, -1), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.throws(() => worker.startHeapProfile(1024, 16, 8), {
    code: 'ERR_OUT_OF_RANGE',
  });

  {
    const handle = await worker.startHeapProfile(
      1024,
      8,
      heapProfilerConstants.SAMPLING_FORCE_GC |
        heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MAJOR_GC |
        heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MINOR_GC,
    );
    JSON.parse(await handle.stop());
    // Stop again
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
