'use strict';

const common = require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

function subscribe({ start, grant, miss, end }) {
  if (start) dc.subscribe('locks.request.start', start);
  if (grant) dc.subscribe('locks.request.grant', grant);
  if (miss) dc.subscribe('locks.request.miss', miss);
  if (end) dc.subscribe('locks.request.end', end);

  return () => {
    if (start) dc.unsubscribe('locks.request.start', start);
    if (grant) dc.unsubscribe('locks.request.grant', grant);
    if (miss) dc.unsubscribe('locks.request.miss', miss);
    if (end) dc.unsubscribe('locks.request.end', end);
  };
}

describe('Web Locks diagnostics channel', () => {
  it('emits start, grant, and end on success', async () => {
    let startEvent;
    const unsubscribe = subscribe({
      start: common.mustCall((e) => startEvent = e),
      grant: common.mustCall(),
      miss: common.mustNotCall(),
      end: common.mustCall(),
    });

    try {
      const result = await navigator.locks.request('normal-lock', async () => 'done');
      assert.strictEqual(result, 'done');
      assert.strictEqual(startEvent.name, 'normal-lock');
      assert.strictEqual(startEvent.mode, 'exclusive');
    } finally {
      unsubscribe();
    }
  });

  it('emits start, miss, and end when lock is unavailable', async () => {
    await navigator.locks.request('ifavailable-true-lock', common.mustCall(async () => {
      let startEvent;
      const unsubscribe = subscribe({
        start: common.mustCall((e) => startEvent = e),
        grant: common.mustNotCall(),
        miss: common.mustCall(),
        end: common.mustCall(),
      });

      try {
        const result = await navigator.locks.request(
          'ifavailable-true-lock',
          { ifAvailable: true },
          (lock) => lock,
        );

        assert.strictEqual(result, null);
        assert.strictEqual(startEvent.name, 'ifavailable-true-lock');
        assert.strictEqual(startEvent.mode, 'exclusive');
      } finally {
        unsubscribe();
      }
    }));
  });

  it('queued lock request emits start, grant, and end without miss', async () => {
    // Outer fires first, inner is queued — so events arrive in insertion order
    let outerStartEvent, innerStartEvent;
    const unsubscribe = subscribe({
      start: common.mustCall((e) => (outerStartEvent ? innerStartEvent = e : outerStartEvent = e), 2),
      grant: common.mustCall(2),
      miss: common.mustNotCall(),
      end: common.mustCall(2),
    });

    try {
      let innerDone;

      const outerResult = await navigator.locks.request('ifavailable-false-lock', common.mustCall(async () => {
        innerDone = navigator.locks.request(
          'ifavailable-false-lock',
          { ifAvailable: false },
          common.mustCall(async (lock) => {
            assert.ok(lock);
            return 'inner-done';
          }),
        );
        await new Promise((resolve) => setTimeout(resolve, 20));
        return 'outer-done';
      }));

      assert.strictEqual(outerResult, 'outer-done');
      assert.strictEqual(await innerDone, 'inner-done');

      assert.strictEqual(outerStartEvent.name, 'ifavailable-false-lock');
      assert.strictEqual(outerStartEvent.mode, 'exclusive');
      assert.strictEqual(innerStartEvent.name, 'ifavailable-false-lock');
      assert.strictEqual(innerStartEvent.mode, 'exclusive');
    } finally {
      unsubscribe();
    }
  });

  it('reports callback error in end event', async () => {
    const expectedError = new Error('Callback error');
    let endEvent;
    const unsubscribe = subscribe({
      start: common.mustCall(),
      grant: common.mustCall(),
      miss: common.mustNotCall(),
      end: common.mustCall((e) => endEvent = e),
    });

    try {
      await assert.rejects(
        navigator.locks.request('error-lock', async () => { throw expectedError; }),
        (error) => error === expectedError,
      );

      assert.strictEqual(endEvent.name, 'error-lock');
      assert.strictEqual(endEvent.mode, 'exclusive');
      assert.strictEqual(endEvent.error, expectedError);
    } finally {
      unsubscribe();
    }
  });

  it('stolen lock ends original request with AbortError', async () => {
    let stolenEndEvent, stealerEndEvent;
    const unsubscribe = subscribe({
      start: common.mustCall(2),
      grant: common.mustCall(2),
      miss: common.mustNotCall(),
      end: common.mustCall((e) => (e.steal ? stealerEndEvent = e : stolenEndEvent = e), 2),
    });

    try {
      let resolveGranted;
      const granted = new Promise((r) => { resolveGranted = r; });

      const originalRejected = assert.rejects(
        navigator.locks.request('steal-lock', async () => {
          resolveGranted();
          await new Promise((r) => setTimeout(r, 200));
        }),
        { name: 'AbortError' },
      );

      await granted;
      await navigator.locks.request('steal-lock', { steal: true }, async () => {});
      await originalRejected;

      assert.strictEqual(stolenEndEvent.name, 'steal-lock');
      assert.strictEqual(stolenEndEvent.mode, 'exclusive');
      assert.strictEqual(stolenEndEvent.steal, false);
      assert.strictEqual(stealerEndEvent.name, 'steal-lock');
      assert.strictEqual(stealerEndEvent.mode, 'exclusive');
      assert.strictEqual(stealerEndEvent.steal, true);
      assert.strictEqual(stolenEndEvent.error.name, 'AbortError');
      assert.strictEqual(stealerEndEvent.error, undefined);
    } finally {
      unsubscribe();
    }
  });
});
