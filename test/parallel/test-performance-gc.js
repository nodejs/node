// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  PerformanceObserver,
  constants
} = require('perf_hooks');

const {
  NODE_PERFORMANCE_GC_MAJOR,
  NODE_PERFORMANCE_GC_MINOR,
  NODE_PERFORMANCE_GC_INCREMENTAL,
  NODE_PERFORMANCE_GC_WEAKCB
} = constants;

const kinds = [
  NODE_PERFORMANCE_GC_MAJOR,
  NODE_PERFORMANCE_GC_MINOR,
  NODE_PERFORMANCE_GC_INCREMENTAL,
  NODE_PERFORMANCE_GC_WEAKCB
];

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
    obs.disconnect();
  }));
  obs.observe({ entryTypes: ['gc'] });
  global.gc();
  // Keep the event loop alive to witness the GC async callback happen.
  setImmediate(() => setImmediate(() => 0));
}

// GC should not keep the event loop alive
{
  let didCall = false;
  process.on('beforeExit', () => {
    assert(!didCall);
    didCall = true;
    global.gc();
  });
}
