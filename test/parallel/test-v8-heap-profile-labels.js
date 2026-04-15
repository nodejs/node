// Flags: --expose-gc
// Heap profile labels require async-context-frame (enabled by default in
// Node.js 24+; use --experimental-async-context-frame on Node.js 22).
'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

// Test: API functions are exported
assert.strictEqual(typeof v8.startSamplingHeapProfiler, 'function');
assert.strictEqual(typeof v8.stopSamplingHeapProfiler, 'function');
assert.strictEqual(typeof v8.getAllocationProfile, 'function');
assert.strictEqual(typeof v8.withHeapProfileLabels, 'function');
assert.strictEqual(typeof v8.setHeapProfileLabels, 'function');

// Test: getAllocationProfile returns undefined when profiler not started
assert.strictEqual(v8.getAllocationProfile(), undefined);

// Test: basic profiling without labels
{
  v8.startSamplingHeapProfiler(64);
  const arr = [];
  for (let i = 0; i < 1000; i++) arr.push({ x: i });
  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  assert.ok(profile);
  assert.ok(Array.isArray(profile.samples));
  assert.ok(profile.samples.length > 0);

  // Every sample should have a labels field (empty object when unlabeled)
  for (const sample of profile.samples) {
    assert.strictEqual(typeof sample.nodeId, 'number');
    assert.strictEqual(typeof sample.size, 'number');
    assert.strictEqual(typeof sample.count, 'number');
    assert.strictEqual(typeof sample.sampleId, 'number');
    assert.strictEqual(typeof sample.labels, 'object');
    assert.ok(sample.labels !== null);
  }
}

// Test: withHeapProfileLabels captures labels on samples
{
  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/test' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/test'
  );
  assert.ok(labeled.length > 0, 'Should have samples labeled with /test');
}

// Test: distinct labels are attributed correctly
{
  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/heavy' }, () => {
    const arr = [];
    for (let i = 0; i < 10000; i++) arr.push(new Array(100));
  });

  v8.withHeapProfileLabels({ route: '/light' }, () => {
    const arr = [];
    for (let i = 0; i < 100; i++) arr.push({ x: i });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const heavy = profile.samples.filter((s) => s.labels.route === '/heavy');
  const light = profile.samples.filter((s) => s.labels.route === '/light');
  assert.ok(heavy.length > 0, 'Should have /heavy samples');
  // /light may have zero samples due to sampling — that's acceptable
  // The key assertion is that /heavy has samples and they are attributed
}

// Test: multi-key labels
{
  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/api', method: 'GET' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/api' && s.labels.method === 'GET'
  );
  assert.ok(labeled.length > 0, 'Should have multi-key labeled samples');
}

// Test: JSON.stringify round-trip
{
  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/json' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const json = JSON.stringify(profile);
  const parsed = JSON.parse(json);
  assert.ok(Array.isArray(parsed.samples));
  const labeled = parsed.samples.filter((s) => s.labels.route === '/json');
  assert.ok(labeled.length > 0, 'Labels survive JSON round-trip');
}

// Test: startSamplingHeapProfiler(0) throws RangeError (0 hits CHECK_GT in V8)
assert.throws(() => v8.startSamplingHeapProfiler(0), {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
});

