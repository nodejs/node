'use strict';

require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const { Worker } = require('worker_threads');

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
});
