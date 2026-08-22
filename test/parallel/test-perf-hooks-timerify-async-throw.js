// Regression test for https://github.com/nodejs/node/issues/42743
// Test that a timerified function which throws asynchronously (i.e. returns
// a rejected promise / thenable) behaves consistently with one that throws
// synchronously: no 'function' performance entry is produced, and no
// histogram record is added.

'use strict';

const common = require('../common');
const assert = require('assert');
const {
  createHistogram,
  performance,
  PerformanceObserver,
  timerify,
} = require('perf_hooks');

// 1. Sync vs async throw should record the same number of histogram samples.
{
  const h1 = createHistogram();
  const h2 = createHistogram();

  function syncThrow() { throw new Error('sync'); }

  async function asyncThrow() { throw new Error('async'); }

  const g1 = timerify(syncThrow, { histogram: h1 });
  const g2 = timerify(asyncThrow, { histogram: h2 });

  assert.throws(() => g1(), /^Error: sync$/);

  assert.rejects(g2(), /^Error: async$/).then(common.mustCall(() => {
    // Both histograms must agree: neither should have recorded a sample,
    // because the throwing async function should behave like the throwing
    // sync function.
    assert.strictEqual(h1.count, 0);
    assert.strictEqual(h2.count, 0);
    assert.strictEqual(h1.count, h2.count);
  }));
}

// 2. No 'function' PerformanceEntry should be produced for either path.
{
  const obs = new PerformanceObserver(common.mustNotCall(
    'no function entry should be enqueued for a throwing timerified fn',
  ));
  obs.observe({ entryTypes: ['function'] });

  const sync = timerify(() => { throw new Error('sync2'); });
  const async_ = timerify(async () => { throw new Error('async2'); });

  assert.throws(() => sync(), /^Error: sync2$/);
  assert.rejects(async_(), /^Error: async2$/).then(common.mustCall(() => {
    // Give the observer a tick to flush any (incorrectly) enqueued entries
    // before disconnecting.
    setImmediate(common.mustCall(() => obs.disconnect()));
  }));
}

// 3. A custom thenable that only implements `then` (no `finally`) and
// fulfills should still produce a performance entry and resolve to the
// original value. This guards against a regression where the wrapper
// relied on `result.finally`.
{
  const obs = new PerformanceObserver(common.mustCall((list, observer) => {
    const entries = list.getEntries();
    assert.strictEqual(entries.length, 1);
    const entry = entries[0];
    assert.strictEqual(entry.entryType, 'function');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(typeof entry.startTime, 'number');
    observer.disconnect();
  }));
  obs.observe({ entryTypes: ['function'] });

  function thenableOnly() {
    return {
      then(onFulfilled) {
        // Resolve asynchronously to mimic real promise semantics.
        setImmediate(() => onFulfilled('value'));
      },
    };
  }

  const wrapped = timerify(thenableOnly);
  const ret = wrapped();
  // The wrapper must return something thenable; `.then` must work and
  // the resolved value must be propagated unchanged.
  assert.strictEqual(typeof ret.then, 'function');
  ret.then(common.mustCall((value) => {
    assert.strictEqual(value, 'value');
  }));
}

// 4. Async fulfillment must still propagate the resolved value.
{
  const wrapped = timerify(async () => 42);
  wrapped().then(common.mustCall((value) => {
    assert.strictEqual(value, 42);
  }));
}

// Reference `performance` so the binding is exercised in this file too.
assert.strictEqual(performance.timerify, timerify);
