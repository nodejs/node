// Flags: --expose-internals
'use strict';

// Tests that the observer counts, which gate the creation of performance
// entries, return to zero once observers disconnect, however the entry types
// were observed.

require('../common');
const assert = require('node:assert');
const { PerformanceObserver } = require('node:perf_hooks');
const { internalBinding } = require('internal/test/binding');
const { hasObserver } = require('internal/perf/observe');

const {
  observerCounts,
  constants: {
    NODE_PERFORMANCE_ENTRY_TYPE_GC,
    NODE_PERFORMANCE_ENTRY_TYPE_HTTP,
    NODE_PERFORMANCE_ENTRY_TYPE_DNS,
  },
} = internalBinding('performance');

const kTypes = {
  gc: NODE_PERFORMANCE_ENTRY_TYPE_GC,
  http: NODE_PERFORMANCE_ENTRY_TYPE_HTTP,
  dns: NODE_PERFORMANCE_ENTRY_TYPE_DNS,
};

function assertCounts(expected) {
  for (const { 0: type, 1: index } of Object.entries(kTypes)) {
    const count = expected[type] ?? 0;
    assert.strictEqual(observerCounts[index], count,
                       `observer count for '${type}'`);
    assert.strictEqual(hasObserver(type), count > 0, `hasObserver('${type}')`);
  }
}

assertCounts({});

{
  // Observing the same type more than once counts it once.
  const obs = new PerformanceObserver(() => {});
  for (const type of ['gc', 'http', 'dns']) {
    obs.observe({ type });
    obs.observe({ type });
  }
  assertCounts({ gc: 1, http: 1, dns: 1 });
  obs.disconnect();
  assertCounts({});
  // Disconnecting again must not decrement the counts any further.
  obs.disconnect();
  assertCounts({});
}

{
  // Duplicate entry types are counted once.
  const obs = new PerformanceObserver(() => {});
  obs.observe({ entryTypes: ['http', 'http', 'gc', 'gc'] });
  assertCounts({ gc: 1, http: 1 });
  obs.disconnect();
  assertCounts({});
}

{
  // Replacing the observed entry types updates the counts.
  const obs = new PerformanceObserver(() => {});
  obs.observe({ entryTypes: ['gc', 'http'] });
  assertCounts({ gc: 1, http: 1 });
  obs.observe({ entryTypes: ['http'] });
  assertCounts({ http: 1 });
  obs.disconnect();
  assertCounts({});
}

{
  // Each observer is counted separately.
  const obs1 = new PerformanceObserver(() => {});
  const obs2 = new PerformanceObserver(() => {});
  obs1.observe({ type: 'gc' });
  obs2.observe({ type: 'gc' });
  obs2.observe({ type: 'gc' });
  assertCounts({ gc: 2 });
  obs1.disconnect();
  assertCounts({ gc: 1 });
  obs2.disconnect();
  assertCounts({});
}
