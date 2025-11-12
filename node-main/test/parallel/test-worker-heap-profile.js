'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.on('message', () => {});
  `, { eval: true });

worker.on('online', common.mustCall(async () => {
  {
    const handle = await worker.startHeapProfile();
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
