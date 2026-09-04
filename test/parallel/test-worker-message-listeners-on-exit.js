'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Worker } = require('node:worker_threads');

const results = [];

function test(name, code, terminate) {
  const worker = new Worker(code, { eval: true });

  worker.on('message', common.mustNotCall());
  worker.on('messageerror', common.mustNotCall());

  assert.strictEqual(worker.listenerCount('message'), 1);
  assert.strictEqual(worker.listenerCount('messageerror'), 1);

  worker.on('exit', common.mustCall(() => {
    results.push({
      name,
      message: worker.listenerCount('message'),
      messageerror: worker.listenerCount('messageerror'),
    });

    if (results.length === 2) {
      results.sort((a, b) => a.name.localeCompare(b.name));
      assert.deepStrictEqual(results, [
        { name: 'normal', message: 0, messageerror: 0 },
        { name: 'terminated', message: 0, messageerror: 0 },
      ]);
    }
  }));

  if (terminate) {
    worker.on('online', () => {
      worker.terminate();
    });
  }
}

test('normal', '', false);
test('terminated', 'setInterval(() => {}, 100);', true);
