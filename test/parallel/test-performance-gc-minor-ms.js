// Flags: --expose-gc --no-warnings --minor-ms
'use strict';

// When V8's Minor Mark-Sweep collector is enabled (--minor-ms), minor garbage
// collections are reported with kind NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP
// rather than NODE_PERFORMANCE_GC_MINOR.

const common = require('../common');
const assert = require('assert');
const {
  PerformanceObserver,
  constants
} = require('perf_hooks');

const {
  NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP,
  NODE_PERFORMANCE_GC_FLAGS_FORCED
} = constants;

const obs = new PerformanceObserver(common.mustCallAtLeast((list) => {
  const entry = list.getEntries()[0];
  assert(entry);
  assert.strictEqual(entry.name, 'gc');
  assert.strictEqual(entry.entryType, 'gc');
  assert.strictEqual(entry.detail.kind, NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP);
  assert.strictEqual(entry.detail.flags, NODE_PERFORMANCE_GC_FLAGS_FORCED);
  obs.disconnect();
}));
obs.observe({ entryTypes: ['gc'] });

globalThis.gc({ type: 'minor' });
// Keep the event loop alive to witness the GC async callback happen.
setImmediate(() => setImmediate(() => 0));
