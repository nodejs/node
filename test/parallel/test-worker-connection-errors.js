'use strict';

const common = require('../common');
const { once } = require('node:events');
const {
  parentPort,
  postMessageToWorker,
  threadId,
  Worker,
  isMainThread,
} = require('node:worker_threads');
const { rejects } = require('node:assert');

async function test() {
  await rejects(common.mustCall(function() {
    return postMessageToWorker(threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_SAME_THREAD',
  });

  await rejects(common.mustCall(function() {
    return postMessageToWorker(123);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_INVALID_ID',
  });

  // The delivery to the first worker will fail as there is no listener for `workerMessage`
  const worker = new Worker(__filename);
  await once(worker, 'message');

  await rejects(common.mustCall(function() {
    return postMessageToWorker(worker.threadId);
  }), {
    name: 'Error',
    code: 'ERR_WORKER_INVALID_ID',
  });

  worker.postMessage('success');
}

if (isMainThread) {
  test();
} else {
  parentPort.postMessage('ready');
  parentPort.once('message', common.mustCall());
}
