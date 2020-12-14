'use strict';

const common = require('../common');
const { Worker } = require('worker_threads');
const assert = require('assert');

(async () => {
  const worker = new Worker(`
    const { parentPort } = require('worker_threads');
    const assert = require('assert');
    parentPort.on('periodicSync', ({ type, tag }) => {
      assert.strictEqual(type, 'periodicSync');
      assert.strictEqual(tag, 'foo');
      parentPort.postMessage({ type, tag });
    });
    parentPort.on('message', () => { /** nop to keep worker alive **/ });
  `, { eval: true });

  let calls = 0;

  worker.on('message', common.mustCall(({ type, tag }) => {
    assert.strictEqual(type, 'periodicSync');
    assert.strictEqual(tag, 'foo');
    if (++calls === 2)
      worker.terminate();
  }, 2));

  assert(worker.periodicSync);

  await worker.periodicSync.register('foo', { minInterval: 100 });
  assert.deepStrictEqual(await worker.periodicSync.getTags(), ['foo']);

  assert.rejects(worker.periodicSync.register('bar', { minInterval: 'a' }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.rejects(worker.periodicSync.register(Symbol('bob')), TypeError);
})().then(common.mustCall());
