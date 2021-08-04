'use strict';

const common = require('../common');
const assert = require('assert');
const {
  PerformanceObserver,
  PerformanceEntry,
  PerformanceMark,
  performance: {
    nodeTiming,
    mark,
    measure,
    clearMarks,
  },
} = require('perf_hooks');

assert(PerformanceObserver);
assert(PerformanceEntry);
assert(PerformanceMark);
assert(mark);
assert(measure);

[undefined, 'a', 'null', 1, true].forEach((i) => {
  const m = mark(i);
  assert(m instanceof PerformanceEntry);
  assert(m instanceof PerformanceMark);

  assert.strictEqual(m.name, `${i}`);
  assert.strictEqual(m.entryType, 'mark');
  assert.strictEqual(typeof m.startTime, 'number');
  assert.strictEqual(m.duration, 0);
  assert.strictEqual(m.detail, null);
});

clearMarks();

assert.throws(() => mark(Symbol('a')), {
  message: /Cannot convert a Symbol value to a string/
});

[undefined, null].forEach((detail) => {
  const m = mark('a', { detail });
  assert.strictEqual(m.name, 'a');
  assert.strictEqual(m.entryType, 'mark');
  assert.deepStrictEqual(m.detail, null);
});
[1, 'any', {}, []].forEach((detail) => {
  const m = mark('a', { detail });
  assert.strictEqual(m.name, 'a');
  assert.strictEqual(m.entryType, 'mark');
  // Value of detail is structured cloned.
  assert.deepStrictEqual(m.detail, detail);
});

clearMarks();

{
  const m = mark('a', { startTime: 1 });
  assert.strictEqual(m.startTime, 1);
}

assert.throws(() => mark('a', { startTime: 'a' }), {
  code: 'ERR_INVALID_ARG_TYPE'
});

clearMarks();
clearMarks(1);
clearMarks(null);

assert.throws(() => clearMarks(Symbol('foo')), {
  message: /Cannot convert a Symbol value to a string/
});

{
  mark('a', { startTime: 0 });
  mark('b', { startTime: 10 });

  {
    const m3 = measure('foo', 'a', 'b');
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, 0);
    assert.strictEqual(m3.duration, 10);
  }

  {
    const m3 = measure('foo', 'a');
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, 0);
    assert(m3.duration > 0);  // Duration is non-deterministic here.
  }

  {
    const m3 = measure('foo', { start: 'a' });
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, 0);
    assert(m3.duration > 0);  // Duration is non-deterministic here.
  }

  {
    const m3 = measure('foo', { end: 'b' });
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, 0);
    assert.strictEqual(m3.duration, 10);
  }

  {
    const m3 = measure('foo', { duration: 11, end: 'b' });
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, -1);
    assert.strictEqual(m3.duration, 11);
  }

  {
    const m3 = measure('foo', { duration: 11, start: 'b' });
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, 10);
    assert.strictEqual(m3.duration, 11);
  }

  {
    const m3 = measure('foo', 'nodeStart');
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, nodeTiming.nodeStart);
    assert(m3.duration > 0);  // Duration is non-deterministic here.
  }

  {
    const m3 = measure('foo', 'nodeStart', 'bootstrapComplete');
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, nodeTiming.nodeStart);
    assert.strictEqual(
      m3.duration,
      nodeTiming.bootstrapComplete - nodeTiming.nodeStart);
  }

  {
    const m3 = measure('foo', { start: 'nodeStart', duration: 10 });
    assert.strictEqual(m3.name, 'foo');
    assert.strictEqual(m3.entryType, 'measure');
    assert.strictEqual(m3.startTime, nodeTiming.nodeStart);
    assert.strictEqual(m3.duration, 10);
  }

  clearMarks();
}

{
  const obs = new PerformanceObserver(common.mustCall((list) => {
    {
      const entries = list.getEntries();
      assert.strictEqual(entries.length, 3);
    }
    {
      const entries = list.getEntriesByType('mark');
      assert.strictEqual(entries.length, 2);
    }
    {
      const entries = list.getEntriesByType('measure');
      assert.strictEqual(entries.length, 1);
    }
    {
      const entries = list.getEntriesByName('a');
      assert.strictEqual(entries.length, 1);
    }
    {
      const entries = list.getEntriesByName('b');
      assert.strictEqual(entries.length, 1);
    }
    {
      const entries = list.getEntriesByName('a to b');
      assert.strictEqual(entries.length, 1);
    }
    obs.disconnect();
  }));
  obs.observe({ entryTypes: ['mark', 'measure'] });
  mark('a');
  mark('b');
  measure('a to b', 'a', 'b');
}
