// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  performance,
  PerformanceObserver
} = require('perf_hooks');

const {
  NODE_PERFORMANCE_GC_MAJOR,
  NODE_PERFORMANCE_GC_MINOR,
  NODE_PERFORMANCE_GC_INCREMENTAL,
  NODE_PERFORMANCE_GC_WEAKCB
} = process.binding('performance').constants;

const kinds = [
  NODE_PERFORMANCE_GC_MAJOR,
  NODE_PERFORMANCE_GC_MINOR,
  NODE_PERFORMANCE_GC_INCREMENTAL,
  NODE_PERFORMANCE_GC_WEAKCB
];

// No observers for GC events, no entries should appear
{
  global.gc();
  const entries = performance.getEntriesByType('gc');
  assert.strictEqual(entries.length, 0);
}

// Adding an observer should force at least one gc to appear
{
  const obs = new PerformanceObserver(common.mustCallAtLeast((list) => {
    const entry = list.getEntries()[0];
    assert(entry);
    assert.strictEqual(entry.name, 'gc');
    assert.strictEqual(entry.entryType, 'gc');
    assert(kinds.includes(entry.kind));
    assert.strictEqual(typeof entry.startTime, 'number');
    assert.strictEqual(typeof entry.duration, 'number');

    performance.clearGC();

    const entries = performance.getEntriesByType('gc');
    assert.strictEqual(entries.length, 0);
    obs.disconnect();
  }));
  obs.observe({ entryTypes: ['gc'] });
  global.gc();
}
