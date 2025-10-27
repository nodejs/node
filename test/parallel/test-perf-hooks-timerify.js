'use strict';

const common = require('../common');
const assert = require('assert');

const {
  createHistogram,
  timerify,
  performance,
  PerformanceObserver
} = require('perf_hooks');

const {
  setTimeout: sleep
} = require('timers/promises');

// Verifies that `performance.timerify` is an alias of `perf_hooks.timerify`.
assert.strictEqual(performance.timerify, timerify);

{
  // Intentional non-op. Do not wrap in common.mustCall();
  const n = timerify(function noop() {});

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
  const n = timerify(() => {
    throw new Error('test');
  });
  assert.throws(() => n(), /^Error: test$/);
  obs.disconnect();
}

{
  class N {}
  const n = timerify(N);

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
    assert.throws(() => timerify(input),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    name: 'TypeError',
                    message: /The "fn" argument must be of type function/
                  });
  });
}

// Function can be wrapped many times, also check length and name
{
  const m = (a, b = 1) => {};
  const n = timerify(m);
  const o = timerify(m);
  const p = timerify(n);
  assert.notStrictEqual(n, o);
  assert.notStrictEqual(n, p);
  assert.notStrictEqual(o, p);
  assert.strictEqual(n.length, m.length);
  assert.strictEqual(n.name, 'timerified m');
  assert.strictEqual(p.name, 'timerified timerified m');
}

(async () => {
  let _deadCode;

  const histogram = createHistogram();
  const m = (a, b = 1) => {
    for (let i = 0; i < 1e3; i++)
      _deadCode = i;
  };
  const n = timerify(m, { histogram });
  assert.strictEqual(histogram.max, 0);
  for (let i = 0; i < 10; i++) {
    n();
    await sleep(10);
  }
  assert.ok(_deadCode >= 0);
  assert.notStrictEqual(histogram.max, 0);
  [1, '', {}, [], false].forEach((histogram) => {
    assert.throws(() => timerify(m, { histogram }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
})().then(common.mustCall());

(async () => {
  const histogram = createHistogram();
  const m = async (a, b = 1) => {
    await sleep(10);
  };
  const n = timerify(m, { histogram });
  assert.strictEqual(histogram.max, 0);
  for (let i = 0; i < 10; i++) {
    await n();
  }
  assert.notStrictEqual(histogram.max, 0);
  [1, '', {}, [], false].forEach((histogram) => {
    assert.throws(() => timerify(m, { histogram }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
})().then(common.mustCall());

// Regression tests for https://github.com/nodejs/node/issues/40623
{
  assert.strictEqual(timerify(function func() {
    return 1;
  })(), 1);
  assert.strictEqual(timerify(function() {
    return 1;
  })(), 1);
  assert.strictEqual(timerify(() => {
    return 1;
  })(), 1);
  class C {}
  const wrap = timerify(C);
  assert.ok(new wrap() instanceof C);
  assert.throws(() => wrap(), {
    name: 'TypeError',
  });
}
