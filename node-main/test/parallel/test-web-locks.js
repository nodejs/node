'use strict';
// Flags: --expose-gc

const common = require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const { Worker } = require('node:worker_threads');
const { AsyncLocalStorage } = require('node:async_hooks');

describe('Web Locks with worker threads', () => {
  it('should handle exclusive locks', async () => {
    const worker = new Worker(`
      const { parentPort } = require('worker_threads');
      const assert = require('node:assert');
      
      navigator.locks.request('exclusive-test', async (lock) => {
        assert.strictEqual(lock.mode, 'exclusive');
        parentPort.postMessage({ success: true });
      }).catch(err => parentPort.postMessage({ error: err.message }));
    `, { eval: true });

    const result = await new Promise((resolve) => {
      worker.once('message', resolve);
    });

    assert.strictEqual(result.success, true);
    await worker.terminate();

    await navigator.locks.request('exclusive-test', async (lock) => {
      assert.strictEqual(lock.mode, 'exclusive');
      assert.strictEqual(lock.name, 'exclusive-test');
    });
  });

  it('should handle shared locks', async () => {
    const worker = new Worker(`
      const { parentPort } = require('worker_threads');
      const assert = require('node:assert');
      
      navigator.locks.request('shared-test', { mode: 'shared' }, async (lock) => {
        assert.strictEqual(lock.mode, 'shared');
        parentPort.postMessage({ success: true });
      }).catch(err => parentPort.postMessage({ error: err.message }));
    `, { eval: true });

    const result = await new Promise((resolve) => {
      worker.once('message', resolve);
    });
    assert.strictEqual(result.success, true);

    await navigator.locks.request('shared-test', { mode: 'shared' }, async (lock1) => {
      await navigator.locks.request('shared-test', { mode: 'shared' }, async (lock2) => {
        assert.strictEqual(lock1.mode, 'shared');
        assert.strictEqual(lock2.mode, 'shared');
      });
    });

    await worker.terminate();
  });

  it('should handle steal option - no existing lock', async () => {
    await navigator.locks.request('steal-simple', { steal: true }, async (lock) => {
      assert.strictEqual(lock.name, 'steal-simple');
      assert.strictEqual(lock.mode, 'exclusive');
    });
  });

  it('should handle steal option - existing lock', async () => {
    let originalLockRejected = false;

    const originalLockPromise = navigator.locks.request('steal-target', async (lock) => {
      assert.strictEqual(lock.name, 'steal-target');
      return 'original-completed';
    }).catch((err) => {
      originalLockRejected = true;
      assert.strictEqual(err.name, 'AbortError');
      assert.strictEqual(err.message, 'The operation was aborted');
      return 'original-rejected';
    });

    const stealResult = await navigator.locks.request('steal-target', { steal: true }, async (stolenLock) => {
      assert.strictEqual(stolenLock.name, 'steal-target');
      assert.strictEqual(stolenLock.mode, 'exclusive');
      return 'steal-completed';
    });

    assert.strictEqual(stealResult, 'steal-completed');

    const originalResult = await originalLockPromise;
    assert.strictEqual(originalLockRejected, true);
    assert.strictEqual(originalResult, 'original-rejected');
  });

  it('should handle ifAvailable option', async () => {
    await navigator.locks.request('ifavailable-test', async () => {
      const result = await navigator.locks.request('ifavailable-test', { ifAvailable: true }, (lock) => {
        return lock; // should be null
      });

      assert.strictEqual(result, null);

      const availableResult = await navigator.locks.request('ifavailable-different-resource',
                                                            { ifAvailable: true }, (lock) => {
                                                              return lock !== null;
                                                            });

      assert.strictEqual(availableResult, true);
    });
  });

  it('should handle AbortSignal', async () => {
    const worker = new Worker(`
      const { parentPort } = require('worker_threads');
      const assert = require('node:assert');
      
      const controller = new AbortController();
      
      navigator.locks.request('signal-after-grant', { signal: controller.signal }, async (lock) => {
        parentPort.postMessage({ acquired: true });
        
        setTimeout(() => controller.abort(), 50);
        
        await new Promise(resolve => setTimeout(resolve, 100));
        return 'completed successfully';
      }).then(result => {
        parentPort.postMessage({ resolved: result });
      }).catch(err => {
        parentPort.postMessage({ rejected: err.name });
      });
    `, { eval: true });

    const acquired = await new Promise((resolve) => {
      worker.once('message', resolve);
    });
    assert.strictEqual(acquired.acquired, true);

    const result = await new Promise((resolve) => {
      worker.once('message', resolve);
    });
    assert.strictEqual(result.resolved, 'completed successfully');

    await worker.terminate();
  });

  it('should handle many concurrent locks without hanging', async () => {
    if (global.gc) global.gc();
    const before = process.memoryUsage().rss;

    let callbackCount = 0;
    let resolveCount = 0;

    const promises = [];
    for (let i = 0; i < 100; i++) {
      const promise = navigator.locks.request(`test-${i}`, async (lock) => {
        callbackCount++;
        const innerPromise = navigator.locks.request(`inner-${i}`, async () => {
          resolveCount++;
          return 'done';
        });
        await innerPromise;
        return `completed-${lock.name}`;
      });

      promises.push(promise);
    }

    await Promise.all(promises);

    if (global.gc) global.gc();

    const after = process.memoryUsage().rss;

    assert.strictEqual(callbackCount, 100);
    assert.strictEqual(resolveCount, 100);
    assert(after < before * 3);
  });

  it('should preserve AsyncLocalStorage context across lock callback', async () => {
    const als = new AsyncLocalStorage();
    const store = { id: 'lock' };

    als.run(store, () => {
      navigator.locks
        .request('als-context-test', async () => {
          assert.strictEqual(als.getStore(), store);
        })
        .then(common.mustCall());
    });
  });

  it('should clean up when worker is terminated with a pending lock', async () => {
    // Acquire the lock in the main thread so that the worker's request will be pending
    await navigator.locks.request('cleanup-test', async () => {
      // Launch a worker that requests the same lock
      const worker = new Worker(`
        const { parentPort } = require('worker_threads');
        
        parentPort.postMessage({ requesting: true });
        
        navigator.locks.request('cleanup-test', async () => {
          return 'should-not-complete';
        }).catch(err => {
          parentPort.postMessage({ error: err.name });
        });
      `, { eval: true });

      const requestSignal = await new Promise((resolve) => {
        worker.once('message', resolve);
      });

      assert.strictEqual(requestSignal.requesting, true);

      await worker.terminate();

    });

    // Request the lock again to make sure cleanup succeeded
    await navigator.locks.request('cleanup-test', async (lock) => {
      assert.strictEqual(lock.name, 'cleanup-test');
    });
  });
});
