// Flags: --expose-internals
// Regression test for https://github.com/nodejs/node/issues/62644.
'use strict';

const common = require('../common');
if (!common.isDebug) {
  common.skip('Only works in debug mode');
}

const assert = require('node:assert');
const { once } = require('node:events');
const {
  isMainThread,
  parentPort,
  workerData,
  Worker,
} = require('node:worker_threads');

const resource = `web-locks-no-livelock`;

if (!isMainThread) {
  // A pending lock request alone does not keep the worker alive, so keep a
  // message listener until the parent terminates this Environment.
  parentPort.on('message', () => {});
  navigator.locks.request(workerData, () => {});
  parentPort.postMessage('ready');
} else {
  const { internalBinding } = require('internal/test/binding');
  const { getGenericUsageCount } = internalBinding('debug');
  const wakeCounter = 'LockManager.WakeEnvironment';

  (async () => {
    let worker;
    let pendingInMain;
    await navigator.locks.request(resource, common.mustCall(async () => {
      // The worker queues an exclusive request for the resource held above.
      worker = new Worker(__filename, { workerData: resource });
      await once(worker, 'message');

      // Queueing a second blocked request must not wake the worker: its
      // request cannot make progress, so the two Environments would wake
      // each other forever (livelock).
      const wakeCount = getGenericUsageCount(wakeCounter);
      pendingInMain = navigator.locks.request(resource, common.mustCall());
      assert.strictEqual(getGenericUsageCount(wakeCounter), wakeCount);
    }));

    await pendingInMain;
    await worker.terminate();
  })().then(common.mustCall());
}
