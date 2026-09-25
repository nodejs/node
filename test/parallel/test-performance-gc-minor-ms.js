// Flags: --expose-gc --no-warnings --minor-ms
'use strict';

// When V8's Minor Mark-Sweep collector is enabled (--minor-ms), minor garbage
// collections are reported with kind NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP
// rather than NODE_PERFORMANCE_GC_MINOR.

const common = require('../common');
const assert = require('assert');
const { gcUntil } = require('../common/gc');
const {
  PerformanceObserver,
  constants
} = require('perf_hooks');

const {
  NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP
} = constants;

let observed = false;
const obs = new PerformanceObserver(common.mustCallAtLeast((list) => {
  for (const entry of list.getEntries()) {
    // Other GC kinds (e.g. a scheduled or incremental GC) may be observed in
    // between; only the minor mark-sweep entry is relevant here.
    if (entry.detail.kind === NODE_PERFORMANCE_GC_MINOR_MARK_SWEEP) {
      assert.strictEqual(entry.name, 'gc');
      assert.strictEqual(entry.entryType, 'gc');
      observed = true;
      obs.disconnect();
      return;
    }
  }
}));
obs.observe({ entryTypes: ['gc'] });

gcUntil('minor gc event', () => observed, 10, { type: 'minor' });
