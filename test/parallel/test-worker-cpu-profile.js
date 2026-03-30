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
    const handle = await worker.startCpuProfile();
    JSON.parse(await handle.stop());
    // Stop again
    JSON.parse(await handle.stop());
  }

  {
    const [handle1, handle2] = await Promise.all([
      worker.startCpuProfile(),
      worker.startCpuProfile(),
    ]);
    const [profile1, profile2] = await Promise.all([
      handle1.stop(),
      handle2.stop(),
    ]);
    JSON.parse(profile1);
    JSON.parse(profile2);
  }

  {
    await worker.startCpuProfile();
    // It will be stopped automatically when the worker is terminated
  }
  worker.terminate();
}));

worker.once('exit', common.mustCall(async () => {
  await assert.rejects(worker.startCpuProfile(), {
    code: 'ERR_WORKER_NOT_RUNNING'
  });
}));
