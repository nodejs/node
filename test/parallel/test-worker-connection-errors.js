'use strict';

const common = require('../common');
const { once } = require('node:events');
const {
  connect,
  setConnectionsListener,
  workerData,
  Worker,
  parentPort,
  threadId,
} = require('node:worker_threads');
const { rejects } = require('node:assert');

const level = workerData?.level ?? 0;

async function test() {
  await rejects(common.mustCall(function() {
    return connect(threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_SAME_THREAD',
  });

  await rejects(common.mustCall(function() {
    return connect(123);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_INVALID_ID',
  });

  // The first worker will refuse connection as there is no listener at all
  const worker1 = new Worker(__filename, { workerData: { shouldHaveListener: false, level: level + 1 } });

  // The second worker will refuse connection as the listener returns false
  const worker2 = new Worker(__filename, { workerData: { shouldHaveListener: true, level: level + 1 } });

  // The second worker will refuse connection as the listener throws
  const worker3 = new Worker(
    __filename,
    { workerData: { shouldHaveListener: true, shouldListenerFail: true, level: level + 1 } },
  );

  await Promise.all([
    once(worker1, 'message'),
    once(worker2, 'message'),
    once(worker3, 'message'),
  ]);

  await rejects(common.mustCall(function() {
    return connect(worker1.threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_CONNECTION_REFUSED',
  });

  await rejects(common.mustCall(function() {
    return connect(worker2.threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_CONNECTION_REFUSED',
  });

  await rejects(common.mustCall(function() {
    return connect(worker3.threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_CONNECTION_REFUSED',
  });

  worker1.postMessage('success');
  worker2.postMessage('success');
  worker3.postMessage('success');
}

if (level === 0) {
  test();
} else {
  if (workerData.shouldHaveListener) {
    setConnectionsListener(common.mustCall(function() {
      if (workerData.shouldListenerFail) {
        throw new Error('KABOOM');
      }

      return false;
    }));
  }

  parentPort.postMessage('ready');

  // Keep the workers alive for the time being
  parentPort.once('message', common.mustCall());
}
