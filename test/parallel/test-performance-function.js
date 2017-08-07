'use strict';

const common = require('../common');
const assert = require('assert');

const {
  performance,
  PerformanceObserver
} = require('perf_hooks');

{
  // Intentional non-op. Do not wrap in common.mustCall();
  const n = performance.timerify(() => {});
  n();
  const entries = performance.getEntriesByType('function');
  assert.strictEqual(entries.length, 0);

  const obs = new PerformanceObserver(common.mustCall((list) => {
    const entries = list.getEntries();
    const entry = entries[0];
    assert(entry);
    assert.strictEqual(entry.name, 'performance.timerify');
    assert.strictEqual(entry.entryType, 'function');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(typeof entry.startTime, 'number');
    obs.disconnect();
    performance.clearFunctions();
  }));
  obs.observe({ entryTypes: ['function'] });
  n();
}

{
  // If the error throws, the error should just be bubbled up and the
  // performance timeline entry will not be reported.
  const obs = new PerformanceObserver(common.mustNotCall());
  obs.observe({ entryTypes: ['function'] });
  const n = performance.timerify(() => {
    throw new Error('test');
  });
  assert.throws(() => n(), /^Error: test$/);
  const entries = performance.getEntriesByType('function');
  assert.strictEqual(entries.length, 0);
  obs.disconnect();
}

{
  class N {}
  const n = performance.timerify(N);
  new n();
  const entries = performance.getEntriesByType('function');
  assert.strictEqual(entries.length, 0);

  const obs = new PerformanceObserver(common.mustCall((list) => {
    const entries = list.getEntries();
    const entry = entries[0];
    assert.strictEqual(entry[0], 1);
    assert.strictEqual(entry[1], 'abc');
    assert(entry);
    assert.strictEqual(entry.name, 'N');
    assert.strictEqual(entry.entryType, 'function');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(typeof entry.startTime, 'number');
    obs.disconnect();
    performance.clearFunctions();
  }));
  obs.observe({ entryTypes: ['function'] });

  new n(1, 'abc');
}

{
  [1, {}, [], null, undefined, Infinity].forEach((i) => {
    assert.throws(() => performance.timerify(i),
                  common.expectsError({
                    code: 'ERR_INVALID_ARG_TYPE',
                    type: TypeError,
                    message: 'The "fn" argument must be of type Function'
                  }));
  });
}

// Function can only be wrapped once, also check length and name
{
  const m = (a, b = 1) => {};
  const n = performance.timerify(m);
  const o = performance.timerify(m);
  const p = performance.timerify(n);
  assert.strictEqual(n, o);
  assert.strictEqual(n, p);
  assert.strictEqual(n.length, m.length);
  assert.strictEqual(n.name, 'timerified m');
}
