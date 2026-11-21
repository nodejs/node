'use strict';

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { test } = require('node:test');
const { Worker } = require('node:worker_threads');

async function waitForGrant(request) {
  let timeout;
  try {
    return await Promise.race([
      request,
      new Promise((resolve, reject) => {
        timeout = setTimeout(() => {
          reject(new Error('Lock request was not granted after worker exit'));
        }, common.platformTimeout(5000));
      }),
    ]);
  } finally {
    clearTimeout(timeout);
  }
}

for (const exitType of ['exit', 'throw', 'resolve-exit', 'release']) {
  for (const waiterThread of ['main', 'worker']) {
    test(`${exitType} wakes a pending lock request in ${waiterThread}`, async (t) => {
      const name = `${exitType}-${waiterThread}`;
      const holder = new Worker(`
        const { parentPort, workerData } = require('node:worker_threads');
        navigator.locks.request(workerData.name, () => {
          return new Promise((resolve) => {
            parentPort.once('message', () => {
              switch (workerData.exitType) {
                case 'throw':
                  throw new Error('lock holder failed');
                case 'resolve-exit':
                  resolve();
                  process.exit();
                  break;
                case 'release':
                  resolve();
                  break;
                default:
                  process.exit();
              }
            });
            parentPort.postMessage('held');
          });
        });
      `, { eval: true, workerData: { name, exitType } });
      t.after(() => holder.terminate());
      let workerError;
      holder.on('error', (err) => { workerError = err; });
      assert.deepStrictEqual(await once(holder, 'message'), ['held']);

      let request;
      if (waiterThread === 'worker') {
        const waiter = new Worker(`
          const { parentPort, workerData } = require('node:worker_threads');
          // A pending lock request alone does not keep the worker alive.
          parentPort.on('message', () => {});
          navigator.locks.request(workerData, () => {
            parentPort.postMessage('granted');
          });
          parentPort.postMessage('queued');
        `, { eval: true, workerData: name });
        t.after(() => waiter.terminate());
        assert.deepStrictEqual(await once(waiter, 'message'), ['queued']);
        request = once(waiter, 'message').then(([message]) => message);
      } else {
        request = navigator.locks.request(name, common.mustCall(() => 'granted'));
        const { pending } = await navigator.locks.query();
        assert.strictEqual(pending.length, 1);
        assert.strictEqual(pending[0].name, name);
      }

      const exited = new Promise((resolve) => holder.once('exit', resolve));
      holder.postMessage('exit');
      assert.strictEqual(await exited, exitType === 'throw' ? 1 : 0);
      if (exitType === 'throw') {
        assert.strictEqual(workerError.message, 'lock holder failed');
      } else {
        assert.strictEqual(workerError, undefined);
      }
      assert.strictEqual(await waitForGrant(request), 'granted');
    });
  }
}

test('exiting with a pending exclusive request unblocks a shared request', async (t) => {
  const name = 'pending-worker-exit';
  await navigator.locks.request(name, { mode: 'shared' }, common.mustCall(async () => {
    const worker = new Worker(`
      const { parentPort, workerData } = require('node:worker_threads');
      parentPort.once('message', () => process.exit());
      navigator.locks.request(workerData, () => {
        throw new Error('Exclusive lock must stay blocked');
      });
      parentPort.postMessage('queued');
    `, { eval: true, workerData: name });
    t.after(() => worker.terminate());
    assert.deepStrictEqual(await once(worker, 'message'), ['queued']);

    const request = navigator.locks.request(
      name, { mode: 'shared' }, common.mustCall(() => 'granted'));
    const { pending } = await navigator.locks.query();
    assert.strictEqual(pending.length, 1);
    assert.strictEqual(pending[0].mode, 'shared');

    const exited = once(worker, 'exit');
    worker.postMessage('exit');
    assert.deepStrictEqual(await exited, [0]);
    assert.strictEqual(await waitForGrant(request), 'granted');
  }));
});

test('concurrent holder exits wake multiple requests and workers', async (t) => {
  const name = 'concurrent-worker-exit';
  const active = new Int32Array(new SharedArrayBuffer(4));
  const holders = Array.from({ length: 2 }, () => new Worker(`
    const { parentPort, workerData } = require('node:worker_threads');
    navigator.locks.request(workerData, { mode: 'shared' }, () => {
      parentPort.once('message', () => process.exit());
      parentPort.postMessage('held');
      return new Promise(() => {});
    });
  `, { eval: true, workerData: name }));
  t.after(() => Promise.all(holders.map((worker) => worker.terminate())));
  assert.deepStrictEqual(
    await Promise.all(holders.map((worker) => once(worker, 'message'))),
    [['held'], ['held']]);

  // Queue worker waiters first so cleanup wakes competing Environments.
  const waiters = Array.from({ length: 8 }, () => new Worker(`
    const assert = require('node:assert');
    const { parentPort, workerData } = require('node:worker_threads');
    const active = new Int32Array(workerData.counter);
    parentPort.on('message', () => {});
    navigator.locks.request(workerData.name, () => {
      assert.strictEqual(Atomics.add(active, 0, 1), 0);
      return new Promise(resolve => {
        parentPort.once('message', () => {
          assert.strictEqual(Atomics.sub(active, 0, 1), 1);
          resolve();
        });
        parentPort.postMessage('granted');
      });
    }).then(() => parentPort.postMessage('released'));
    parentPort.postMessage('queued');
  `, { eval: true, workerData: { name, counter: active.buffer } }));
  t.after(() => Promise.all(waiters.map((worker) => worker.terminate())));
  assert.deepStrictEqual(
    await Promise.all(waiters.map((worker) => once(worker, 'message'))),
    Array.from({ length: 8 }, () => ['queued']));
  const granted = waiters.map((worker) => new Promise((resolve, reject) => {
    worker.on('message', (message) => {
      if (message === 'granted') worker.postMessage('release');
      if (message === 'released') resolve('granted');
    });
    worker.once('error', reject);
  }));

  const requests = [];
  for (const value of [0, 1]) {
    requests.push(navigator.locks.request(name, common.mustCall(() => {
      assert.strictEqual(Atomics.add(active, 0, 1), 0);
      assert.strictEqual(Atomics.sub(active, 0, 1), 1);
      return value;
    })));
  }
  const { pending } = await navigator.locks.query();
  assert.strictEqual(pending.length, 2);

  const exited = holders.map((worker) => once(worker, 'exit'));
  const completed = Promise.all([...exited, ...requests, ...granted]);
  for (const worker of holders) worker.postMessage('exit');
  assert.deepStrictEqual(await waitForGrant(completed),
                         [[0], [0], 0, 1, ...Array(8).fill('granted')]);
  assert.strictEqual(Atomics.load(active, 0), 0);
});
