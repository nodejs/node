'use strict';

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { Worker } = require('node:worker_threads');

async function testCallbackThrows() {
  for (const steal of [false, true]) {
    const name = 'callback-throws';
    const error = new Error('callback failed');
    let next;

    await assert.rejects(navigator.locks.request(name, { steal }, common.mustCall(() => {
      next = navigator.locks.request(name, common.mustCall(() => 'granted'));
      throw error;
    })), error);

    const { held } = await navigator.locks.query();
    assert.deepStrictEqual(held, []);
    assert.strictEqual(await next, 'granted');
  }
}

async function testQueueContinuesAfterThrow() {
  const worker = new Worker(`
    const { parentPort } = require('node:worker_threads');
    navigator.locks.request('a', () => navigator.locks.request('b', () => {
      parentPort.once('message', () => process.exit());
      parentPort.postMessage('held');
      return new Promise(() => {});
    }));
  `, { eval: true });
  await once(worker, 'message');

  const error = new Error('callback failed');
  const first = assert.rejects(navigator.locks.request('a', common.mustCall(() => {
    throw error;
  })), error);
  const next = navigator.locks.request('b', common.mustCall(() => 'granted'));
  worker.postMessage('exit');

  await first;
  assert.strictEqual(await next, 'granted');
}

testCallbackThrows().then(testQueueContinuesAfterThrow).then(common.mustCall());
