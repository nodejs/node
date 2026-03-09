import { describe, test } from 'node:test';
import assert from 'node:assert/strict';

// Helper to capture execution order
function makeTracker() {
  const log = [];
  return {
    log,
    track: (label) => log.push(label),
    assertOrder: (...expected) => assert.deepEqual(log, expected),
  };
}

describe('before hook cleanup function', () => {

  test('cleanup returned from before() is called after the test', async (t) => {
    const tracker = makeTracker();

    await t.test('run test with cleanup', async (t) => {
      t.before(() => {
        tracker.track('before');
        return () => tracker.track('before-cleanup');
      });

      t.after(() => tracker.track('after'));

      tracker.track('test-body');
    });

    // After the nested test completes, verify cleanup ran
    await t.test('verify cleanup ran', () => {
      tracker.assertOrder('before', 'test-body', 'after', 'before-cleanup');
    });
  });

  test('cleanup returned from before() runs after explicit t.after() hooks', async (t) => {
    const tracker = makeTracker();

    await t.test('run test with cleanup and after hook', async (t) => {
      t.before(() => {
        tracker.track('before');
        return () => tracker.track('before-cleanup');
      });

      t.after(() => tracker.track('after'));

      tracker.track('test-body');
    });

    // Verify cleanup ran after the after hook
    await t.test('verify order', () => {
      tracker.assertOrder('before', 'test-body', 'after', 'before-cleanup');
    });
  });

  test('multiple before() cleanups run in LIFO order', async (t) => {
    const tracker = makeTracker();

    await t.test('run test with multiple before hooks', async (t) => {
      t.before(() => {
        tracker.track('before-1');
        return () => tracker.track('cleanup-1');
      });

      t.before(() => {
        tracker.track('before-2');
        return () => tracker.track('cleanup-2');
      });

      tracker.track('test-body');
    });

    // Verify cleanups ran in LIFO order
    await t.test('verify LIFO order', () => {
      tracker.assertOrder('before-1', 'before-2', 'test-body', 'cleanup-2', 'cleanup-1');
    });
  });

  test('before() without a return value does not register a cleanup', async (t) => {
    const tracker = makeTracker();

    await t.test('run test without cleanup', async (t) => {
      t.before(() => {
        tracker.track('before');
        // no return value
      });

      t.after(() => tracker.track('after'));

      tracker.track('test-body');
    });

    // Verify only before, test-body, and after ran (no cleanup)
    await t.test('verify no cleanup', () => {
      tracker.assertOrder('before', 'test-body', 'after');
    });
  });

  test('async cleanup function from before() works correctly', async (t) => {
    const tracker = makeTracker();

    await t.test('run test with async cleanup', async (t) => {
      // Synchronous before hook that returns async cleanup
      t.before(() => {
        tracker.track('before');
        return async () => {
          await Promise.resolve();
          tracker.track('async-cleanup');
        };
      });

      tracker.track('test-body');
    });

    // Verify async cleanup ran
    await t.test('verify async cleanup ran', () => {
      tracker.assertOrder('before', 'test-body', 'async-cleanup');
    });
  });

  test('cleanup from before() runs even if the test body throws', async (t) => {
    const tracker = makeTracker();

    await t.test('run test that throws', async (t) => {
      t.before(() => {
        tracker.track('before');
        return () => tracker.track('before-cleanup');
      });

      try {
        throw new Error('test error');
      } catch {
        tracker.track('caught-error');
      }
    });

    // Verify cleanup still ran even though test threw
    await t.test('verify cleanup after error', () => {
      tracker.assertOrder('before', 'caught-error', 'before-cleanup');
    });
  });

});
