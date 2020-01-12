'use strict';

const common = require('../common');
const assert = require('assert');

const {
  performance,
  PerformanceObserver
} = require('perf_hooks');

{
  // Intentional non-op. Do not wrap in common.mustCall();
  const n = performance.timerify(function noop() {});

  const obs = new PerformanceObserver(common.mustCall((list) => {
    const entries = list.getEntries();
    const entry = entries[0];
    assert(entry);
    assert.strictEqual(entry.name, 'noop');
    assert.strictEqual(entry.entryType, 'function');
    assert.strictEqual(typeof entry.duration, 'number');
    assert.strictEqual(typeof entry.startTime, 'number');
    obs.disconnect();
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
  obs.disconnect();
}

{
  class N {}
  const n = performance.timerify(N);

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
  }));
  obs.observe({ entryTypes: ['function'] });

  new n(1, 'abc');
}

{
  [1, {}, [], null, undefined, Infinity].forEach((input) => {
    assert.throws(() => performance.timerify(input),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    name: 'TypeError',
                    message: /The "fn" argument must be of type function/
                  });
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
