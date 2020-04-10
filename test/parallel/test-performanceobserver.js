// Flags: --expose-internals
'use strict';

const common = require('../common');
const Countdown = require('../common/countdown');
const assert = require('assert');
const { inspect } = require('util');
const { internalBinding } = require('internal/test/binding');
const {
  observerCounts: counts
} = internalBinding('performance');
const {
  performance,
  PerformanceObserver,
  constants
} = require('perf_hooks');

const {
  NODE_PERFORMANCE_ENTRY_TYPE_NODE,
  NODE_PERFORMANCE_ENTRY_TYPE_MARK,
  NODE_PERFORMANCE_ENTRY_TYPE_MEASURE,
  NODE_PERFORMANCE_ENTRY_TYPE_GC,
  NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION,
} = constants;

assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_NODE], 0);
assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);
assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MEASURE], 0);
assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_GC], 0);
assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_FUNCTION], 0);

{
  [1, null, undefined, {}, [], Infinity].forEach((i) => {
    assert.throws(
      () => new PerformanceObserver(i),
      {
        code: 'ERR_INVALID_CALLBACK',
        name: 'TypeError',
        message: `Callback must be a function. Received ${inspect(i)}`
      }
    );
  });
  const observer = new PerformanceObserver(common.mustNotCall());

  [1, null, undefined].forEach((input) => {
    assert.throws(
      () => observer.observe(input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "options" argument must be of type object.' +
                 common.invalidArgTypeHelper(input)
      });
  });

  [1, undefined, null, {}, Infinity].forEach((i) => {
    assert.throws(() => observer.observe({ entryTypes: i }),
                  {
                    code: 'ERR_INVALID_OPT_VALUE',
                    name: 'TypeError',
                    message: 'The value "[object Object]" is invalid ' +
                                   'for option "entryTypes"'
                  });
  });

  const obs = new PerformanceObserver(common.mustNotCall());
  obs.observe({ entryTypes: ['mark', 'mark'] });
  obs.disconnect();
  performance.mark('42');
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);
}

// Test Non-Buffered
{
  const observer =
    new PerformanceObserver(common.mustCall(callback, 3));

  const countdown =
    new Countdown(3, () => {
      observer.disconnect();
      assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);
    });

  function callback(list, obs) {
    assert.strictEqual(obs, observer);
    const entries = list.getEntries();
    assert.strictEqual(entries.length, 1);
    countdown.dec();
  }
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_NODE], 0);
  observer.observe({ entryTypes: ['mark', 'node'] });
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 1);
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_NODE], 1);
  performance.mark('test1');
  performance.mark('test2');
  performance.mark('test3');
}


// Test Buffered
{
  const observer =
    new PerformanceObserver(common.mustCall(callback, 1));
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);

  function callback(list, obs) {
    assert.strictEqual(obs, observer);
    const entries = list.getEntries();
    assert.strictEqual(entries.length, 3);
    observer.disconnect();
    assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 0);

    {
      const entriesByName = list.getEntriesByName('test1');
      assert.strictEqual(entriesByName.length, 1);
      assert.strictEqual(entriesByName[0].name, 'test1');
      assert.strictEqual(entriesByName[0].entryType, 'mark');
    }

    {
      const entriesByName = list.getEntriesByName('test1', 'mark');
      assert.strictEqual(entriesByName.length, 1);
      assert.strictEqual(entriesByName[0].name, 'test1');
      assert.strictEqual(entriesByName[0].entryType, 'mark');
    }

    {
      const entriesByName = list.getEntriesByName('test1', 'measure');
      assert.strictEqual(entriesByName.length, 0);
    }

    {
      const entriesByType = list.getEntriesByType('measure');
      assert.strictEqual(entriesByType.length, 1);
      assert.strictEqual(entriesByType[0].name, 'test3');
      assert.strictEqual(entriesByType[0].entryType, 'measure');
    }
  }

  observer.observe({ entryTypes: ['mark', 'measure'], buffered: true });
  // Do this twice to make sure it doesn't throw
  observer.observe({ entryTypes: ['mark', 'measure'], buffered: true });
  // Even tho we called twice, count should be 1
  assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_MARK], 1);
  performance.mark('test1');
  performance.mark('test2');
  performance.measure('test3', 'test1', 'test2');
}
