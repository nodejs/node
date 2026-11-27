'use strict';

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { setTimeout: delay } = require('node:timers/promises');
const { describe, it } = require('node:test');
const { Worker } = require('node:worker_threads');

describe('Web Locks with workers', () => {
  it('steals a lock held by another worker environment', async () => {
    let notifyAcquired;
    const acquired = new Promise((resolve) => {
      notifyAcquired = resolve;
    });
    const originalRequest = navigator.locks.request(
      'cross-environment-steal',
      common.mustCall(async () => {
        notifyAcquired();
        await new Promise(() => {});
      }),
    );
    const originalRejected = assert.rejects(
      originalRequest,
      { name: 'AbortError' },
    );

    await acquired;
    const worker = new Worker(`
      const { parentPort } = require('node:worker_threads');

      navigator.locks.request(
        'cross-environment-steal',
        { steal: true },
        () => 'stolen',
      ).then(
        (result) => parentPort.postMessage({ result }),
        (error) => parentPort.postMessage({ error: error.message }),
      );
    `, { eval: true });

    const [message] = await once(worker, 'message');
    assert.deepStrictEqual(message, { result: 'stolen' });
    await originalRejected;
    await worker.terminate();
  });

  it('rejects a stolen lock while its callback completes', async () => {
    const state = new Int32Array(new SharedArrayBuffer(4));
    const worker = new Worker(`
      const { parentPort, workerData } = require('node:worker_threads');
      const state = new Int32Array(workerData);

      parentPort.postMessage('ready');
      Atomics.wait(state, 0, 0);
      navigator.locks.request(
        'cross-environment-steal-release',
        { steal: true },
        () => {
          Atomics.store(state, 0, 2);
          Atomics.notify(state, 0);
        },
      ).then(() => parentPort.postMessage('stolen'));
    `, { eval: true, workerData: state.buffer });

    await once(worker, 'message');
    const stolen = once(worker, 'message');
    const originalRequest = navigator.locks.request(
      'cross-environment-steal-release',
      async () => {
        Atomics.store(state, 0, 1);
        Atomics.notify(state, 0);
        Atomics.wait(state, 0, 1);
      },
    );
    const originalRejected = assert.rejects(
      originalRequest,
      { name: 'AbortError' },
    );

    let timeout;
    await Promise.race([
      originalRejected,
      new Promise((resolve, reject) => {
        timeout = setTimeout(
          () => reject(new Error('stolen lock request did not settle')),
          1000,
        );
      }),
    ]).finally(() => clearTimeout(timeout));
    assert.deepStrictEqual(await stolen, ['stolen']);
    await worker.terminate();
  });

  it('keeps a worker alive while its request is pending', async () => {
    let worker;
    let outcome;

    await navigator.locks.request('pending-worker-request', async () => {
      worker = new Worker(`
        const { parentPort } = require('node:worker_threads');

        parentPort.postMessage('ready');
        navigator.locks.request('pending-worker-request', () => {
          parentPort.postMessage('acquired');
        });
      `, { eval: true });

      await once(worker, 'message');
      outcome = Promise.race([
        once(worker, 'message').then(([message]) => message),
        once(worker, 'exit').then(([code]) => `exit:${code}`),
      ]);
      await delay(100);
    });

    assert.strictEqual(await outcome, 'acquired');
    await worker.terminate();
  });

  it('grants a pending request after the lock owner terminates', async () => {
    const owner = new Worker(`
      const { parentPort } = require('node:worker_threads');

      parentPort.on('message', () => {});
      navigator.locks.request('terminated-lock-owner', async () => {
        parentPort.postMessage('acquired');
        await new Promise(() => {});
      });
    `, { eval: true });
    await once(owner, 'message');

    const waiter = new Worker(`
      const { parentPort } = require('node:worker_threads');

      const request = navigator.locks.request('terminated-lock-owner', () => {
        parentPort.postMessage('acquired');
      });
      parentPort.postMessage('queued');
      request.catch((error) => parentPort.postMessage(error.name));
    `, { eval: true });
    await once(waiter, 'message');
    const acquired = once(waiter, 'message').then(([message]) => message);

    await owner.terminate();
    const result = await Promise.race([
      acquired,
      delay(common.platformTimeout(500), null, { ref: false }),
    ]);
    if (result === null) {
      await waiter.terminate();
    }
    assert.strictEqual(result, 'acquired');
    await waiter.terminate();
  });

  it('lets a worker exit after its pending request is aborted', async () => {
    let worker;

    await navigator.locks.request('aborted-worker-request', common.mustCall(async () => {
      worker = new Worker(`
        const { parentPort } = require('node:worker_threads');
        const controller = new AbortController();

        navigator.locks.request(
          'aborted-worker-request',
          { signal: controller.signal },
          () => parentPort.postMessage('acquired'),
        ).catch((error) => parentPort.postMessage(error.name));
        setImmediate(() => controller.abort());
      `, { eval: true });

      const exited = once(worker, 'exit').then(([code]) => code);
      const [message] = await once(worker, 'message');
      assert.strictEqual(message, 'AbortError');
      const exitCode = await Promise.race([
        exited,
        delay(common.platformTimeout(500), null, { ref: false }),
      ]);
      if (exitCode === null) {
        await worker.terminate();
      }
      assert.strictEqual(exitCode, 0);
    }));
  });

  it('grants exclusive locks atomically across workers', async () => {
    const workerCount = 9;
    const rounds = 500;
    const resourceCount = 4;
    const counters = new Int32Array(
      new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * resourceCount * 2),
    );
    const workerSource = `
      const { parentPort, workerData } = require('node:worker_threads');
      const counters = new Int32Array(workerData.counters);

      parentPort.on('message', async (message) => {
        if (message === 'close') {
          parentPort.close();
          return;
        }

        for (let i = 0; i < workerData.rounds; i++) {
          const resource = i % workerData.resourceCount;
          await navigator.locks.request(\`exclusive-race-\${resource}\`, async () => {
            const active = Atomics.add(counters, resource, 1) + 1;
            if (active > 1) {
              Atomics.add(counters, workerData.resourceCount + resource, 1);
            }
            await new Promise(setImmediate);
            Atomics.sub(counters, resource, 1);
          });
        }

        parentPort.postMessage('done');
      });
      parentPort.postMessage('ready');
    `;
    const workers = Array.from({ length: workerCount }, () => new Worker(
      workerSource,
      {
        eval: true,
        workerData: {
          counters: counters.buffer,
          resourceCount,
          rounds,
        },
      },
    ));

    await Promise.all(workers.map(async (worker) => {
      await once(worker, 'message');
      worker.postMessage('start');
      const [message] = await once(worker, 'message');
      assert.strictEqual(message, 'done');
    }));

    const violations = Array.from(counters.subarray(resourceCount));
    await Promise.all(workers.map(async (worker) => {
      worker.postMessage('close');
      await once(worker, 'exit');
    }));
    assert.deepStrictEqual(violations, [0, 0, 0, 0]);
  });
});
