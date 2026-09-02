// Flags: --expose-gc
// Verify that label storage is deduplicated and that samples retained after
// collection keep their labels until profiler teardown.
'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

const VOCAB = 10;
const N = 2000;

function runWorkload(iterations) {
  for (let i = 0; i < iterations; i++) {
    // Cycle over a small vocabulary of routes so the intern table sees
    // VOCAB distinct ALS values regardless of N.
    const route = `r-${i % VOCAB}`;
    v8.withHeapProfileLabels({ route }, () => {
      // Allocate a small object that immediately dies.
      const _dead = { i, payload: 'x'.repeat(8) };
      return _dead.i;
    });
    // Drive weak callbacks periodically so retained samples accumulate
    // throughout the loop, not only at the end.
    if ((i + 1) % 100 === 0) {
      global.gc();
    }
  }
  global.gc();
  global.gc();
}

// Label-array storage should scale with VOCAB rather than N. Sample structs
// still scale with the number of retained samples.
{
  global.gc();
  global.gc();
  const baseHeap = process.memoryUsage().heapUsed;
  const handle = v8.startHeapProfile({
    sampleInterval: 64,
    stackDepth: 16,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
    labels: true,
  });
  try {
    runWorkload(N);
    const delta = process.memoryUsage().heapUsed - baseHeap;
    assert.ok(
      delta < 5 * 1024 * 1024,
      `Heap delta after ${N} iterations across ${VOCAB} routes is ` +
      `${(delta / 1024 / 1024).toFixed(2)} MB (limit 5 MB). The intern ` +
      `table should keep ALS-array footprint bounded by VOCAB, not N.`
    );
  } finally {
    handle.stop();
  }
}

// Collected samples retained by the profiler must keep their labels.
{
  global.gc();
  global.gc();
  const handle = v8.startHeapProfile({
    sampleInterval: 64,
    stackDepth: 16,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
    labels: true,
  });
  try {
    for (let i = 0; i < 1000; i++) {
      const route = `r-${i % VOCAB}`;
      v8.withHeapProfileLabels({ route }, () => {
        const _dead = { i, payload: 'x'.repeat(8) };
        return _dead.i;
      });
      if ((i + 1) % 100 === 0) {
        global.gc();
      }
    }
    global.gc();
    global.gc();

    const profile = handle.getAllocationProfile();
    assert.ok(profile);
    assert.ok(Array.isArray(profile.samples));
    assert.ok(profile.samples.length > 0,
              'Profiler should retain samples with includeObjectsCollected*');

    // Filter to samples whose label.route is one we set. Samples
    // attributed to internal V8 allocations made outside the
    // withHeapProfileLabels block legitimately have empty labels
    // (no ALS frame at allocation time) — those are not the
    // population we care about here.
    const ourSamples = profile.samples.filter((s) => {
      if (!s.labels) return false;
      const r = s.labels.route;
      if (typeof r !== 'string') return false;
      return /^r-\d+$/.test(r);
    });

    // Require enough labelled samples to show attribution survives the
    // periodic collections throughout the workload.
    assert.ok(
      ourSamples.length >= 1000,
      `Expected >=1000 retained samples carrying our route labels, ` +
      `got ${ourSamples.length} ` +
      `(of ${profile.samples.length} total samples)`
    );

    // Vocabulary check: all VOCAB routes should appear among the
    // retained samples (the intern table preserves attribution for
    // dead-retained samples, so we should see all 10 routes).
    const seenRoutes = new Set(ourSamples.map((s) => s.labels.route));
    assert.strictEqual(
      seenRoutes.size, VOCAB,
      `Expected all ${VOCAB} routes to appear in retained labels, ` +
      `got ${seenRoutes.size}: ${[...seenRoutes].sort().join(',')}`
    );
  } finally {
    handle.stop();
  }
}