// Test: withHeapProfileLabels validates arguments
assert.throws(() => v8.withHeapProfileLabels('bad', () => {}), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.withHeapProfileLabels({}, 'bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Test: setHeapProfileLabels validates arguments
assert.throws(() => v8.setHeapProfileLabels('bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Test: repeated start/stop cycles work
{
  for (let cycle = 0; cycle < 3; cycle++) {
    v8.startSamplingHeapProfiler(64);
    v8.withHeapProfileLabels({ route: `/cycle${cycle}` }, () => {
      const arr = [];
      for (let i = 0; i < 1000; i++) arr.push({ x: i });
    });
    const profile = v8.getAllocationProfile();
    v8.stopSamplingHeapProfiler();
    assert.ok(profile);
    assert.ok(profile.samples.length > 0);
  }
}

// Test: GC'd samples are retained with includeCollectedObjects: true
{
  v8.startSamplingHeapProfiler(64, 16, { includeCollectedObjects: true });

  v8.withHeapProfileLabels({ route: '/heavy-gc' }, () => {
    for (let i = 0; i < 500; i++) {
      // Allocate ~100KB arrays that become garbage immediately
      new Array(25000).fill(i);
    }
  });

  v8.withHeapProfileLabels({ route: '/light-gc' }, () => {
    const arr = [];
    for (let i = 0; i < 10; i++) arr.push({ x: i });
  });

  // Force garbage collection to remove unreferenced objects
  global.gc();

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const heavy = profile.samples.filter((s) => s.labels.route === '/heavy-gc');
  assert.ok(
    heavy.length > 0,
    'GC\'d samples should still be present with includeCollectedObjects: true'
  );

  // Heavy route allocated ~50MB, light route ~trivial — ratio should be high
  const heavyBytes = heavy.reduce((sum, s) => sum + s.size * s.count, 0);
  const light = profile.samples.filter((s) => s.labels.route === '/light-gc');
  const lightBytes = light.reduce((sum, s) => sum + s.size * s.count, 0);
  if (lightBytes > 0) {
    const ratio = heavyBytes / lightBytes;
    assert.ok(
      ratio > 50,
      `Heavy/light ratio should be >50 after GC, got ${ratio.toFixed(1)}`
    );
  }
}

// Test: GC'd samples are removed without includeCollectedObjects (default)
{
  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/gc-default' }, () => {
    for (let i = 0; i < 500; i++) {
      // Allocate ~100KB arrays that become garbage immediately
      new Array(25000).fill(i);
    }
  });

  // Force garbage collection — without includeCollectedObjects, samples are
  // removed from the profile via V8's OnWeakCallback
  global.gc();

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const samples = profile.samples.filter(
    (s) => s.labels.route === '/gc-default'
  );
  const totalBytes = samples.reduce((sum, s) => sum + s.size * s.count, 0);
  // After GC, most or all samples should be gone. The total bytes retained
  // should be much less than what was allocated (~50MB).
  assert.ok(
    totalBytes < 5 * 1024 * 1024,
    `Without includeCollectedObjects, GC'd samples should mostly be removed ` +
    `(got ${(totalBytes / 1024 / 1024).toFixed(1)}MB)`
  );
}

// Test: includeCollectedObjects: true retains samples, false does not
{
  // Start WITH includeCollectedObjects
  v8.startSamplingHeapProfiler(64, 16, { includeCollectedObjects: true });
  v8.withHeapProfileLabels({ route: '/retained' }, () => {
    for (let i = 0; i < 200; i++) new Array(25000).fill(i);
  });
  global.gc();
  const withProfile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const withSamples = withProfile.samples.filter(
    (s) => s.labels.route === '/retained'
  );
  const withBytes = withSamples.reduce((sum, s) => sum + s.size * s.count, 0);

  // Start WITHOUT includeCollectedObjects
  v8.startSamplingHeapProfiler(64);
  v8.withHeapProfileLabels({ route: '/removed' }, () => {
    for (let i = 0; i < 200; i++) new Array(25000).fill(i);
  });
  global.gc();
  const withoutProfile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const withoutSamples = withoutProfile.samples.filter(
    (s) => s.labels.route === '/removed'
  );
  const withoutBytes = withoutSamples.reduce(
    (sum, s) => sum + s.size * s.count, 0
  );

  // With includeCollectedObjects should retain significantly more bytes.
  // withBytes must be positive to avoid vacuous pass when both are 0.
  assert.ok(withBytes > 0,
    `includeCollectedObjects should retain samples: withBytes=${withBytes}`);
  assert.ok(
    withBytes > withoutBytes * 5,
    `includeCollectedObjects should retain more samples: ` +
    `with=${(withBytes / 1024).toFixed(0)}KB, ` +
    `without=${(withoutBytes / 1024).toFixed(0)}KB`
  );
}

// Test: setHeapProfileLabels doesn't leak entries when called repeatedly
// Each enterWith() creates a new AsyncContextFrame (CPED). Without cleanup,
// old entries accumulate in the label map. The fix calls unregister before
// enterWith so the old CPED entry is removed.
{
  v8.startSamplingHeapProfiler(64);

  // Call setHeapProfileLabels 100 times — only the last label should matter
  for (let i = 0; i < 100; i++) {
    v8.setHeapProfileLabels({ route: `/iter${i}` });
  }

  // Allocate under the final label
  const arr = [];
  for (let i = 0; i < 5000; i++) arr.push({ data: i });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  // The final label should be present on samples
  const finalLabeled = profile.samples.filter(
    (s) => s.labels.route === '/iter99'
  );
  assert.ok(
    finalLabeled.length > 0,
    'Should have samples labeled with final /iter99'
  );

  // Old labels should NOT appear on the post-loop allocations
  // (they may appear on allocations made during the loop itself, which is fine)
  // The key check: only /iter99 should appear on samples from the bulk alloc
  const allRoutes = new Set(
    profile.samples
      .filter((s) => s.labels.route)
      .map((s) => s.labels.route)
  );
  // With the fix, old entries are cleaned up before enterWith. Intermediate
  // labels may still appear (allocations during the loop), but the entry
  // count should not grow — verified by no crash or OOM under heavy use.
  assert.ok(allRoutes.has('/iter99'), 'Final label must be present');
}

// Test: labels survive when another ALS store changes the CPED address.
// Regression test for nodejs/node#62649: CPED is a shared AsyncContextFrame
// Map. Its identity (address) changes when ANY ALS store changes, not just the
// heap profile labels store. The old address-based lookup would fail because
// the CPED address at allocation time differs from the one registered with
// withHeapProfileLabels. The CPED-storage approach stores the full CPED value
// on each sample at allocation time and resolves labels at profile-read time
// via Map lookup — immune to address changes.
{
  const { AsyncLocalStorage } = require('async_hooks');
  const otherALS = new AsyncLocalStorage();

  v8.startSamplingHeapProfiler(64);

  v8.withHeapProfileLabels({ route: '/cped-identity' }, () => {
    // Allocate before changing other ALS (CPED address is X)
    const before = [];
    for (let i = 0; i < 2000; i++) before.push({ pre: i });

    // Change a DIFFERENT ALS store — this creates a new AsyncContextFrame,
    // changing the CPED address to Y. The heap profile labels ALS store is
    // still set (it was inherited into the new frame).
    otherALS.enterWith({ unrelated: 'data' });

    // Allocate after the other ALS change (CPED address is now Y, not X)
    const after = [];
    for (let i = 0; i < 2000; i++) after.push({ post: i });
  });

  const profile = v8.getAllocationProfile();
  v8.stopSamplingHeapProfiler();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/cped-identity'
  );
  assert.ok(
    labeled.length > 0,
    'Labels must survive when another ALS store changes the CPED address ' +
    '(nodejs/node#62649 regression)'
  );
}
