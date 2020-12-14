'use strict';
const { mustCall, mustNotCall } = require('../common');
const assert = require('assert');
const { locks, Worker } = require('worker_threads');
const { EventEmitter, once, getEventListeners } = require('events');

const { setImmediate: tick } = require('timers/promises');

(async () => {
  {
    // Acquiring a lock and releasing it works.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    await locks.request('lock-name', mustCall(async (lock) => {
      const snapshot = await locks.query();
      assert.strictEqual(snapshot.pending.length, 0);
      assert.strictEqual(snapshot.held.length, 1);
      assert.strictEqual(snapshot.held[0].name, 'lock-name');
      assert.strictEqual(snapshot.held[0].mode, 'exclusive');
      assert.strictEqual(lock.name, 'lock-name');
      assert.strictEqual(lock.mode, 'exclusive');
    }));
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Acquiring a lock will make other lock requests for the same name wait.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    let firstRequestDone = false;
    let secondRequest;
    let secondRequestDone = false;
    await locks.request('lock-name', mustCall(async (lock) => {
      secondRequest = locks.request('lock-name', mustCall(async (lock) => {
        assert(firstRequestDone);
        secondRequestDone = true;
        const snapshot = await locks.query();
        assert.strictEqual(snapshot.pending.length, 0);
        assert.strictEqual(snapshot.held.length, 1);
      }));
      const snapshot = await locks.query();
      assert.strictEqual(snapshot.pending.length, 1);
      assert.strictEqual(snapshot.held.length, 1);
      assert.strictEqual(secondRequestDone, false);
      await tick();
      assert.strictEqual(secondRequestDone, false);
      firstRequestDone = true;
    }));
    await secondRequest;
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Acquiring a shared lock will not make other shared lock requests for the
    // same name wait.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    await locks.request('lock-name', {
      mode: 'shared'
    }, mustCall(async (lock) => {
      await locks.request('lock-name', {
        mode: 'shared'
      }, mustCall(async (lock) => {
        const snapshot = await locks.query();
        assert.strictEqual(snapshot.pending.length, 0);
        assert.strictEqual(snapshot.held.length, 2);
      }));
    }));
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Acquiring a shared lock will make exclusive lock requests for the same
    // name wait.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    let firstRequestDone = false;
    let secondRequest;
    let secondRequestDone = false;
    await locks.request('lock-name', {
      mode: 'shared'
    }, mustCall(async (lock) => {
      secondRequest = locks.request('lock-name', mustCall(async (lock) => {
        assert(firstRequestDone);
        secondRequestDone = true;
        const snapshot = await locks.query();
        assert.strictEqual(snapshot.pending.length, 0);
        assert.strictEqual(snapshot.held.length, 1);
      }));
      const snapshot = await locks.query();
      assert.strictEqual(snapshot.pending.length, 1);
      assert.strictEqual(snapshot.held.length, 1);
      assert.strictEqual(secondRequestDone, false);
      await tick();
      assert.strictEqual(secondRequestDone, false);
      firstRequestDone = true;
    }));
    await secondRequest;
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Lock requests can be aborted.
    const ee = new EventEmitter();
    const locked = locks.request('lock-name', mustCall(async (lock) => {
      await once(ee, 'stop');
    }));
    const controller = new AbortController();
    const cantlock = locks.request('lock-name', {
      signal: controller.signal
    }, mustNotCall());
    await tick();
    assert.strictEqual((await locks.query()).held.length, 1);
    assert.strictEqual((await locks.query()).pending.length, 1);
    controller.abort();
    await assert.rejects(cantlock, { name: 'AbortError' });
    ee.emit('stop');
    await locked;
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Aborting already-acquired locks has no effect.
    const controller = new AbortController();
    await locks.request('lock-name', {
      signal: controller.signal
    }, mustCall(async (lock) => {
      controller.abort();
    }));
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    assert.deepStrictEqual(getEventListeners(controller.signal, 'abort'), []);
  }

  {
    // ifAvailable will make a second request 'fail'.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    await locks.request('lock-name', mustCall(async (lock) => {
      assert.strictEqual(lock.name, 'lock-name');
      await locks.request('lock-name', {
        ifAvailable: true
      }, mustCall(async (lock) => {
        assert.strictEqual(lock, null);
      }));
    }));
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Locks are cross-thread.
    const workerData = new Uint8Array(new SharedArrayBuffer(1));
    let w;
    await locks.request('lock-name', mustCall(async (lock) => {
      w = new Worker(`
        const { locks, workerData, parentPort } = require('worker_threads');
        const assert = require('assert');
        locks.request('lock-name', async (lock) => {
          assert.strictEqual(lock.name, 'lock-name');
          Atomics.add(workerData, 0, 1);
        }).then(() => Atomics.add(workerData, 0, 1));
        Atomics.add(workerData, 0, 1);
        parentPort.postMessage('up');
      `, { eval: true, workerData });
      await once(w, 'message');
      assert.strictEqual(workerData[0], 1);
      await tick();
      assert.strictEqual(workerData[0], 1);
    }));
    await once(w, 'exit');
    assert.strictEqual(workerData[0], 3);
  }

  {
    // Locks held at thread exit do not end up blocking new acquire calls.
    const w = new Worker(`
      const { locks, workerData, parentPort } = require('worker_threads');
      locks.request('lock-name', async() => process.exit(42));
    `, { eval: true });
    const [ code ] = await once(w, 'exit');
    assert.strictEqual(code, 42);
    await locks.request('lock-name', mustCall(async (lock) => {
      assert.strictEqual(lock.name, 'lock-name');
    }));
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }

  {
    // Locks can be stolen.
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
    let inner;
    const outer = locks.request('lock-name', mustCall(async (lock) => {
      inner = locks.request('lock-name', {
        steal: true
      }, mustCall(async (lock) => {
        const snapshot = await locks.query();
        assert.strictEqual(snapshot.pending.length, 0);
        assert.strictEqual(snapshot.held.length, 1);
      }));
    }));
    await assert.rejects(outer, { name: 'AbortError' });
    await inner;
    assert.deepStrictEqual(await locks.query(), { held: [], pending: [] });
  }
})().then(mustCall());
