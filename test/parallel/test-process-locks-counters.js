'use strict';

require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const { Worker, locks } = require('node:worker_threads');

function totalAcquired(snapshot) {
  return snapshot.totalExclusiveAcquired + snapshot.totalSharedAcquired;
}

function holdersTotal(snapshot) {
  return snapshot.holdersExclusive + snapshot.holdersShared;
}

function pendingTotal(snapshot) {
  return snapshot.pendingExclusive + snapshot.pendingShared;
}

describe('process.locksCounters()', () => {
  it('should return valid counters structure with correct types', async () => {
    assert.strictEqual(typeof process.locksCounters, 'function');

    const snapshot = process.locksCounters();

    // BigInt counters
    assert.strictEqual(typeof snapshot.totalAborts, 'bigint');
    assert(snapshot.totalAborts >= 0n);
    assert.strictEqual(typeof snapshot.totalSteals, 'bigint');
    assert(snapshot.totalSteals >= 0n);
    assert.strictEqual(typeof snapshot.totalExclusiveAcquired, 'bigint');
    assert(snapshot.totalExclusiveAcquired >= 0n);
    assert.strictEqual(typeof snapshot.totalSharedAcquired, 'bigint');
    assert(snapshot.totalSharedAcquired >= 0n);

    // Number counters
    assert.strictEqual(typeof snapshot.holdersExclusive, 'number');
    assert(snapshot.holdersExclusive >= 0);
    assert.strictEqual(typeof snapshot.holdersShared, 'number');
    assert(snapshot.holdersShared >= 0);
    assert.strictEqual(typeof snapshot.pendingExclusive, 'number');
    assert(snapshot.pendingExclusive >= 0);
    assert.strictEqual(typeof snapshot.pendingShared, 'number');
    assert(snapshot.pendingShared >= 0);

    const totalAcq = totalAcquired(snapshot);
    assert.strictEqual(typeof totalAcq, 'bigint');
    assert.strictEqual(totalAcq, snapshot.totalExclusiveAcquired + snapshot.totalSharedAcquired);
  });

  it('should increment totalExclusiveAcquired and track holders', async () => {
    const resource = 'test-exclusive-acquire';
    const before = process.locksCounters();

    await locks.request(resource, async (lock) => {
      assert.strictEqual(lock.mode, 'exclusive');
      const current = process.locksCounters();

      assert.strictEqual(totalAcquired(current), totalAcquired(before) + 1n);
      assert.strictEqual(current.totalExclusiveAcquired, before.totalExclusiveAcquired + 1n);

      assert.strictEqual(current.holdersExclusive, before.holdersExclusive + 1);
      assert.strictEqual(holdersTotal(current), holdersTotal(before) + 1);
    });

    const after = process.locksCounters();
    assert.strictEqual(after.totalExclusiveAcquired, before.totalExclusiveAcquired + 1n);
    assert.strictEqual(after.holdersExclusive, before.holdersExclusive);
  });

  it('should track multiple shared locks held simultaneously', async () => {
    const resource = 'test-multiple-shared';
    const before = process.locksCounters();

    await locks.request(resource, { mode: 'shared' }, async () => {
      const afterFirst = process.locksCounters();
      assert.strictEqual(afterFirst.holdersShared, before.holdersShared + 1);

      await locks.request(resource, { mode: 'shared' }, async () => {
        const afterSecond = process.locksCounters();

        assert.strictEqual(afterSecond.holdersShared, before.holdersShared + 2);
        assert.strictEqual(afterSecond.totalSharedAcquired, before.totalSharedAcquired + 2n);
      });
    });

    const after = process.locksCounters();
    assert.strictEqual(after.totalSharedAcquired, before.totalSharedAcquired + 2n);
    assert.strictEqual(after.holdersShared, before.holdersShared);
  });

  it('should track pending lock requests', async () => {
    const resource = 'test-pending-lock';
    const before = process.locksCounters();

    const firstLock = locks.request(resource, async () => {
      return 'first-lock';
    });

    const secondLock = locks.request(resource, async () => {
      return 'second-lock';
    });

    const current = process.locksCounters();

    // Should have one holder and one pending
    assert(current.holdersExclusive >= before.holdersExclusive + 1);
    assert(current.pendingExclusive >= before.pendingExclusive + 1);
    assert(pendingTotal(current) >= pendingTotal(before) + 1);

    // Release the locks
    await firstLock;
    await secondLock;

    const after = process.locksCounters();
    assert.strictEqual(after.totalExclusiveAcquired, before.totalExclusiveAcquired + 2n);
  });

  it('should increment totalAborts when ifAvailable fails', async () => {
    const resource = 'test-ifavailable-abort';
    const before = process.locksCounters();

    await locks.request(resource, async () => {
      // Try to acquire with ifAvailable while lock is held
      const result = await locks.request(resource, { ifAvailable: true }, (lock) => {
        return lock;
      });

      assert.strictEqual(result, null);

      const after = process.locksCounters();
      assert.strictEqual(after.totalAborts, before.totalAborts + 1n);
    });
  });

  it('should increment totalAborts when callback rejects', async () => {
    const resource = 'test-callback-reject-abort';
    const before = process.locksCounters();

    await locks.request(resource, async () => {
      throw new Error('Callback rejection');
    }).catch(() => {
      // Expected rejection
    });

    const after = process.locksCounters();
    assert.strictEqual(after.totalExclusiveAcquired, before.totalExclusiveAcquired + 1n);
    assert.strictEqual(after.totalAborts, before.totalAborts + 1n);
  });

  it('should increment totalSteals when stealing a lock', async () => {
    const resource = 'test-steal-counter';
    const before = process.locksCounters();

    const originalPromise = locks.request(resource, async () => {
      await new Promise((resolve) => setTimeout(resolve, 100));
      return 'original';
    }).catch((err) => {
      assert.strictEqual(err.name, 'AbortError');
      return 'stolen';
    });

    const stealResult = await locks.request(resource, { steal: true }, async (lock) => {
      assert.strictEqual(lock.mode, 'exclusive');
      return 'completed';
    });

    assert.strictEqual(stealResult, 'completed');
    assert.strictEqual(await originalPromise, 'stolen');

    const after = process.locksCounters();
    assert.strictEqual(after.totalSteals, before.totalSteals + 1n);
    assert.strictEqual(after.totalExclusiveAcquired, before.totalExclusiveAcquired + 2n);
  });

  it('should handle counters correctly across worker threads', async () => {
    const resource = 'test-cross-thread';
    const before = process.locksCounters();

    const worker = new Worker(`
      const { parentPort, locks } = require('worker_threads');

      locks.request('test-cross-thread', async () => {
        // Lock acquired and released
      }).then(() => {
        parentPort.postMessage('done');
      });
    `, { eval: true });

    await new Promise((resolve) => {
      worker.once('message', resolve);
    });

    await worker.terminate();

    await locks.request(resource, async (lock) => {
      assert.strictEqual(lock.mode, 'exclusive');
    });

    const after = process.locksCounters();
    assert.strictEqual(totalAcquired(after), totalAcquired(before) + 2n);
    assert.strictEqual(after.totalExclusiveAcquired, before.totalExclusiveAcquired + 2n);
  });

  it('should accumulate counters correctly over many acquisitions', async () => {
    const resource = 'test-accumulation';
    const before = process.locksCounters();
    const iterations = 1_000;

    for (let i = 0; i < iterations; i++) {
      await locks.request(resource, { mode: i % 2 === 0 ? 'exclusive' : 'shared' }, async () => {
        // Do nothing, just acquire the lock
      });
    }

    const after = process.locksCounters();

    const totalAcquired = after.totalExclusiveAcquired + after.totalSharedAcquired;
    const beforeTotal = before.totalExclusiveAcquired + before.totalSharedAcquired;

    assert.strictEqual(totalAcquired, beforeTotal + BigInt(iterations));

    assert(after.totalExclusiveAcquired > before.totalExclusiveAcquired);
    assert(after.totalSharedAcquired > before.totalSharedAcquired);
  });
});
