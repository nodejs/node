'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on the main thread');
}

const worker = new Worker(`
  const { parentPort } = require('worker_threads');
  parentPort.once('message', () => process.exit(0));
  parentPort.postMessage('ready');
`, { eval: true });

worker.once('message', common.mustCall(async () => {
  const usage = await worker.getMemoryUsage();
  const keys = [
    'rss',
    'heapTotal',
    'heapUsed',
    'external',
    'arrayBuffers',
  ].sort();

  assert.deepStrictEqual(Object.keys(usage).sort(), keys);

  for (const key of keys) {
    assert.strictEqual(typeof usage[key], 'number', `Expected ${key} to be a number`);
    assert.ok(usage[key] >= 0, `Expected ${key} to be >= 0`);
  }

  assert.ok(usage.heapUsed <= usage.heapTotal,
            'heapUsed should not exceed heapTotal');

  worker.postMessage('exit');
}));

worker.once('exit', common.mustCall(async (code) => {
  assert.strictEqual(code, 0);
  await assert.rejects(worker.getMemoryUsage(), {
    code: 'ERR_WORKER_NOT_RUNNING'
  });
}));
