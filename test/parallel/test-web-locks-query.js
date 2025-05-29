'use strict';

require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const { Worker } = require('worker_threads');

describe('Web Locks - query missing WPT tests', () => {
  it('should report different ids for held locks from different contexts', async () => {
    const worker = new Worker(`
      const { parentPort } = require('worker_threads');
      
      navigator.locks.request('different-contexts-resource', { mode: 'shared' }, async (lock) => {
        const state = await navigator.locks.query();
        const heldLocks = state.held.filter(l => l.name === 'different-contexts-resource');
        
        parentPort.postMessage({ clientId: heldLocks[0].clientId });
        
        await new Promise(resolve => {
          parentPort.once('message', () => resolve());
        });
      }).catch(err => parentPort.postMessage({ error: err.message }));
    `, { eval: true });

    const workerResult = await new Promise((resolve) => {
      worker.once('message', resolve);
    });

    await navigator.locks.request('different-contexts-resource', { mode: 'shared' }, async (lock) => {
      const state = await navigator.locks.query();
      const heldLocks = state.held.filter((l) => l.name === 'different-contexts-resource');

      const mainClientId = heldLocks[0].clientId;

      assert.notStrictEqual(mainClientId, workerResult.clientId);

      worker.postMessage('release');
    });

    await worker.terminate();
  });

  it('should observe a deadlock scenario', async () => {
    const worker = new Worker(`
      const { parentPort } = require('worker_threads');
      
      navigator.locks.request('deadlock-resource-1', async (lock1) => {
        parentPort.postMessage({ acquired: 'resource1' });
        
        await new Promise(resolve => {
          parentPort.once('message', () => resolve());
        });
        
        const result = await navigator.locks.request('deadlock-resource-2', 
          { ifAvailable: true }, (lock2) => lock2 !== null);
        
        parentPort.postMessage({ acquired2: result });
        
        await new Promise(resolve => {
          parentPort.once('message', () => resolve());
        });
      }).catch(err => parentPort.postMessage({ error: err.message }));
    `, { eval: true });

    const step1 = await new Promise((resolve) => {
      worker.once('message', resolve);
    });
    assert.strictEqual(step1.acquired, 'resource1');

    await navigator.locks.request('deadlock-resource-2', async (lock2) => {
      worker.postMessage('try-resource2');

      const step2 = await new Promise((resolve) => {
        worker.once('message', resolve);
      });
      assert.strictEqual(step2.acquired2, false);

      const canGetResource1 = await navigator.locks.request('deadlock-resource-1',
                                                            { ifAvailable: true }, (lock1) => lock1 !== null);

      assert.strictEqual(canGetResource1, false);

      const state = await navigator.locks.query();
      const resource2Lock = state.held.find((l) => l.name === 'deadlock-resource-2');
      assert(resource2Lock);

      worker.postMessage('release');
    });

    await worker.terminate();
  });
});
