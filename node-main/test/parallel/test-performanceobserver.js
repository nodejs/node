// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
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
  NODE_PERFORMANCE_ENTRY_TYPE_GC,
  NODE_PERFORMANCE_ENTRY_TYPE_HTTP2,
} = constants;

assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_GC], 0);
assert.strictEqual(counts[NODE_PERFORMANCE_ENTRY_TYPE_HTTP2], 0);

{
  [1, null, undefined, {}, [], Infinity].forEach((i) => {
    assert.throws(
      () => new PerformanceObserver(i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
  });
  const observer = new PerformanceObserver(common.mustNotCall());

  [1, 'test'].forEach((input) => {
    assert.throws(
      () => observer.observe(input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "options" argument must be of type object.' +
                 common.invalidArgTypeHelper(input)
      });
  });

  [1, null, {}, Infinity].forEach((i) => {
    assert.throws(() => observer.observe({ entryTypes: i }),
                  {
                    code: 'ERR_INVALID_ARG_TYPE',
                    name: 'TypeError'
                  });
  });

  const obs = new PerformanceObserver(common.mustNotCall());
  obs.observe({ entryTypes: ['mark', 'mark'] });
  obs.disconnect();
  performance.mark('42');
}

// Test Non-Buffered
{
  const observer =
    new PerformanceObserver(common.mustCall(callback));

  function callback(list, obs) {
    assert.strictEqual(obs, observer);
    const entries = list.getEntries();
    assert.strictEqual(entries.length, 3);
    observer.disconnect();
  }
  observer.observe({ entryTypes: ['mark', 'node'] });
  performance.mark('test1');
  performance.mark('test2');
  performance.mark('test3');
}
