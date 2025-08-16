'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.on('message', () => {});
  `, { eval: true });

[
  -1,
  1.1,
  NaN,
  undefined,
  {},
  [],
  null,
  function() {},
  Symbol(),
  true,
  Infinity,
].forEach((name) => {
  try {
    worker.startCpuProfile(name);
  } catch (e) {
    assert.ok(/ERR_INVALID_ARG_TYPE/i.test(e.code));
  }
});

const name = 'demo';

worker.on('online', common.mustCall(async () => {
  {
    const handle = await worker.startCpuProfile(name);
    JSON.parse(await handle.stop());
    // Stop again
    JSON.parse(await handle.stop());
  }

  {
    const [handle1, handle2] = await Promise.all([
      worker.startCpuProfile('demo1'),
      worker.startCpuProfile('demo2'),
    ]);
    const [profile1, profile2] = await Promise.all([
      handle1.stop(),
      handle2.stop(),
    ]);
    JSON.parse(profile1);
    JSON.parse(profile2);
  }

  {
    // Calling startCpuProfile twice with same name will throw an error
    await worker.startCpuProfile(name);
    try {
      await worker.startCpuProfile(name);
    } catch (e) {
      assert.ok(/ERR_CPU_PROFILE_ALREADY_STARTED/i.test(e.code));
    }
    // Does not need to stop the profile because it will be stopped
    // automatically when the worker is terminated
  }

  worker.terminate();
}));

worker.once('exit', common.mustCall(async () => {
  await assert.rejects(worker.startCpuProfile(name), {
    code: 'ERR_WORKER_NOT_RUNNING'
  });
}));
