'use strict';

// node:threads/worker and node:threads/messaging export the same objects as
// node:worker_threads, can only be loaded with the node: scheme, and see the
// parentPort and workerData of the thread they are loaded in.

const common = require('../common');
const assert = require('assert');
const { builtinModules, isBuiltin } = require('module');
const workerThreads = require('worker_threads');
const threadsWorker = require('node:threads/worker');
const threadsMessaging = require('node:threads/messaging');

const workerExports = [
  'getEnvironmentData',
  'isInternalThread',
  'isMainThread',
  'parentPort',
  'postMessageToThread',
  'resourceLimits',
  'SHARE_ENV',
  'setEnvironmentData',
  'threadId',
  'threadName',
  'Worker',
  'workerData',
];
const messagingExports = [
  'isMarkedAsUntransferable',
  'markAsUncloneable',
  'markAsUntransferable',
  'moveMessagePortToContext',
  'receiveMessageOnPort',
];

assert.deepStrictEqual(Object.keys(threadsWorker).sort(),
                       [...workerExports].sort());
assert.deepStrictEqual(Object.keys(threadsMessaging).sort(),
                       [...messagingExports].sort());
for (const name of workerExports) {
  assert.strictEqual(threadsWorker[name], workerThreads[name], name);
}
for (const name of messagingExports) {
  assert.strictEqual(threadsMessaging[name], workerThreads[name], name);
}

for (const id of ['threads/worker', 'threads/messaging']) {
  assert(isBuiltin(`node:${id}`), id);
  assert(!isBuiltin(id), id);
  assert(builtinModules.includes(`node:${id}`), id);
  assert(!builtinModules.includes(id), id);
  assert.throws(() => require(id), { code: 'MODULE_NOT_FOUND' });
}

const worker = new threadsWorker.Worker(`
  const assert = require('assert');
  const workerThreads = require('node:worker_threads');
  const threadsWorker = require('node:threads/worker');
  assert.strictEqual(threadsWorker.isMainThread, false);
  assert.strictEqual(threadsWorker.parentPort, workerThreads.parentPort);
  assert.strictEqual(threadsWorker.workerData, workerThreads.workerData);
  assert.strictEqual(threadsWorker.threadId, workerThreads.threadId);
  threadsWorker.parentPort.postMessage(threadsWorker.workerData);
`, { eval: true, workerData: { hello: 'world' } });
worker.on('message', common.mustCall((message) => {
  assert.deepStrictEqual(message, { hello: 'world' });
}));
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
