'use strict';
const common = require('../../common');

if (common.isAIX || common.isIBMi) {
  // see: https://github.com/libuv/libuv/pull/4599#issuecomment-2498376606
  common.skip('AIX, IBM i do not support get/set thread name');
}

const assert = require('node:assert');
const { parentPort, Worker, isMainThread } = require('node:worker_threads');
const bindingPath = require.resolve(`./build/${common.buildType}/binding`);
const binding = require(bindingPath);

if (isMainThread) {
  assert.strictEqual(binding.getThreadName(), 'MainThread');

  const worker = new Worker(__filename);
  worker.on('message', common.mustCall((data) => {
    assert.strictEqual(data, 'WorkerThread');
  }));
  worker.on('error', common.mustNotCall());
  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));

  const namedWorker = new Worker(__filename, { name: 'NamedThread' });
  namedWorker.on('message', common.mustCall((data) => {
    assert.strictEqual(data, 'NamedThread');
  }));
  namedWorker.on('error', common.mustNotCall());
  namedWorker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
} else {
  parentPort.postMessage(binding.getThreadName());
}
