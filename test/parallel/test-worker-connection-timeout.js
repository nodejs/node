'use strict';

const common = require('../common');
const { once } = require('node:events');
const {
  connect,
  setConnectionsListener,
  workerData,
  Worker,
  parentPort,
} = require('node:worker_threads');
const { rejects } = require('node:assert');

const level = workerData?.level ?? 0;

async function test() {
  const worker = new Worker(__filename, { workerData: { shouldHaveListener: false, level: level + 1 } });
  await once(worker, 'message');

  await rejects(common.mustCall(function() {
    return connect(worker.threadId, 1000);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_CONNECTION_REFUSED',
  });

  await rejects(common.mustCall(function() {
    return connect(worker.threadId, { foo: 'bar' }, 1000);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_CONNECTION_REFUSED',
  });

  worker.postMessage('success');
}

if (level === 0) {
  test();
} else {
  setConnectionsListener(common.mustCall(function() {
    return new Promise(common.mustCall(function(resolve) {
      setTimeout(resolve, common.platformTimeout(2000));
    }));
  }, 2));

  parentPort.postMessage('ready');

  // Keep the workers alive for the time being
  parentPort.once('message', common.mustCall());
}
